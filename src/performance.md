# Performance

Figures from `SimdFixBenchmark` (release build: `-O3`, LTO, `-march=native`), measured on 2026-06-19.

## Test environment

| | |
|---|---|
| CPU | Apple M1 Pro |
| RAM | 32 GiB |
| OS | macOS 26.5.1 |
| Compiler | Apple clang 21.0.0 |

Time/throughput come from the benchmark's own reporting. Instructions and cycles
come from hardware counters (`/usr/bin/time -l`, instructions retired / cycles
elapsed), divided by the number of messages parsed. Counter figures include
whole-process overhead (startup, buffer fill), which is well under 1% for the
hot benchmarks but inflates the COLD figures slightly (it fills a 1 GiB buffer
before parsing).

## Results

| Benchmark | Message | ns/msg | GB/s |
|-----------|---------|-------:|-----:|
| COLD CACHE | Logon, 142 B | 83.3 | 1.71 |
| HOT CACHE | Logon, 142 B | 83.2 | 1.71 |
| GETTERS | Logon, 142 B | 119.9 | 1.18 |
| GROUPS | Logon + 3 hops, 253 B | 216.2 | 1.17 |
| ENCODE | Logon + 3 hops, ~224 B | 100.4 | 2.23 |
| NOS HOT | NewOrderSingle, 154 B | 86.0 | 1.79 |
| NOS GETTERS | NewOrderSingle, 154 B | 127.9 | 1.20 |
| NOS ENCODE | NewOrderSingle, 154 B | 38.0 | 4.29 |

## What each benchmark measures

- **COLD CACHE** — parses a 1 GiB buffer once; data streams from DRAM.
- **HOT CACHE** — parses a 256 KiB buffer 4,096 times; fits in L2, measures pure compute throughput of `PayloadDecoder::parse`.
- **GETTERS** — hot-cache parse plus every `LogonDecoder` getter applied to each message.
- **GROUPS** — as GETTERS, on a Logon carrying a 3-entry hops repeating group, iterating the group.
- **ENCODE** — encodes a Logon message with a 3-entry hops repeating group via `FixPayloadEncoder`/`LogonEncoder`.
- **NOS HOT** — hot-cache tokenization of a `NewOrderSingle` (154 B, 12 fields); no getters.
- **NOS GETTERS** — as NOS HOT plus all 8 mandatory `NewOrderSingleDecoder` getters (clOrdID, handlInst, symbol, side, transactTime, orderQty, ordType, price).
- **NOS ENCODE** — encodes a flat `NewOrderSingle` (no repeating groups) via `FixPayloadEncoder`/`NewOrderSingleEncoder`.

## Changes since previous measurement (2026-06-18)

Five optimisations were applied across the decode and encode paths:

1. **Inline tag copy** — `m_tags[]` is now populated inline during
   `processBlock`/`processTrailer` instead of a separate post-pass loop,
   eliminating a second traversal of up to 64 entries per parse.
2. **SWAR `convertToFixedDecimal`** — decimal field parsing (e.g. `price()`)
   uses SWAR digit parsing for values ≤ 8 bytes instead of a per-character
   loop with a branch for the `.` separator.
3. **Compile-time tag prefix** — `FieldEncoder::encode<Tag>()` precomputes
   `"TAG="` as a `constexpr` array and emits a single `memcpy` instead of
   a `memcpy` for the tag digits plus a separate `'='` store.
4. **`[[likely]]` on `processBlock` loop** — the inner tag-extraction while
   loop is annotated so the compiler lays out the fast path without the
   CheckSum early-exit branch polluting the instruction stream.
5. **Vectorised checksum accumulation** — the encoder's checksum loop
   accumulates partial sums in a `uint16x8_t` via `vpaddlq_u8` across blocks
   and reduces to scalar once at the end, avoiding a `vaddlvq_u8` horizontal
   reduction per block.

| Benchmark | Before | After | Change |
|-----------|-------:|------:|-------:|
| COLD CACHE | 84.9 ns | 83.3 ns | −1.9% |
| HOT CACHE | 84.5 ns | 83.2 ns | −1.5% |
| NOS HOT | 89.1 ns | 86.0 ns | −3.5% |
| NOS GETTERS | 126.6 ns | 127.9 ns | +1.0% |
| LOGON ENC | 115.1 ns | 100.4 ns | −12.8% |
| NOS ENCODE | 60.5 ns | 38.0 ns | −37.2% |

## Observations

- Parsing costs ~10.9 instructions per byte (HOT CACHE), and cold vs. hot
  throughput is nearly identical — the parser is compute-bound, not
  memory-bound, even when streaming from DRAM.
- Field access (GETTERS) adds ~38 ns per message on top of parsing for Logon;
  NOS GETTERS adds ~45 ns for 8 fields versus Logon's 6, consistent with the
  extra linear-search steps.
- The 3-entry repeating group roughly doubles per-message cost versus GETTERS
  (longer message plus group iteration), while byte throughput stays the same.
- NOS ENCODE (~38 ns/msg) is ~62% faster than Logon ENCODE (~100 ns/msg)
  because it has no repeating-group encoding pass.

## Reproducing

```bash
cmake -B cmake-build-release -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release
/usr/bin/time -l ./cmake-build-release/SimdFixBenchmark logon-hot   # or logon-cold|logon-hot|logon-getters|logon-groups|logon-encode|nos-hot|nos-getters|nos-encode|all
```

Instructions per message = instructions retired ÷ messages parsed, where
messages parsed is `(256 KiB / message length) × 4096` for the hot benchmarks
(plus 4 warm-up passes for HOT CACHE), `1 GiB / message length` for COLD, and
1,000,000 for ENCODE.