# Performance

Figures from `SimdFixBenchmark` (release build: `-O3`, LTO, `-march=native`), measured on 2026-06-11.

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

| Benchmark | Message | ns/msg | GB/s | Instructions/msg | Cycles/msg | IPC |
|-----------|---------|-------:|-----:|-----------------:|-----------:|----:|
| COLD CACHE | Logon, 142 B | 83.6 | 1.70 | ~1,610 | ~340 | 4.7 |
| HOT CACHE | Logon, 142 B | 84.2 | 1.69 | ~1,510 | ~268 | 5.6 |
| GETTERS | Logon, 142 B | 122.9 | 1.16 | ~2,295 | ~392 | 5.9 |
| GROUPS | Logon + 3 hops, 253 B | 218.6 | 1.16 | ~4,080 | ~698 | 5.8 |
| ENCODE | Logon + 3 hops, ~224 B | 125.0 | 1.78 | ~815 | ~400 | 2.0 |

## What each benchmark measures

- **COLD CACHE** — parses a 1 GiB buffer once; data streams from DRAM.
- **HOT CACHE** — parses a 256 KiB buffer 4,096 times; fits in L2, measures pure compute throughput of `PayloadDecoder::parse`.
- **GETTERS** — hot-cache parse plus every `LogonDecoder` getter applied to each message.
- **GROUPS** — as GETTERS, on a Logon carrying a 3-entry hops repeating group, iterating the group.
- **ENCODE** — encodes a Logon message with a 3-entry hops repeating group via `FixPayloadEncoder`/`LogonEncoder`.

## Observations

- Parsing costs ~10.6 instructions per byte (HOT CACHE), and cold vs. hot
  throughput is nearly identical — the parser is compute-bound, not
  memory-bound, even when streaming from DRAM.
- IPC of 5.6–5.9 is close to the M1 performance core's retire width, so
  per-message cost scales with instruction count rather than stalls.
- Field access (GETTERS) adds ~785 instructions (~124 cycles) per message on
  top of parsing.
- The 3-entry repeating group roughly doubles per-message cost versus GETTERS
  (longer message plus group iteration), while byte throughput stays the same.
- ENCODE's IPC (~2.0) is much lower than the decode paths (5.6-5.9) — encoding
  is less parallel SIMD-wise, but at ~815 instructions/msg it's still cheaper
  than GETTERS (~2,295 instructions/msg).

## Reproducing

```bash
cmake -B cmake-build-release -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release
/usr/bin/time -l ./cmake-build-release/SimdFixBenchmark hot   # or cold|getters|groups|encode|all
```

Instructions per message = instructions retired ÷ messages parsed, where
messages parsed is `(256 KiB / message length) × 4096` for the hot benchmarks
(plus 4 warm-up passes for HOT CACHE), `1 GiB / message length` for COLD, and
1,000,000 for ENCODE.