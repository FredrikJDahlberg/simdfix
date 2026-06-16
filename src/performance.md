# Performance

Figures from `SimdFixBenchmark` (release build: `-O3`, LTO, `-march=native`), measured on 2026-06-16.

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
| COLD CACHE | Logon, 142 B | 85.2 | 1.67 | ~1,632 | ~324 | 5.0 |
| HOT CACHE | Logon, 142 B | 85.6 | 1.66 | ~1,542 | ~272 | 5.7 |
| GETTERS | Logon, 142 B | 121.3 | 1.17 | ~2,332 | ~388 | 6.0 |
| GROUPS | Logon + 3 hops, 253 B | 218.2 | 1.16 | ~4,135 | ~696 | 5.9 |
| ENCODE | Logon + 3 hops, ~224 B | 119.2 | 1.88 | ~867 | ~378 | 2.3 |

## What each benchmark measures

- **COLD CACHE** — parses a 1 GiB buffer once; data streams from DRAM.
- **HOT CACHE** — parses a 256 KiB buffer 4,096 times; fits in L2, measures pure compute throughput of `PayloadDecoder::parse`.
- **GETTERS** — hot-cache parse plus every `LogonDecoder` getter applied to each message.
- **GROUPS** — as GETTERS, on a Logon carrying a 3-entry hops repeating group, iterating the group.
- **ENCODE** — encodes a Logon message with a 3-entry hops repeating group via `FixPayloadEncoder`/`LogonEncoder`.

## Observations

- Parsing costs ~10.9 instructions per byte (HOT CACHE), and cold vs. hot
  throughput is nearly identical — the parser is compute-bound, not
  memory-bound, even when streaming from DRAM.
- IPC of 5.7–6.0 is close to the M1 performance core's retire width, so
  per-message cost scales with instruction count rather than stalls.
- Field access (GETTERS) adds ~790 instructions (~116 cycles) per message on
  top of parsing.
- The 3-entry repeating group roughly doubles per-message cost versus GETTERS
  (longer message plus group iteration), while byte throughput stays the same.
- ENCODE improved from 125 ns/msg to 119 ns/msg (−4.8%) versus the 2026-06-11
  baseline. The checksum loop was vectorised with NEON (`vaddlvq_u8`, 16
  bytes/iteration): instruction count rose slightly (~815 → ~867) but cycles
  fell from ~400 to ~378 (−5.5%) and IPC rose from 2.0 to 2.3, reflecting
  improved pipelining.

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