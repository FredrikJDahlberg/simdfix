# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository. See [README.md](README.md) for full project documentation and benchmarks.

## What This Is

**simdfix** is a SIMD-accelerated FIX protocol codec in C++23 (ARM NEON).

## Build Commands

```bash
# Debug build (includes AddressSanitizer + coverage)
cmake -B cmake-build-debug -DCMAKE_BUILD_TYPE=Debug
cmake --build cmake-build-debug

# Release build (O3, LTO, march=native)
cmake -B cmake-build-release -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release

# Build a specific target
cmake --build cmake-build-debug --target DecoderTest
```

## Test Commands

```bash
# Run all tests
cd cmake-build-debug && ctest

# Run a single test binary
./cmake-build-debug/PayloadDecoderTest

# Filter to specific tests
./cmake-build-debug/MessageDecoderTest --gtest_filter="MessageDecoder.Logon"
./cmake-build-debug/PayloadDecoderTest --gtest_filter="PayloadDecoder.TrailerSplitCheckSum"
./cmake-build-debug/FieldDecoderTest --gtest_filter="FieldDecoder.GetFixedDecimal"

# Run benchmark (always use the release build; debug build numbers are meaningless due to ASan/coverage overhead)
./cmake-build-release/SimdFixBenchmark
./cmake-build-release/SimdFixBenchmark logon-cold|logon-hot|logon-getters|logon-groups|nos-hot|er-hot|all   # default is all
```

## Coding Style

- **Brace style**: Allman (opening brace on its own line) for namespaces, structs, functions, and control flow.
- **Indentation**: 4 spaces, no tabs.
- **Naming**: `PascalCase` for types/structs/enums (`FieldDecoder`, `GroupDecoder`, `ParentType`); `camelCase` for functions, methods, and local variables (`findIndex`, `pushGroupScope`, `nextGroupOffset`); `m_` prefix for member variables (`m_decoder`, `m_offset`, `m_scopeDepth`).
- **Template parameters**: `PascalCase`, including non-type parameters (`Tag`, `Required`, `Parent`, `Enum`).
- **`[[nodiscard]]`**: applied to all accessor/getter methods that return a value.
- **`constexpr`**: applied to methods that can be evaluated at compile time, especially field accessors.
- **Error handling**: no exceptions in the parsing/decoding path — fallible operations return `std::expected<T, Result>`. (`std::invalid_argument` thrown from `GroupDecoder::wrap` on a missing tag is a known exception to this rule.)
- **Encapsulation**: keep raw buffers/spans (`m_data`, `m_tokens`, `m_tags`) private; expose narrow accessors (`tokenAt`, `indexOf`, `byteAt`) instead of the underlying containers.
- **Doc comments**: use Doxygen-style `/** ... */` blocks with `@tparam`, `@param`, `@return`, and `@throws` as applicable, as in `FieldDecoder.hpp`. Apply to public/template methods on encoder and decoder classes.
- **Generated code**: never hand-edit files produced by `Generator` (e.g. `FixMessageDecoders.hpp`, `FixMessageHandler.hpp`); change `protocol.xml`/`config.xml` or the generator source instead. Generated headers live in the CMake build directory (`<build>/org/limitless/fix/generated/`), not in the source tree.
