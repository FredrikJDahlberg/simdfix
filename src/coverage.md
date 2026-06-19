# Coverage Report

Generated: 2026-06-19  
Build: Debug (AddressSanitizer + `-fprofile-instr-generate -fcoverage-mapping`)  
Tool: `xcrun llvm-profdata` / `xcrun llvm-cov report`  
Tests: 113 tests across 10 test binaries, all passing

## Results

| File | Regions | Cover | Functions | Executed | Lines | Cover | Branches | Cover |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| `encoder/DataEncoder.hpp` | 3 | 100.00% | 2 | 100.00% | 8 | 100.00% | — | — |
| `encoder/FieldEncoder.hpp` | 21 | 100.00% | 13 | 100.00% | 107 | 91.59% | 6 | 83.33% |
| `encoder/GroupEncoder.hpp` | 4 | 100.00% | 3 | 100.00% | 10 | 100.00% | — | — |
| `encoder/MessageEncoder.hpp` | 3 | 100.00% | 3 | 100.00% | 7 | 100.00% | — | — |
| `encoder/PayloadEncoder.hpp` | 11 | 100.00% | 5 | 100.00% | 46 | 100.00% | 4 | 100.00% |
| `utils/NullableInt.hpp` | 110 | 97.27% | 37 | 100.00% | 175 | 96.57% | 56 | 75.00% |
| `decoder/GroupDecoder.hpp` | 18 | 94.44% | 7 | 100.00% | 52 | 92.31% | 8 | 75.00% |
| `decoder/PayloadDecoder.hpp` | 124 | 89.52% | 10 | 100.00% | 329 | 91.49% | 86 | 81.40% |
| `unused/QuadSearch.hpp` | 38 | 81.58% | 1 | 100.00% | 57 | 70.18% | 22 | 72.73% |
| `decoder/DataDecoder.hpp` | 10 | 80.00% | 3 | 100.00% | 23 | 65.22% | 4 | 50.00% |
| `utils/FixedDecimal.hpp` | 38 | 78.95% | 22 | 77.27% | 124 | 56.45% | 10 | 60.00% |
| `decoder/FieldDecoder.hpp` | 81 | 74.07% | 24 | 87.50% | 174 | 59.77% | 30 | 70.00% |
| `utils/Utils.hpp` | 112 | 71.43% | 23 | 82.61% | 370 | 44.86% | 102 | 70.59% |
| `messages/FixMessageEncoders.hpp` | 71 | 63.38% | 64 | 59.38% | 233 | 60.09% | — | — |
| `CodecTypes.hpp` | 21 | 42.86% | 14 | 21.43% | 35 | 20.00% | 4 | 50.00% |
| `messages/FixMessageDecoders.hpp` | 94 | 38.30% | 85 | 31.76% | 266 | 33.08% | — | — |
| `messages/FixMessageHandler.hpp` | 24 | 29.17% | 7 | 28.57% | 57 | 35.09% | 22 | 40.91% |
| `decoder/MessageDecoder.hpp` | 6 | 16.67% | 6 | 16.67% | 47 | 2.13% | — | — |
| `simd/LinearSearch.hpp` | 1 | 0.00% | 1 | 0.00% | 22 | 0.00% | — | — |
| **TOTAL** | **852** | **74.65%** | **366** | **66.39%** | **2289** | **63.65%** | **364** | **71.70%** |

## Notes

### `simd/LinearSearch.hpp`
The combined report cannot attribute coverage to `LinearSearch.hpp` because its
`inline find()` function is compiled into every test binary that transitively
includes the file. Each compilation produces different internal counter IDs, and
`llvm-cov` discards all conflicting entries as mismatched data.

Running the report against `LinearSearchTest` alone shows **100% coverage**
(13/13 regions, 1/1 functions, 22/22 lines, 8/8 branches). The 8 `LinearSearch`
tests cover all paths: empty array, single element, pure scalar path (< 8
elements), exact SIMD boundary (8 elements), SIMD + scalar tail (9 elements),
two SIMD chunks (16 elements), duplicate key handling, and basics.

### `decoder/MessageDecoder.hpp`
Low line coverage (2%) despite passing tests. `MessageDecoder` is a class
template instantiated only for specific protocol types via the generated
`FixMessageDecoders.hpp`; most template specialisations are not exercised by the
current test suite.

### `messages/` generated headers
Coverage below 40% for `FixMessageDecoders.hpp` and `FixMessageHandler.hpp`
because only the Logon and Logout message types are exercised; all other
generated message types are untested.

## How to regenerate

```bash
cmake --build cmake-build-debug --target Coverage
```
