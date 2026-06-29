# simdfix

A SIMD-accelerated [FIX](https://www.fixtrading.org/standards/fix-sessions-online/) (Financial Information Exchange) protocol codec in C++23, targeting ARM NEON. Decodes and encodes FIX messages using 16-byte parallel NEON operations and SWAR (SIMD Within A Register) digit parsing with zero copies.

## Features

- **Header-only library** — add it as a CMake `INTERFACE` dependency (`Session.cpp` is the only compiled translation unit).
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
| Logon decode | 142 B | 1.54 GB/s | 92 ns/msg |
| Logon getters | 142 B | 1.21 GB/s | 118 ns/msg |
| Logon encode | 142 B | 2.19 GB/s | 103 ns/msg |
| NewOrderSingle decode | 154 B | 1.62 GB/s | 95 ns/msg |
| NewOrderSingle getters | 154 B | 1.02 GB/s | 151 ns/msg |
| NewOrderSingle encode | 154 B | 4.34 GB/s | 38 ns/msg |
| ExecutionReport decode | 245 B | 1.68 GB/s | 146 ns/msg |
| ExecutionReport getters | 245 B | 1.04 GB/s | 236 ns/msg |
| ExecutionReport encode | 245 B | 2.42 GB/s | 101 ns/msg |

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
| `src/generator/resources/config.xml` | Engine identity, buffer sizes, timing, session topology | Always |

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

## Architecture

### Key Components

#### Decoding

**`PayloadDecoder.hpp`** — Core tokenization engine. Processes 16 bytes/cycle with NEON to detect `=` (tag end) and `0x01` (field end). Returns a flat `Field[]` array (position + tag + length, no copies) and a `Result` status. Handles fragmented messages and split tags across chunk boundaries. Validates checksum, body length, and begin string.

**`FieldDecoder.hpp`** (`detail/decoder/`) — Field-level access to a decoded message. Provides typed field accessors (`getUint32`, `getInt32`, `getFixedDecimal`, `getTimestamp`, `getEnum`, `getString`) with SWAR-accelerated numeric parsing. Manages repeating-group scope tracking. Memoizes date-to-epoch conversion across fields sharing the same date.

**`MessageDecoder.hpp`** — Base for generated message decoders. Wraps a `FieldDecoder` and extracts/validates standard header fields (SenderCompID, TargetCompID, MsgSeqNum, SendingTime) against an optional `SessionContext`.

**`GroupDecoder.hpp`** — Repeating-group iterator. Scans for group entries by their delimiter tag, pushes/pops `FieldDecoder` scopes so field lookups are restricted to the current group entry.

**`DataDecoder.hpp`** — Handles raw-data (Length+Data tag pair) fields, e.g. XmlData.

#### Encoding

**`PayloadEncoder.hpp`** — Serializes encoded fields into a byte buffer. Writes the begin string, body length, and checksum framing. Uses SWAR for integer-to-ASCII conversion.

**`FieldEncoder.hpp`** (`detail/encoder/`) — Field-level encoding. Converts typed values (integers, decimals, timestamps, enums, strings) to their FIX wire representation.

**`MessageEncoder.hpp`** — Base for generated message encoders. Provides the `wrap`/`encodedLength`/`type` interface that `PayloadEncoder` uses to frame a complete message.

**`GroupEncoder.hpp`** — Encodes repeating groups: manages the group-count tag and per-entry field serialization.

#### Shared

**`Uint8x16.hpp`** (`detail/simd/`) — Thin NEON wrapper (`uint8x16_t`). All SIMD operations go through here. If porting off ARM, this is the only layer to replace.

**`LinearSearch.hpp`** (`detail/simd/`) — NEON-accelerated tag lookup in the `uint16_t` tag array. Processes 8 tags per cycle.

**`Conversions.hpp`** (`utils/`) — SWAR digit parsing (`asciiToUint64`), digit validation (`isDigits`), timestamp parsing/formatting, integer-to-ASCII conversion, enum lookup tables, and `FixedDecimal` encoding/decoding helpers.

**`FixedDecimal.hpp`** (`utils/`) — Fixed-point decimal type (8 implicit decimal places, `int64_t` mantissa). Supports arithmetic via reciprocal multiplication (no hardware division). Used for FIX price/quantity fields.

**`FixTypes.hpp`** (`generated/messages/`) — Generated enum wrappers and type metadata for the protocol's field types (e.g., `OrdType`, `Side`, `ExecType`).

#### Generated Code

All generated headers live under `<build>/org/limitless/fix/generated/` and are excluded from the source tree. The build directory is on the include path for all consumers, so headers are included as `org/limitless/fix/generated/…`.

**`generated/messages/FixMessageDecoders.hpp`** / **`FixMessageEncoders.hpp`** / **`FixMessageHandler.hpp`** — Generated from `session.xml` (and optionally merged with the application spec). Contain per-message-type decoder/encoder structs and the `FixMessageHandler<T>` CRTP dispatch base (which provides default no-op `handle` overloads for every message type and a `receive<Message>()` method with a deferred constraint that catches wrong handler signatures at instantiation time). Do not edit manually.

**`generated/messages/FixTypes.hpp`** — Generated enum wrappers and type metadata (e.g., `OrdType`, `Side`, `ExecType`). The `MessageType` enum is the merged union of values from both specs.

**`generated/config/FixEngine.hpp`** / **`FixConfig.hpp`** — Generated from `config.xml`. Contain protocol version constants (`FIXT_1_1`) and engine-level configuration (session parameters). Do not edit manually.

**`Generator` (`FixGenerator.cpp`)** — Reads `session.xml`, `config.xml`, and an optional application spec and emits all six generated headers. Run via the `GenerateMessages` CMake target after changing any input XML file.

#### Session

**`Session.hpp`** / **`Session.cpp`** — FIX session state machine (Disconnected → Connecting → Connected → SentLogon → Active). Subclasses `MessageHandler` to handle session-level messages (Logon, Logout, Heartbeat, TestRequest, ResendRequest, SequenceReset, Reject).

### Design Constraints

- **Max 256 fields** per message — compile-time constant in `PayloadDecoder.hpp`.
- **ARM NEON only** — `Uint8x16` wraps `arm_neon.h` intrinsics directly.
- **No exceptions** — errors propagate as `std::expected<T, Result>`. (`std::invalid_argument` thrown from `GroupDecoder::wrap` on a missing tag is a known exception to this rule.)

### Adding a New Message Type

Session-layer messages (Logon, Heartbeat, etc.) belong in `session.xml`. Application messages (NewOrderSingle, ExecutionReport, etc.) belong in the application spec (`protocol.xml` in this repository). For each:

1. Add the enum values for the new message to the `MessageType` enum in the appropriate XML file — the generator merges `MessageType` from both specs automatically.
2. Add the message definition with its fields.
3. Run `cmake --build cmake-build-debug --target GenerateMessages` to regenerate all six headers.
4. Add a `handle(FooDecoder&)` override in the application handler.

## License

See [LICENSE](LICENSE) for details.
