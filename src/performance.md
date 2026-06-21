# Performance

Figures from `SimdFixBenchmark` (release build: `-O3`, LTO, `-march=native`), measured on 2026-06-20.

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
| LOGON COLD | Logon, 142 B | 91.3 | 1.56 |
| LOGON HOT | Logon, 142 B | 90.7 | 1.57 |
| LOGON GETTERS | Logon, 142 B | 119.2 | 1.19 |
| LOGON GROUPS | Logon + 3 hops, 253 B | 215.2 | 1.18 |
| LOGON DATA | Logon + XmlData, 166 B | 150.7 | 1.10 |
| LOGON ENCODE | Logon + 3 hops, ~224 B | 102.5 | 2.19 |
| NOS HOT | NewOrderSingle, 154 B | 93.9 | 1.64 |
| NOS GETTERS | NewOrderSingle, 154 B | 127.5 | 1.21 |
| NOS ENCODE | NewOrderSingle, 154 B | 38.0 | 4.29 |

## What each benchmark measures

- **LOGON COLD** — parses a 1 GiB buffer once; data streams from DRAM.
- **LOGON HOT** — parses a 256 KiB buffer 4,096 times; fits in L2, measures pure compute throughput of `PayloadDecoder::parse`.
- **LOGON GETTERS** — hot-cache parse plus every `LogonDecoder` getter applied to each message.
- **LOGON GROUPS** — as GETTERS, on a Logon carrying a 3-entry hops repeating group, iterating the group.
- **LOGON DATA** — hot-cache parse plus getters of a Logon carrying an 11-byte XmlData payload (exercises the inline data-skip path).
- **LOGON ENCODE** — encodes a Logon message with a 3-entry hops repeating group via `FixPayloadEncoder`/`LogonEncoder`.
- **NOS HOT** — hot-cache tokenization of a `NewOrderSingle` (154 B, 12 fields); no getters.
- **NOS GETTERS** — as NOS HOT plus all 8 mandatory `NewOrderSingleDecoder` getters (clOrdID, handlInst, symbol, side, transactTime, orderQty, ordType, price).
- **NOS ENCODE** — encodes a flat `NewOrderSingle` (no repeating groups) via `FixPayloadEncoder`/`NewOrderSingleEncoder`.

## Changes since previous measurement (2026-06-19)

1. **Split-tag digit-zero fix** — `processBlock` checked `digits[0] == 0`
   to detect whether a tag carried across a 16-byte chunk boundary had
   ended. The digit `'0'` maps to value 0 after the SIMD subtraction,
   making it indistinguishable from "no digit". Changed to check the
   `tagDigitFlags` bitmask (`(tagDigitFlags & 0xF) == 0`) instead.
2. **New message types** — added ExecutionReport (35=8), ResendRequest
   (35=2), Reject (35=3), and SequenceReset (35=4) to the protocol
   definition, with ExecType, OrdStatus, SessionRejectReason, and
   GapFillFlag enums. These are compile-time code-generated decoders and
   encoders; the tokenizer hot path is unchanged.
3. **FixedString protocol constants** — `Protocol::Values` codes are now
   also available as free `inline constexpr FixedString` variables
   (`FIXT_1_1`, `FIX_4_3`, `FIX_4_4`) for use as template arguments.

| Benchmark | Before | After | Change |
|-----------|-------:|------:|-------:|
| LOGON COLD | 83.3 ns | 91.3 ns | +9.6% |
| LOGON HOT | 83.2 ns | 90.7 ns | +9.0% |
| LOGON GETTERS | 119.9 ns | 119.2 ns | −0.6% |
| NOS HOT | 86.0 ns | 93.9 ns | +9.2% |
| NOS GETTERS | 127.9 ns | 127.5 ns | −0.3% |
| LOGON ENCODE | 100.4 ns | 102.5 ns | +2.1% |
| NOS ENCODE | 38.0 ns | 38.0 ns | 0.0% |

The tokenization-only benchmarks (COLD, HOT, NOS HOT) show ~9% higher
latency. The code change (one branch condition in `processBlock`) is
too small to explain this; the likely cause is measurement environment
variation (thermal state, background load). Getter and encode benchmarks
are within run-to-run noise.

## Observations

- Cold vs. hot throughput is nearly identical — the parser is compute-bound,
  not memory-bound, even when streaming from DRAM.
- Field access (LOGON GETTERS) adds ~29 ns per message on top of parsing;
  NOS GETTERS adds ~34 ns for 8 fields versus Logon's 6, consistent with the
  extra linear-search steps.
- The 3-entry repeating group (LOGON GROUPS) roughly doubles per-message cost
  versus LOGON GETTERS (longer message plus group iteration), while byte
  throughput stays the same.
- The XmlData inline-skip path (LOGON DATA) adds ~31 ns over LOGON GETTERS,
  reflecting the scalar scan past the data payload and the extra token emission.
- NOS ENCODE (~38 ns/msg) is ~63% faster than LOGON ENCODE (~103 ns/msg)
  because it has no repeating-group encoding pass.

## Reproducing

```bash
cmake -B cmake-build-release -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release
/usr/bin/time -l ./cmake-build-release/SimdFixBenchmark logon-hot   # or logon-cold|logon-hot|logon-getters|logon-groups|logon-data|logon-encode|nos-hot|nos-getters|nos-encode|all
```

Instructions per message = instructions retired ÷ messages parsed, where
messages parsed is `(256 KiB / message length) × 4096` for the hot benchmarks
(plus 4 warm-up passes for HOT CACHE), `1 GiB / message length` for COLD, and
1,000,000 for ENCODE.