# Coverage Report

Generated: 2026-06-16  
Build: Debug (AddressSanitizer + `-fprofile-instr-generate -fcoverage-mapping`)  
Tool: `xcrun llvm-profdata` / `xcrun llvm-cov report`  
Tests: 68 tests across 10 test binaries, all passing

## Results

| File | Regions | Cover | Functions | Executed | Lines | Cover | Branches | Cover |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| `encoder/FieldEncoder.hpp` | 16 | 100.00% | 11 | 100.00% | 85 | 84.71% | 4 | 75.00% |
| `encoder/GroupEncoder.hpp` | 4 | 100.00% | 3 | 100.00% | 10 | 100.00% | — | — |
| `encoder/MessageEncoder.hpp` | 3 | 100.00% | 3 | 100.00% | 7 | 100.00% | — | — |
| `encoder/PayloadEncoder.hpp` | 8 | 100.00% | 5 | 100.00% | 38 | 100.00% | 2 | 100.00% |
| `decoder/PayloadDecoder.hpp` | 101 | 95.05% | 8 | 100.00% | 271 | 96.68% | 72 | 88.89% |
| `decoder/GroupDecoder.hpp` | 12 | 91.67% | 7 | 85.71% | 52 | 63.46% | 4 | 75.00% |
| `decoder/FieldDecoder.hpp` | 93 | 78.49% | 23 | 91.30% | 144 | 86.81% | 40 | 75.00% |
| `unused/QuadSearch.hpp` | 38 | 81.58% | 1 | 100.00% | 57 | 70.18% | 22 | 72.73% |
| `simd/Uint8x16.hpp` | 60 | 83.33% | 34 | 85.29% | 141 | 80.14% | 10 | 60.00% |
| `utils/FixedDecimal.hpp` | 77 | 77.92% | 19 | 94.74% | 128 | 86.72% | 42 | 64.29% |
| `utils/Utils.hpp` | 121 | 74.38% | 23 | 86.96% | 360 | 53.61% | 108 | 72.22% |
| `utils/BitSet64.hpp` | 18 | 50.00% | 16 | 43.75% | 54 | 46.30% | — | — |
| `CodecTypes.hpp` | 21 | 42.86% | 14 | 21.43% | 35 | 20.00% | 4 | 50.00% |
| `messages/FixMessageEncoders.hpp` | 51 | 56.86% | 46 | 52.17% | 163 | 52.15% | — | — |
| `messages/FixMessageHandler.hpp` | 20 | 35.00% | 6 | 33.33% | 48 | 41.67% | 18 | 44.44% |
| `messages/FixMessageDecoders.hpp` | 65 | 35.38% | 58 | 27.59% | 183 | 28.96% | — | — |
| `decoder/MessageDecoder.hpp` | 6 | 16.67% | 6 | 16.67% | 47 | 2.13% | — | — |
| `simd/LinearSearch.hpp` | 1 | 0.00% | 1 | 0.00% | 22 | 0.00% | — | — |
| **TOTAL** | **715** | **72.73%** | **284** | **62.68%** | **1845** | **64.77%** | **326** | **73.31%** |

## Notes

### `simd/LinearSearch.hpp`
The combined report cannot attribute coverage to `LinearSearch.hpp` because its
`inline find()` function is compiled into every test binary that transitively
includes the file. Each compilation produces different internal counter IDs, and
`llvm-cov` discards all conflicting entries as mismatched data.

Running the report against `LinearSearchTest` alone shows **100% coverage**
(13/13 regions, 1/1 functions, 22/22 lines, 8/8 branches). The 7 `LinearSearch`
tests cover all paths: empty array, single element, pure scalar path (< 8
elements), exact SIMD boundary (8 elements), SIMD + scalar tail (9 elements),
two SIMD chunks (16 elements), and duplicate key handling.

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
