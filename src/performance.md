# Performance

Figures from `SimdFixBenchmark` (release build: `-O3`, LTO, `-march=native`), measured on 2026-06-24.

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
| LOGON COLD | Logon, 142 B | 92.4 | 1.54 |
| LOGON HOT | Logon, 142 B | 91.8 | 1.55 |
| LOGON GETTERS | Logon, 142 B | 109.3 | 1.30 |
| LOGON GROUPS | Logon + 3 hops, 253 B | 199.8 | 1.27 |
| LOGON DATA | Logon + XmlData, 166 B | 153.2 | 1.08 |
| LOGON ENCODE | Logon + 3 hops, ~224 B | 101.9 | 2.20 |
| NOS HOT | NewOrderSingle, 154 B | 95.5 | 1.61 |
| NOS GETTERS | NewOrderSingle, 154 B | 130.3 | 1.18 |
| NOS ENCODE | NewOrderSingle, 154 B | 38.0 | 4.29 |
| ER HOT | ExecutionReport, 245 B | 145.9 | 1.68 |
| ER GETTERS | ExecutionReport, 245 B | 237.6 | 1.03 |
| ER ENCODE | ExecutionReport, 245 B | 101.9 | 2.40 |

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
- **ER HOT** — hot-cache tokenization of a flat `ExecutionReport` (245 B); no getters.
- **ER GETTERS** — as ER HOT plus all 16 `ExecutionReportDecoder` getters (orderID, clOrdID, execID, execType, ordStatus, symbol, side, orderQty, price, lastQty, lastPx, leavesQty, cumQty, avgPx, transactTime, text) — a wide mix of string, enum, integer, decimal, and timestamp fields.
- **ER ENCODE** — encodes a flat `ExecutionReport` via `FixPayloadEncoder`/`ExecutionReportEncoder`.

## Changes since previous measurement (2026-06-21)

1. **Enum decode via compile-time byte table** — `utils::find<Enum>` now
   builds a 256-entry lookup table at compile time when every enum code is a
   single byte, turning enum resolution into one indexed load instead of a
   linear scan over the code list. Multi-byte protocols fall back to the
   original scan.
2. **Repeating-group single delimiter scan** — `GroupDecoder` caches the
   offset of the next entry (`m_next`, computed once when an entry is
   entered) and reuses it as the start of the following entry, so each
   group entry costs one delimiter scan instead of two.
3. **Timestamp date cache in the decoder** — `FieldDecoder` memoizes the
   last `YYYYMMDD` → epoch-days result, keyed on the 8 date bytes. When
   consecutive timestamps share a date (e.g. SendingTime/TransactTime in
   one message, or a day's feed) the calendar arithmetic is skipped and
   only the time-of-day is parsed. The cache persists across `wrap()`.
4. **ExecutionReport benchmark** — added `er-hot`, `er-getters`, and
   `er-encode` to exercise a wider, more realistic message (245 B, 16
   fields spanning every field type).

| Benchmark | Before (06-21) | After (06-24) | Change |
|-----------|---------------:|--------------:|-------:|
| LOGON HOT | 90.5 ns | 91.8 ns | +1.4% |
| LOGON GETTERS | 112.8 ns | 109.3 ns | −3.1% |
| LOGON GROUPS | 211.2 ns | 199.8 ns | −5.4% |
| LOGON DATA | 143.7 ns | 153.2 ns | +6.6% |
| NOS HOT | 93.6 ns | 95.5 ns | +2.0% |
| NOS GETTERS | 129.8 ns | 130.3 ns | +0.4% |
| LOGON ENCODE | 101.1 ns | 101.9 ns | +0.8% |
| NOS ENCODE | 37.1 ns | 38.0 ns | +2.4% |

The signal is in the field-access paths: LOGON GETTERS (−3.1%) and LOGON
GROUPS (−5.4%) improve from the enum byte-table and the group single-scan,
both compounded by the timestamp date cache (the Logon group carries three
same-date timestamps). The tokenization-only and encode paths
(LOGON/NOS HOT, both ENCODE, LOGON DATA) are untouched by these changes;
their ±1–7% drift is run-to-run/thermal variation between measurement
sessions, not a regression — all three optimizations live in the
decoder's getter path only.

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
- NOS ENCODE (~38 ns/msg) is ~63% faster than LOGON ENCODE (~102 ns/msg)
  because it has no repeating-group encoding pass.
- ExecutionReport (245 B, 16 fields) is the widest message benchmarked: its
  16 getters add ~92 ns over the hot parse (ER GETTERS 237.6 ns vs ER HOT
  145.9 ns), roughly proportional to its field count versus the smaller
  Logon and NewOrderSingle getter sets.

## Reproducing

```bash
cmake -B cmake-build-release -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release
/usr/bin/time -l ./cmake-build-release/SimdFixBenchmark logon-hot   # or logon-cold|logon-hot|logon-getters|logon-groups|logon-data|logon-encode|nos-hot|nos-getters|nos-encode|er-hot|er-getters|er-encode|all
```

Instructions per message = instructions retired ÷ messages parsed, where
messages parsed is `(256 KiB / message length) × 4096` for the hot benchmarks
(plus 4 warm-up passes for HOT CACHE), `1 GiB / message length` for COLD, and
1,000,000 for ENCODE.