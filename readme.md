# simdfix

A SIMD-accelerated [FIX](https://www.fixtrading.org/standards/fix-sessions-online/) (Financial Information Exchange) protocol parser and encoder in C++23. It uses 16-byte parallel SIMD operations (ARM NEON / x86 SSE4.1) and SWAR digit parsing to tokenize raw FIX messages with zero copies.

## Features

- **Header-only library** — no compiled translation units; add it as a CMake `INTERFACE` dependency.
- **SIMD tokenization** — processes 16 bytes per cycle to detect tag delimiters (`=`) and field separators (`0x01`).
- **Zero-copy parsing** — the decoder produces a flat token array of positions, tags, and lengths without copying message data.
- **Encode and decode** — typed field, group, and data (raw binary) accessors for both reading and writing FIX messages.
- **Code generation** — message decoders, encoders, and grammars are generated from `protocol.xml` via the included `Generator` tool.
- **No exceptions in the hot path** — fallible operations return `std::expected<T, Result::Values>`.

## Requirements

- C++23 compiler (Clang 16+ or GCC 13+)
- CMake 3.20+
- ARM (NEON) or x86-64 (SSE4.1) target
- [Google Test](https://github.com/google/googletest) (for tests)
- [pugixml](https://pugixml.org/) (for the code generator)

## Building

```bash
# Configure and build (Debug — includes AddressSanitizer and coverage instrumentation)
cmake -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build cmake-build-debug

# Configure and build (Release — O3, LTO, march=native)
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

# Filter to a specific test case
./cmake-build-debug/MessageDecoderTest --gtest_filter="MessageDecoder.Logon"
```

## Benchmarks

Always use a Release build — Debug builds include AddressSanitizer and coverage overhead that skews numbers.

```bash
./cmake-build-release/SimdFixBenchmark            # run all benchmarks
./cmake-build-release/SimdFixBenchmark logon-hot   # run a specific benchmark
```

## Code Coverage

```bash
cmake --build cmake-build-debug --target Coverage
```

This runs all test binaries, merges their `profraw` files, and prints an `llvm-cov` summary report.

## Code Generation

Message types (decoders, encoders, handler dispatch, grammar) are generated from `src/generator/resources/protocol.xml`. To regenerate after changing the protocol spec:

```bash
cmake --build cmake-build-debug --target GenerateMessages
```

Do not hand-edit files under `src/main/cpp/org/limitless/fix/messages/` — they are overwritten by the generator.

## Architecture

```
Raw FIX bytes
    -> PayloadDecoder::parse()    # SIMD tokenization; fills Token[] array
    -> MessageHandler::handle()   # Dispatch by MsgType tag
    -> MessageDecoder<Protocol>   # Typed field access via LinearSearch
    -> Application handler
```

| Component | Purpose |
|---|---|
| `PayloadDecoder` | Core SIMD engine — detects `=` and `0x01` in 16-byte chunks to build the token array. |
| `Uint8x16` | Thin SIMD wrapper — selects ARM NEON or x86 SSE4.1 backend at compile time. |
| `MessageDecoder` | Templated field extractor — uses `LinearSearch` over the token array to find tags. |
| `FieldDecoder` | Typed accessors for individual FIX fields (int, string, decimal, etc.). |
| `GroupDecoder` | Repeating-group iteration over the token array. |
| `DataDecoder` | Raw binary (data+length) field pairs. |
| `MessageEncoder` | Builds outbound FIX messages with checksum and body-length computation. |
| `Grammar` | Compile-time protocol metadata (`constexpr` tag numbers, types, mandatory flags). |
| `Generator` | Reads `protocol.xml` and emits decoder, encoder, handler, and grammar headers. |

## License

See [LICENSE](LICENSE) for details.
