# Performance

Figures from `SimdFixBenchmark` (release build: `-O3`, LTO, `-march=native`), measured on 2026-06-18.

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
| COLD CACHE | Logon, 142 B | 84.9 | 1.67 | ~1,632 | ~324 | 5.0 |
| HOT CACHE | Logon, 142 B | 84.5 | 1.68 | ~1,542 | ~272 | 5.7 |
| GETTERS | Logon, 142 B | 120.4 | 1.18 | ~2,332 | ~388 | 6.0 |
| GROUPS | Logon + 3 hops, 253 B | 218.4 | 1.16 | ~4,135 | ~696 | 5.9 |
| ENCODE | Logon + 3 hops, ~224 B | 115.1 | 1.95 | ~867 | ~378 | 2.3 |
| NOS HOT | NewOrderSingle, 154 B | 89.1 | 1.73 | ~1,635 | ~285 | 5.7 |
| NOS GETTERS | NewOrderSingle, 154 B | 126.6 | 1.22 | ~2,498 | ~403 | 6.2 |
| NOS ENCODE | NewOrderSingle, 154 B | 60.5 | 2.54 | ~401 | ~177 | 2.3 |

## What each benchmark measures

- **COLD CACHE** — parses a 1 GiB buffer once; data streams from DRAM.
- **HOT CACHE** — parses a 256 KiB buffer 4,096 times; fits in L2, measures pure compute throughput of `PayloadDecoder::parse`.
- **GETTERS** — hot-cache parse plus every `LogonDecoder` getter applied to each message.
- **GROUPS** — as GETTERS, on a Logon carrying a 3-entry hops repeating group, iterating the group.
- **ENCODE** — encodes a Logon message with a 3-entry hops repeating group via `FixPayloadEncoder`/`LogonEncoder`.
- **NOS HOT** — hot-cache tokenization of a `NewOrderSingle` (154 B, 12 fields); no getters.
- **NOS GETTERS** — as NOS HOT plus all 8 mandatory `NewOrderSingleDecoder` getters (clOrdID, handlInst, symbol, side, transactTime, orderQty, ordType, price).
- **NOS ENCODE** — encodes a flat `NewOrderSingle` (no repeating groups) via `FixPayloadEncoder`/`NewOrderSingleEncoder`.

## Observations

- Parsing costs ~10.9 instructions per byte (HOT CACHE), and cold vs. hot
  throughput is nearly identical — the parser is compute-bound, not
  memory-bound, even when streaming from DRAM.
- IPC of 5.7–6.2 is close to the M1 performance core's retire width, so
  per-message cost scales with instruction count rather than stalls.
- Field access (GETTERS) adds ~790 instructions (~116 cycles) per message on
  top of parsing for Logon; NOS GETTERS adds ~863 instructions (~118 cycles)
  for 8 fields versus Logon's 6, consistent with the extra linear-search steps.
- The 3-entry repeating group roughly doubles per-message cost versus GETTERS
  (longer message plus group iteration), while byte throughput stays the same.
- ENCODE improved from 125 ns/msg to 115 ns/msg (−8%) versus the 2026-06-11
  baseline. The checksum loop was vectorised with NEON (`vaddlvq_u8`, 16
  bytes/iteration): instruction count rose slightly (~815 → ~867) but cycles
  fell and IPC rose from 2.0 to 2.3, reflecting improved pipelining.
- NOS ENCODE (60.5 ns/msg, ~401 instr) is ~48% faster than Logon ENCODE
  (115.1 ns/msg, ~867 instr) because it has no repeating-group encoding pass.

## Reproducing

```bash
cmake -B cmake-build-release -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release
/usr/bin/time -l ./cmake-build-release/SimdFixBenchmark hot   # or cold|getters|groups|encode|nos-hot|nos-getters|nos-encode|all
```

Instructions per message = instructions retired ÷ messages parsed, where
messages parsed is `(256 KiB / message length) × 4096` for the hot benchmarks
(plus 4 warm-up passes for HOT CACHE), `1 GiB / message length` for COLD, and
1,000,000 for ENCODE.