<p align="center"><img src="doc/simdfix.png" width="600" alt="simdfix"/></p>

A SIMD-accelerated [FIX](https://www.fixtrading.org/standards/fix-sessions-online/) (Financial Information Exchange) protocol codec in C++23, targeting ARM NEON. Decodes and encodes FIX messages using 16-byte parallel NEON operations and SWAR (SIMD Within A Register) digit parsing with zero copies.

## Features

- **Header-only library** — add it as a CMake `INTERFACE` dependency.
- **SIMD tokenization** — processes 16 bytes per cycle to detect tag delimiters (`=`) and field separators (`0x01`).
- **Zero-copy parsing** — the decoder produces a flat `Field[]` array of positions, tags, and lengths without copying message data.
- **Encode and decode** — typed field, group, and data (raw binary) accessors for both reading and writing FIX messages.
- **Code generation** — message decoders, encoders, and handler dispatch are generated from a session spec (`session.xml`) and an optional application spec (`protocol.xml`) via the included `Generator` tool. Session-layer messages are always generated; application messages are merged in when the application spec is present.
- **No exceptions in the hot path** — fallible operations return `std::expected<T, Result>`.

## Requirements

- C++20 or C++23 compiler (Clang 16+ or GCC 13+)
- CMake 3.20+
- ARM (NEON) and INTEL (SSE) targets
- [Google Test](https://github.com/google/googletest) (for tests)
- [pugixml](https://pugixml.org/) (for the code generator)

## Building

```bash
# Debug build (includes AddressSanitizer + coverage)
cmake -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build cmake-build-debug

# Release build (O3, LTO, march=native)
cmake -B cmake-build-release -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release

# Build a single target
cmake --build cmake-build-debug --target PayloadDecoderTest
```

## Running Tests

```bash
# Run all tests via CTest
cd cmake-build-debug && ctest --output-on-failure

# Run a single test binary
./cmake-build-debug/MessageDecoderTest

# Filter to specific tests
./cmake-build-debug/MessageDecoderTest --gtest_filter="MessageDecoder.Logon"
./cmake-build-debug/PayloadDecoderTest --gtest_filter="PayloadDecoder.TrailerSplitCheckSum"
./cmake-build-debug/FieldDecoderTest --gtest_filter="FieldDecoder.GetFixedDecimal"
```

## Benchmarks

Always use a Release build — Debug builds include AddressSanitizer and coverage overhead that skews numbers.

```bash
./cmake-build-release/SimdFixBenchmark            # run all benchmarks
./cmake-build-release/SimdFixBenchmark logon-hot   # run a specific benchmark
```

Available benchmarks: `logon-cold`, `logon-hot`, `logon-getters`, `logon-groups`, `logon-data`, `logon-encode`, `nos-hot`, `nos-getters`, `nos-encode`, `er-hot`, `er-getters`, `er-encode`, or `all` (default).

### Results (Apple M4, Release build)

| Benchmark | Message Size | Throughput | Latency |
|-----------|-------------|-----------|---------|
| Logon cold | 142 B | 1.47 GB/s | 96.5 ns/msg |
| Logon decode | 142 B | 1.50 GB/s | 94.8 ns/msg |
| Logon getters | 142 B | 1.18 GB/s | 120.5 ns/msg |
| Logon groups | 142 B | 1.22 GB/s | 208.0 ns/msg |
| Logon data | 166 B | 1.08 GB/s | 153.1 ns/msg |
| Logon encode | 142 B | 1.94 GB/s | 115.3 ns/msg |
| NewOrderSingle decode | 154 B | 0.64 GB/s | 240.4 ns/msg |
| NewOrderSingle getters | 154 B | 0.51 GB/s | 301.2 ns/msg |
| NewOrderSingle encode | 154 B | 4.14 GB/s | 39.3 ns/msg |
| ExecutionReport decode | 245 B | 1.42 GB/s | 173.1 ns/msg |
| ExecutionReport getters | 245 B | 1.00 GB/s | 245.9 ns/msg |
| ExecutionReport encode | 245 B | 2.39 GB/s | 102.5 ns/msg |

## Code Coverage

```bash
cmake --build cmake-build-debug --target Coverage
```

This runs all test binaries, merges their `profraw` files, and prints an `llvm-cov` summary report.

## Code Generation

Generation is driven by three XML files and produces six headers under `<build>/org/limitless/fix/generated/`.

| File | Role | Required |
|------|------|----------|
| `src/generator/resources/session.xml` | Session-layer messages (Logon, Logout, Heartbeat, TestRequest, ResendRequest, Reject, SequenceReset) and their enums | Always |
| `src/generator/resources/protocol.xml` | Application-layer messages (e.g. NewOrderSingle, ExecutionReport) and their enums | Optional |
| `src/generator/resources/config.xml` | Engine identity, buffer sizes, timing, session topology | Optional |

When both `session.xml` and `protocol.xml` are present the generator merges their data models before emitting code. Shared enums — in particular `MessageType` — are merged by value: entries from `session.xml` come first, then any new values from the application spec are appended. Duplicate values are silently dropped.

The generator CLI reflects this split:

```
Generator <session.xml> <output-dir> <config.xml> <config-output-dir> [<application.xml>]
```

To regenerate after changing any XML file:

```bash
cmake --build cmake-build-debug --target GenerateMessages
```

Generated headers are never checked into the repository and must not be hand-edited — they are overwritten on every build that touches the generator or its input XML files.

## License

See [LICENSE](LICENSE) for details.
