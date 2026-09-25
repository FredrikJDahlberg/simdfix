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
- **Code generation** — message decoders, encoders, and handler dispatch are generated from a session spec (`session.xml`) and an optional application spec via the included `Generator` tool, driven from CMake by `simdfix_generate()`. Session-layer messages are always generated; application messages are merged in when the application spec is present. Each spec can be generated into its own namespace, so one program can use several.
- **No exceptions in the hot path** — fallible operations return `std::expected<T, Result>`.

## Requirements

- C++20 or C++23 compiler (Clang 16+ or GCC 13+)
- CMake 3.20+
- ARM (NEON) and INTEL (SSE) targets
- [Google Test](https://github.com/google/googletest) (for tests; downloaded automatically)
- [pugixml](https://pugixml.org/) (for the code generator; built from source if not installed)

## Usage

The message decoders and encoders are generated at build time from XML specs (see [Code Generation](#code-generation)). The `simdfix_generate()` CMake function runs the generator over your specs and makes a target to link; linking it brings in the library too, and your code is compiled only after the headers are generated.

```cmake
simdfix_generate(<name>
    [APP_XML <file>]        # your application messages and enums
    [SESSION_XML <file>]    # default: the session.xml shipped with simdfix
    [CONFIG_XML <file>]     # default: the config.xml shipped with simdfix
    [NAMESPACE <ns>]        # default: org::limitless::simdfix::generated
    [OUTPUT_DIR <dir>])     # default: ${CMAKE_CURRENT_BINARY_DIR}/<name>
```

Include `<namespace as a path>/messages/FixMessages.hpp`, e.g. `org/limitless/simdfix/generated/messages/FixMessages.hpp` for the default namespace. It pulls in the library (`org/limitless/simdfix/Fix.hpp`) and everything generated for that spec. Headers under `detail/` are internal.

### Adding simdfix to your project

With FetchContent:

```cmake
include(FetchContent)
FetchContent_Declare(
        simdfix
        GIT_REPOSITORY https://github.com/FredrikJDahlberg/simdfix.git
        GIT_TAG        main
        GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(simdfix)

simdfix_generate(OrderMessages APP_XML orders.xml NAMESPACE myapp::fix)

add_executable(app main.cpp)
target_link_libraries(app PRIVATE OrderMessages)
```

As a subdirectory (e.g. a git submodule), with the same `simdfix_generate()` and `target_link_libraries()` calls:

```cmake
add_subdirectory(external/simdfix EXCLUDE_FROM_ALL)
```

Either way the tests and benchmarks are skipped when simdfix is not the top-level project, so only the generator and its pugixml dependency are built.

As an installed package. The install holds the headers, the generator, the default specs (`session.xml`, `config.xml`, and `protocol.xml` with common application messages) and `simdfix_generate()`:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DSIMDFIX_BUILD_TESTS=OFF
cmake --build build
cmake --install build --prefix /opt/simdfix
```

```cmake
find_package(SimdFix REQUIRED)   # configure with -DCMAKE_PREFIX_PATH=/opt/simdfix
simdfix_generate(OrderMessages APP_XML ${SIMDFIX_RESOURCE_DIR}/protocol.xml)
target_link_libraries(app PRIVATE OrderMessages)
```

### Compiler flags

The library adds only its include paths, C++20, and `-msse4.1` on x86 to your target; your build type and flags are left alone.

- **x86:** SSE4.1 is the baseline, and the only instruction set simdfix needs. You don't need `-march=native`. The benchmarks use it, but that's this project's own choice for its own build. Add it only if you would anyway, knowing the binary then won't run on older CPUs.
- **ARM:** NEON is part of every AArch64 target, so no flag is needed.
- **Exceptions:** the headers compile with `-fno-exceptions`.
- **Tracing:** define `SIMDFIX_TRACE` to have the decoder print a trace of every block and token it parses. It is off by default in every build type.

### Encoding a message

The examples below use messages from `protocol.xml`, generated into the default namespace.

```cpp
#include "org/limitless/simdfix/generated/messages/FixMessages.hpp"

using namespace org::limitless::simdfix;
using namespace org::limitless::simdfix::generated::messages;

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

Derive a handler from the generated `FixMessageHandler` and add a `handle` overload for each message type you need. Every message is validated before dispatch, and types without an overload are skipped. Field accessors return `expected<T, Result>` and read straight from the input buffer, so strings come back as `std::string_view` with no copy. `PayloadDecoder` takes its field capacity and raw data fields from the spec.

```cpp
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

Repeating groups are read with `count()`, `next()` and `hasNext()` on the group accessor. An absent group reads as empty, and a nested group is read from the current entry of its enclosing group. Raw data fields (e.g. `XmlDataLen`/`XmlData`) declared in the spec are skipped safely, even when their bytes contain SOH or `=`. See `src/test/cpp/org/limitless/simdfix/` for more examples.

### Using your own spec

An application spec declares your messages and enums:

```xml
<protocol name="application">
    <types>
        <enum name="MessageType">
            <element name="Quote" value="S"/>
        </enum>
        <enum name="QuoteType">
            <element name="Indicative" value="0"/>
            <element name="Tradeable" value="1"/>
        </enum>
    </types>

    <message name="Quote" id="S">
        <field name="Header" type="StandardHeader"/>
        <field name="QuoteID" tag="117" primitiveType="string" length="20"/>
        <field name="Symbol" tag="55" primitiveType="string" length="8"/>
        <field name="QuoteType" tag="537" type="QuoteType" presence="optional"/>
        <field name="BidPx" tag="132" primitiveType="decimal"/>
        <field name="OfferPx" tag="133" primitiveType="decimal"/>
        <field name="BidSize" tag="134" primitiveType="uint32"/>
        <field name="OfferSize" tag="135" primitiveType="uint32"/>
        <field name="TransactTime" tag="60" primitiveType="timestamp" presence="optional"/>
    </message>
</protocol>
```

```cmake
simdfix_generate(QuoteMessages APP_XML quotes.xml NAMESPACE quotes::fix)
target_link_libraries(app PRIVATE QuoteMessages)
```

```cpp
#include "quotes/fix/messages/FixMessages.hpp"

using namespace quotes::fix::messages;
```

The generator then emits `QuoteEncoder` and `QuoteDecoder`, with one accessor per field (`quoteID()`, `bidPx()`, `quoteType()`, ...) and a `QuoteType` enum. The session messages from `session.xml` are always included.

A program can use several specs, e.g. one per venue. Give each its own `simdfix_generate()` target and `NAMESPACE`, then link them all.

[`examples/quotes`](examples/quotes) is a complete project that encodes and decodes a stream of quotes. It builds against an installed simdfix when `CMAKE_PREFIX_PATH` points at one, and against this source tree otherwise:

```bash
cmake -S examples/quotes -B examples/quotes/build -DCMAKE_BUILD_TYPE=Release
cmake --build examples/quotes/build
./examples/quotes/build/quotes
```

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

Generation is driven by up to three XML files. For each `simdfix_generate()` target it produces six headers under `<OUTPUT_DIR>/<namespace as a path>/`: `messages/FixMessages.hpp` (the one to include), `messages/FixTypes.hpp`, `messages/FixMessageDecoders.hpp`, `messages/FixMessageEncoders.hpp`, `messages/FixMessageHandler.hpp` and `config/FixEngine.hpp`.

| File | Role | Required |
|------|------|----------|
| `src/generator/resources/session.xml` | Session-layer messages (Logon, Logout, Heartbeat, TestRequest, ResendRequest, Reject, SequenceReset) and their enums | Always; defaults to the shipped copy |
| `src/generator/resources/protocol.xml` | Application-layer messages (e.g. NewOrderSingle, ExecutionReport) and their enums | Optional |
| `src/generator/resources/test.xml` | `protocol.xml` plus the extra groups, components and enums the tests exercise | Used by the in-tree tests |
| `src/generator/resources/config.xml` | Engine identity, buffer sizes, timing, session topology | Always; defaults to the shipped copy |

When both `session.xml` and an application spec are present the generator merges their data models before emitting code. Shared enums — in particular `MessageType` — are merged by value: entries from `session.xml` come first, then any new values from the application spec are appended. Duplicate values are silently dropped.

The in-tree tests and benchmarks generate `test.xml` into the default namespace (the `SimdFixTestMessages` target). An install ships `session.xml`, `config.xml` and `protocol.xml` under `<prefix>/share/simdfix`, which `find_package(SimdFix)` exposes as `SIMDFIX_RESOURCE_DIR`.

`simdfix_generate()` runs the generator for you. Its CLI is:

```
Generator [--namespace <ns>] <session.xml> <output-dir> <config.xml> <config-output-dir> [<application.xml>]
```

`--namespace` defaults to `org::limitless::simdfix::generated`. The generated headers include each other by that namespace as a path, so the output directories must be `<root>/<namespace as a path>/messages` and `.../config`, with `<root>` on the include path.

Headers are regenerated whenever the generator or one of its input XML files changes. They are never checked into the repository and must not be hand-edited.

## License

See [LICENSE](LICENSE) for details.
