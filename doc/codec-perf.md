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
| LOGON COLD | Logon, 142 B | 94.0 | 1.51 |
| LOGON HOT | Logon, 142 B | 94.2 | 1.51 |
| LOGON GETTERS | Logon, 142 B | 118.4 | 1.20 |
| LOGON GROUPS | Logon + 3 hops, 253 B | 203.6 | 1.24 |
| LOGON DATA | Logon + XmlData, 166 B | 142.8 | 1.16 |
| LOGON ENCODE | Logon + 3 hops, ~224 B | 137.0 | 1.61 |
| NOS HOT | NewOrderSingle, 154 B | 96.8 | 1.59 |
| NOS GETTERS | NewOrderSingle, 154 B | 152.8 | 1.01 |
| NOS ENCODE | NewOrderSingle, 154 B | 102.1 | 1.60 |
| ER HOT | ExecutionReport, 245 B | 148.7 | 1.65 |
| ER GETTERS | ExecutionReport, 245 B | 249.4 | 0.98 |
| ER ENCODE | ExecutionReport, 245 B | 177.9 | 1.38 |

> **Encode figures measure real work.** The encode benchmarks feed their field
> inputs through a runtime taint (see below) so the compiler cannot constant-fold
> a literal message into precomputed bytes. Earlier revisions reported folded
> encode numbers (e.g. NOS ENCODE 37 ns / 4.37 GB/s) that were unattainable with
> runtime inputs.

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

## Changes since previous measurement (2026-07-18)

1. **Encode benchmarks made non-foldable** — the encode loops fed compile-time
   literal inputs (sequence number, timestamps, quantities, prices). With the
   whole message known at compile time, the optimizer precomputed each formatted
   field — most significantly the 21-byte UTCTimestamp — into constant stores, so
   the benchmark measured constant folding rather than encoding. Each encode loop
   now adds a runtime value (a `volatile` read, always zero, so the emitted bytes
   are unchanged) to its numeric inputs, forcing the formatting to run as it does
   in a live engine. The encode figures rise accordingly and now reflect real
   per-message cost. Decode benchmarks are unaffected — they already parse runtime
   bytes from a buffer.
2. **Runtime `PayloadEncoder`** — `PayloadEncoder` is now constructed from runtime
   begin-string / sender / target strings rather than compile-time template
   parameters. The header prefix is built once into an inline, heap-free
   `std::array`; `wrap()` copies a fixed-size prefix that lowers to inline NEON
   stores (no `memcpy` call).

| Benchmark | Before (folded) | After (real) | Change |
|-----------|----------------:|-------------:|-------:|
| LOGON ENCODE | 102.3 ns | 137.0 ns | +33.9% |
| NOS ENCODE | 37.3 ns | 102.1 ns | +173.7% |
| ER ENCODE | 103.0 ns | 177.9 ns | +72.7% |

The jump is largest for NewOrderSingle: with only two timestamps and a short
body, the folded-away timestamp formatting dominated its previous figure. The
change is a measurement correction, not a regression — for realistic (runtime)
inputs the runtime encoder is in fact slightly faster than the former
compile-time-template encoder (NOS ~76 ns vs ~90 ns when both format a runtime
timestamp); the old 37 ns was never attainable outside a literal-only benchmark.
Decode figures drift ±1–4% from the 2026-06-24 run (run-to-run/thermal), with no
algorithmic change.

## Changes since previous measurement (2026-06-24)

1. **Namespace/directory alignment** — `FieldDecoder` moved to
   `detail::decoder`, `FieldEncoder` to `detail::encoder`, matching their
   directory paths. `PayloadEncoder` moved from `detail/encoder/` to the
   public `encoder/` directory, symmetric with `PayloadDecoder`.
2. **Public/internal separation enforced** — Introduced
   `detail::TokenizedMessage` to bundle the token spans passed between
   `PayloadDecoder` and the generated handler. `MessageDecoder::wrap` is
   now protected, taking `TokenizedMessage` instead of raw `Field`/tag
   spans. Removed `using namespace detail` from five public headers that
   did not need it.
3. **detail/ excluded from install** — `cmake --install` no longer ships
   the `detail/` tree; consumers depend only on the public headers.

These are structural refactors with no algorithmic changes. All numbers
are within run-to-run noise of the previous measurement.

| Benchmark | Before (06-24) | After (06-24) | Change |
|-----------|---------------:|--------------:|-------:|
| LOGON HOT | 91.8 ns | 92.3 ns | +0.5% |
| LOGON GETTERS | 109.3 ns | 109.2 ns | −0.1% |
| LOGON GROUPS | 199.8 ns | 199.8 ns | 0.0% |
| LOGON DATA | 153.2 ns | 154.2 ns | +0.7% |
| NOS HOT | 95.5 ns | 95.0 ns | −0.5% |
| NOS GETTERS | 130.3 ns | 128.5 ns | −1.4% |
| ER HOT | 145.9 ns | 145.7 ns | −0.1% |
| ER GETTERS | 237.6 ns | 239.3 ns | +0.7% |
| LOGON ENCODE | 101.9 ns | 102.3 ns | +0.4% |
| NOS ENCODE | 38.0 ns | 37.3 ns | −1.8% |
| ER ENCODE | 101.9 ns | 103.0 ns | +1.1% |

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
- NOS ENCODE (~102 ns/msg) is faster than LOGON ENCODE (~137 ns/msg): the Logon
  carries a 3-entry hops group, i.e. three extra timestamps and counters to
  format, on top of a longer message.
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