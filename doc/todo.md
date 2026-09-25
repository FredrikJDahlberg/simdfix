TODO
-----

Unsupported FIX data types
-----

`DataModel.hpp` (`m_types`) maps the spec's primitive types `char`, `uint8`,
`int32`, `uint32`, `int64`, `uint64`, `decimal`, `timestamp` (UTCTimestamp),
`timeonly` (UTCTimeOnly), `dateonly` (UTCDateOnly) and `string`; `<data>`
elements cover data and XMLData, and a spec's `<enum>`s cover char/String codes.
Most FIX types reduce to these: Qty, Price, PriceOffset, Amt, Percentage and float
to `decimal`; int, Length, TagNum, SeqNum, NumInGroup and DayOfMonth to the integer
types; Country, Currency, Exchange, Language, XID and XIDREF to `string`.

Not supported yet:

- `LocalMktDate` — same wire format as `UTCDateOnly`, different semantic
  (local timezone). Can share the same `Category::UTCDateOnly` codec.
- `LocalMktTime` — "HH:MM:SS[.sss]" in the market's local time. Same wire
  format as `UTCTimeOnly`, so it can share `Category::UTCTimeOnly`.
- `MonthYear` — "YYYYMM", "YYYYMMDD", or "YYYYMMwN" (6, 8, or 8 bytes).
  Variable-length; needs its own parser and a struct or integer
  representation.
- `TZTimeOnly`, `TZTimestamp` — time/timestamp with timezone offset
  suffix ("Z", "±HH" or "±HH:MM"). Extends the UTC parsers with offset
  extraction.
- `Tenor` — duration code ("D1", "W2", "M3", "Y1"). Small enough to
  represent as a string or a dedicated struct with unit+count.
- `MultipleCharValue`, `MultipleStringValue` — space-delimited multi-value
  fields. Would need a container return type (e.g. small vector or
  iterator) rather than a single scalar.
- `Boolean` as a type — today only as a spec-declared enum (`session.xml`'s
  `Boolean`, "Y"/"N"); no `bool` primitive that decodes to `bool`.

Supported only in part:

- Sub-millisecond `UTCTimestamp`/`UTCTimeOnly` — FIX allows 0, 3, 6, 9 (or 12)
  fractional-second digits. The parsers accept only none or 3 (`Conversions.hpp`:
  lengths 17/21 and 8/12) and reject microseconds and nanoseconds as invalid; the
  encoder writes milliseconds, and the decoded type is `std::chrono::milliseconds`.
  Needs parsing of 6/9 digits and a finer duration type (or a precision per field).

Session handling
-----

- Add runtime configuration of session context: memcpy(dst, src, 64)
calculate offset for remaining payload, use fixed length buffer, 
add compId max length to config
