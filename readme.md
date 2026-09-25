<p align="center"><img src="doc/simdfix.png" width="600" alt="simdfix"/></p>

<p align="center">
  <a href="https://github.com/FredrikJDahlberg/simdfix/actions/workflows/ci.yml"><img src="https://github.com/FredrikJDahlberg/simdfix/actions/workflows/ci.yml/badge.svg?branch=main" alt="tests"/></a>
  <a href="LICENSE"><img src="https://img.shields.io/github/license/FredrikJDahlberg/simdfix" alt="License"/></a>
  <img src="https://img.shields.io/badge/C%2B%2B-20%20%7C%2023-blue?logo=cplusplus" alt="C++20 | C++23"/>
  <img src="https://img.shields.io/badge/SIMD-NEON%20%7C%20SSE-orange" alt="SIMD: NEON | SSE"/>
  <img src="https://img.shields.io/badge/header--only-yes-brightgreen" alt="Header-only"/>
</p>

A SIMD-accelerated [FIX](https://www.fixtrading.org/standards/fix-sessions-online/) (Financial Information Exchange) protocol codec in C++20/23, targeting ARM NEON and x86 SSE. Decodes and encodes FIX messages using 16-byte parallel SIMD operations and SWAR (SIMD Within A Register) digit parsing with zero copies.

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
- [Google Test](https://github.com/google/googletest) (for tests; downloaded automatically)
- [pugixml](https://pugixml.org/) (for the code generator; built from source if not installed)

## Usage

### Adding simdfix to your project

The message decoders and encoders are generated at build time from the XML specs (see [Code Generation](#code-generation)), so a consumer depends on the `GenerateMessages` target or installs simdfix after a build.

As a subdirectory (e.g. a git submodule). The tests and benchmarks are skipped when simdfix is not the top-level project, so only the generator and its pugixml dependency are built:

```cmake
add_subdirectory(external/simdfix EXCLUDE_FROM_ALL)

add_executable(app main.cpp)
target_link_libraries(app PRIVATE SimdFix::SimdFix)
add_dependencies(app GenerateMessages)
```

As an installed package:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DSIMDFIX_BUILD_TESTS=OFF
cmake --build build
cmake --install build --prefix /opt/simdfix
```

```cmake
find_package(SimdFix REQUIRED)   # configure with -DCMAKE_PREFIX_PATH=/opt/simdfix
target_link_libraries(app PRIVATE SimdFix::SimdFix)
```

Include the umbrella header `org/limitless/simdifx/Fix.hpp`. Headers under `detail/` are internal. `SimdFix::SimdFix` adds only its include paths, C++20, and `-msse4.1` on x86 to your target; your build type and flags are left alone. Define `NDEBUG` in production builds (CMake does this for `Release`): without it the decoder prints a trace of every block it parses.

### Encoding a message

```cpp
#include "org/limitless/simdifx/Fix.hpp"

using namespace org::limitless::simdifx;
using namespace org::limitless::simdifx::generated::messages;

std::array<uint8_t, 512> buffer{};
FixPayloadEncoder encoder{Protocol::FIXT_1_1, "BUYER", "SELLER"};
encoder.wrap(0, buffer);

const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
    std::chrono::system_clock::now().time_since_epoch());

NewOrderSingleEncoder order{};
encoder.wrapMessage(order)
    .sequenceNumber(1)
    .sendingTime(now)
    .clOrdID("ORDER1")
    .handlInst(HandlInst::AutoPrivate)
    .symbol("AAPL")
    .side(Side::Buy)
    .transactTime(now)
    .orderQty(100)
    .ordType(OrdType::Limit)
    .price(utils::FixedDecimal{15025, -2});   // 150.25

const auto length = encoder.encode(order);    // writes BodyLength (9) and CheckSum (10)
// buffer[0, length) holds the complete message
```

### Decoding messages

Derive a handler from the generated `FixMessageHandler` and add a `handle` overload for each message type you need. Every message is validated before dispatch, and types without an overload are skipped. Field accessors return `expected<T, Result>` and read straight from the input buffer, so strings come back as `std::string_view` with no copy.

```cpp
using namespace org::limitless::simdifx::decoder;

struct OrderHandler : FixMessageHandler<OrderHandler>
{
    using FixMessageHandler::handle;

    Result handle(NewOrderSingleDecoder& order)
    {
        const std::string_view symbol = order.symbol().value();
        const uint32_t quantity = order.orderQty().value();
        const double price = order.price().value_or(utils::FixedDecimal{}).toDouble();
        // ...
        return Result::Success;
    }
};

PayloadDecoder<Protocol::FIXT_1_1> decoder;
OrderHandler handler;

std::span<const uint8_t> input = received;    // bytes read from the socket
while (!input.empty())
{
    const auto [processed, status] = decoder.parse(input, handler);
    if (status == Result::MessageFragment)
    {
        break;                                // incomplete message: keep the bytes and read more
    }
    if (processed == 0)
    {
        break;                                // unrecoverable framing error (e.g. InvalidBeginString): close the session
    }
    if (status != Result::Success)
    {
        // the message was consumed but rejected; name(status) describes why
    }
    input = input.subspan(processed);
}
```

Repeating groups are read with `count()`, `next()` and `hasNext()` on the group accessor. See `src/test/cpp/org/limitless/simdifx/` for more examples.

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

Tests, benchmarks and the coverage/profiling targets are controlled by `SIMDFIX_BUILD_TESTS`, which defaults to `ON` when simdfix is the top-level project and `OFF` when it is added with `add_subdirectory` or FetchContent.

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

Generation is driven by three XML files and produces five headers under `<build>/org/limitless/simdifx/generated/`.

| File | Role | Required |
|------|------|----------|
| `src/generator/resources/session.xml` | Session-layer messages (Logon, Logout, Heartbeat, TestRequest, ResendRequest, Reject, SequenceReset) and their enums | Always |
| `src/generator/resources/protocol.xml` | Application-layer messages (e.g. NewOrderSingle, ExecutionReport) and their enums | Optional |
| `src/generator/resources/test.xml` | `protocol.xml` plus the extra groups, components and enums the tests exercise | Used by the in-tree build |
| `src/generator/resources/config.xml` | Engine identity, buffer sizes, timing, session topology | Optional |

When both `session.xml` and `protocol.xml` are present the generator merges their data models before emitting code. Shared enums — in particular `MessageType` — are merged by value: entries from `session.xml` come first, then any new values from the application spec are appended. Duplicate values are silently dropped.

The in-tree build passes `test.xml` as the application spec, so tests, benchmarks and the default install see its superset of messages. The spec is selected by `APP_XML` in `CMakeLists.txt`.

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
