# Performance

Figures from `SimdFixBenchmark` (release build: `-O3`, LTO, `-march=native`), measured on 2026-06-21.

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
| LOGON COLD | Logon, 142 B | 90.8 | 1.56 |
| LOGON HOT | Logon, 142 B | 90.5 | 1.57 |
| LOGON GETTERS | Logon, 142 B | 112.8 | 1.26 |
| LOGON GROUPS | Logon + 3 hops, 253 B | 211.2 | 1.20 |
| LOGON DATA | Logon + XmlData, 166 B | 143.7 | 1.16 |
| LOGON ENCODE | Logon + 3 hops, ~224 B | 101.1 | 2.22 |
| NOS HOT | NewOrderSingle, 154 B | 93.6 | 1.65 |
| NOS GETTERS | NewOrderSingle, 154 B | 129.8 | 1.19 |
| NOS ENCODE | NewOrderSingle, 154 B | 37.1 | 4.40 |

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

## Changes since previous measurement (2026-06-20)

1. **Index caching for required fields** — `validate()` now stores the
   token index (`int8_t`, 1 byte per field) for each required field found
   via `findIndex()`. Getters for required fields use the cached index
   with `valueAt`/`enumAt`/`timeOnlyAt`/`dateOnlyAt` instead of
   re-searching. Optional fields still search on demand.
2. **UTC timestamp range validation** — `dateTimeToEpochUTC`,
   `timeOnlyToMillis`, and `dateOnlyToEpochUTC` now validate hours (0-23),
   minutes (0-59), seconds (0-59), month (1-12), and day (1-31), returning
   -1 for out-of-range values. Getters propagate the error as
   `std::unexpected{Result::InvalidLength}`.
3. **Integer and decimal field validation** — `convertToUint32`,
   `convertToInt32`, and `convertToFixedDecimal` now validate field
   content at the point of use. Length bounds reject overflow (>10 digits
   for uint32, >11 for int32, >20 for decimal). SWAR `isDigits` checks
   all bytes are '0'–'9'; `findByte('.')` locates the decimal point
   without a libc call. Returns `InvalidLength` or `InvalidValue`.
4. **Enum validation** — `getEnum`/`enumAt` return
   `std::unexpected{Result::InvalidValue}` for unrecognized codes instead
   of silently returning `Enum::Null`.
5. **Scope safety** — `wrap()` resets group scope depth;
   `popGroupScope()` guards against underflow.

| Benchmark | Before | After | Change |
|-----------|-------:|------:|-------:|
| LOGON HOT | 90.7 ns | 90.5 ns | −0.2% |
| LOGON GETTERS | 119.2 ns | 112.8 ns | −5.4% |
| NOS HOT | 93.9 ns | 93.6 ns | −0.3% |
| NOS GETTERS | 127.5 ns | 129.8 ns | +1.8% |
| LOGON ENCODE | 102.5 ns | 101.1 ns | −1.4% |
| NOS ENCODE | 38.0 ns | 37.1 ns | −2.4% |

Index caching reduces Logon getter overhead from 28.5 ns to 22.3 ns
(−22%). NOS getters show a small increase (+2.3 ns) from the decimal
digit validation on the `price` field, offset by index caching.
Tokenization-only and encode benchmarks are unchanged.

## Observations

- Cold vs. hot throughput is nearly identical — the parser is compute-bound,
  not memory-bound, even when streaming from DRAM.
- Field access (LOGON GETTERS) adds ~22 ns per message on top of parsing
  thanks to index caching of required fields; NOS GETTERS adds ~36 ns for
  its larger mix of required and optional fields (including SWAR digit
  validation on integers and decimals).
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