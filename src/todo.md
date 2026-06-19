TODO
-----

1. Move decoding functions from utils to decoder.
2. Add price scaling support — `FixedDecimal` currently uses a fixed 10^-8
   scale; encoding always produces 8 fractional digits (e.g. `120.00000000`).
   Need configurable precision or trailing-zero stripping in
   `fixedDecimalToAscii` so prices encode cleanly (e.g. `120` or `120.50`).

Unsupported FIX data types
-----

`DataModel.hpp` (`m_types`) currently maps `protocol.xml` primitive types to
`char`, `uint8`, `int32`, `uint32`, `int64`, `uint64`, `decimal`,
`timestamp`, and `string`. The following standard FIX data types have no
mapping/`Category` and cannot yet be used in `protocol.xml`:

- `UTCTimeOnly` — "HH:MM:SS" or "HH:MM:SS.sss" (8 or 12 bytes). Parsing
  is a subset of the existing `dateTimeToEpochUTC` logic (time-of-day
  portion). Needs a `Category::TimeOnly` that maps to
  `std::chrono::milliseconds` (millis since midnight) and a
  `getTimeOnly` / `encodeTimeOnly` pair.
- `UTCDateOnly` — "YYYYMMDD" (8 bytes). Parsing reuses the existing SWAR
  date parse in `dateTimeToEpochUTC`. Needs a `Category::DateOnly` that
  maps to `std::chrono::days` (or an integer day count) and a
  `getDateOnly` / `encodeDateOnly` pair.
- `LocalMktDate` — same wire format as `UTCDateOnly`, different semantic
  (local timezone). Can share the same `Category::DateOnly` codec.
- `MonthYear` — "YYYYMM", "YYYYMMDD", or "YYYYMMwN" (6, 8, or 8 bytes).
  Variable-length; needs its own parser and a struct or integer
  representation.
- `TZTimeOnly`, `TZTimestamp` — time/timestamp with timezone offset
  suffix ("±HH" or "±HH:MM"). Extends the UTC parsers with offset
  extraction.
- `Tenor` — duration code ("D1", "W2", "M3", "Y1"). Small enough to
  represent as a string or a dedicated struct with unit+count.
- `MultipleCharValue`, `MultipleStringValue` — space-delimited multi-value
  fields. Would need a container return type (e.g. small vector or
  iterator) rather than a single scalar.

Tokenizer binary-safe data (inline skip)
-----

DONE — `PayloadDecoder<DataFields>` skips data payloads containing
embedded SOH or `=` bytes. Generator emits `FixDataFields.hpp` mapping
length tags to data tags from `<data>` elements in `protocol.xml`.
`emitDataSkip()` in `processBlock` parses the length value, emits a
synthetic data token, and advances the scan offset past the payload.
CTAD deduction guide preserves `PayloadDecoder decoder{...}` for
non-data-aware usage (deduces `NoDataFields`).
