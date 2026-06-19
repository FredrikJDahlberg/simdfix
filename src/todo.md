TODO
-----

1. FIXED keep decoder/encoder types separate
2. move decoding functions from utils to decoder
3. FIXED decide on API

       encoder.wrap(buffer)
           .one(1).two(2).three(3)
           .hops(3)
               .next().time(10).string("hepp")
               .next().time(20)
               .next().time(20).string("hepp")
4. ~~configure decoder/encoder with protocol constants.~~ done via
   `PayloadEncoder<Protocol, Target, Sender>` / `FixPayloadEncoder`.
5. ~~implement fixed decimal type for internal use (`float` / `Qty` / `Price` /
   `PriceOffset` / `Amt` / `Percentage` decoding/encoding without floating point).~~
   done via `FixedDecimal` (`utils/FixedDecimal.hpp`).
6. ~~add `FieldDecoder`/`FieldEncoder` support for `FixedDecimal`:
   - `FieldDecoder`: parse an ASCII decimal string (optional leading '-',
     optional '.' with fractional digits) into a `FixedDecimal`
     (mantissa/exponent from the digit string and fractional digit count).
   - `FieldEncoder`: write a `FixedDecimal` back to ASCII (sign, digits,
     '.' placement from the exponent).~~
   done via `FieldDecoder::convertToFixedDecimal` / `getFixedDecimal` and
   `FieldEncoder::encode<Tag>(FixedDecimal)` /
   `utils::fixedDecimalToAscii`.
   ~~- add a `Category` for decimal types and map `float` / `Qty` / `Price` /
     `PriceOffset` / `Amt` / `Percentage` to it in `DataModel.hpp` so they can
     be used in `protocol.xml`.~~
   done via `Category::Decimal` in `CodecTypes.hpp`; `decimal` primitive
   type registered in `DataModel.hpp`; generator emits `getFixedDecimal` /
   `encode<Tag>(FixedDecimal)` for `Decimal` fields.
7. Add price scaling support — `FixedDecimal` currently uses a fixed 10^-8
   scale; encoding always produces 8 fractional digits (e.g. `120.00000000`).
   Need configurable precision or trailing-zero stripping in
   `fixedDecimalToAscii` so prices encode cleanly (e.g. `120` or `120.50`).


FIXMEs/TODOs in source
-----

Collected from the source tree 2026-06-12. None remaining.

Unsupported FIX data types
-----

`DataModeiml.hpp` (`m_types`) currently maps `protocol.xml` primitive types to
`char`, `uint8`, `int32`, `uint32`, `int64`, `uint64`, `decimal`,
`timestamp`, and `string`. The following standard FIX data types have no
mapping/`Category` and cannot yet be used in `protocol.xml`:

- ~~`float` / `Qty` / `Price` / `PriceOffset` / `Amt` / `Percentage` — decimal
  (fixed/floating point) numbers; only integers are supported.~~
  `decimal` primitive type now supported via `Category::Decimal` /
  `FixedDecimal`. Additional FIX type aliases (`float`, `Qty`, `Price`,
  `PriceOffset`, `Amt`, `Percentage`) can be added as derived types in
  `protocol.xml` with `primitiveType="decimal"`.
- ~~`Boolean` — single-character 'Y'/'N'; would need a bool `Category` (currently
  falls back to `char`/`String`).~~
  Mapped as an enum in `protocol.xml` with values `Yes`=`Y`, `No`=`N`.
  Fields use `type="Boolean"` and go through the existing enum
  encode/decode path — no new `Category` needed.
- ~~`data` / `XMLData` / `Length`-prefixed raw bytes — see "Length-prefixed
  data types" below.~~
  Implemented via `DataDecoder` / `DataEncoder` classes
  (`decoder/DataDecoder.hpp`, `encoder/DataEncoder.hpp`). Uses the group
  pattern in `protocol.xml`: `<data name="XmlData" tag="213"
  counter="XmlDataLen" lengthTag="212"/>`. Generator emits `DataDecoder` /
  `DataEncoder` members and typed accessors. Limitation: the tokenizer
  still splits on SOH inside data payloads — works for text-safe content
  but not raw binary with embedded SOH (see tokenizer notes below).
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

Length-prefixed data types
-----

~~`data`, `XMLData`, and similar FIX types use a two-field pattern: a
`Length` tag (integer) followed by a `Data` tag whose value is exactly
that many raw bytes (may contain SOH, `=`, or any binary content).~~

Implemented:
- `DataDecoder` (`decoder/DataDecoder.hpp`) — wraps `FieldDecoder&`;
  `get<LengthTag, DataTag, Required, Parent>()` returns `DataResult`
  (`std::expected<std::span<const uint8_t>, Result::Values>`).
- `DataEncoder` (`encoder/DataEncoder.hpp`) — wraps `FieldEncoder&`;
  `encode<LengthTag, DataTag>(std::span<const uint8_t>)` writes
  `LengthTag=N SOH DataTag=<raw bytes> SOH`.
- `Category::Data` added to `CodecTypes.hpp`.
- Generator handles `<data>` elements (same pattern as `<group>`):
  emits `DataDecoder`/`DataEncoder` members and typed accessors.
- `protocol.xml` example: `<data name="XmlData" tag="213"
  counter="XmlDataLen" lengthTag="212" presence="optional"/>`.

### Remaining: tokenizer binary-safe data

`PayloadDecoder` still splits on every SOH unconditionally, so data
payloads containing embedded SOH or `=` bytes will be tokenized
incorrectly. Two options for a future fix:

1. **Post-tokenize fixup**: after tokenizing, scan for Length tags and
   merge the subsequent tokens into a single synthetic token spanning
   the declared byte count. Schema-agnostic; only runs for messages
   with data fields.
2. **Inline skip**: teach `processBlock` to recognise Length tags and
   skip N bytes without delimiter scanning. Faster but couples the
   tokenizer to the protocol schema.

Open issues in FieldEncoder
-----

`src/main/cpp/org/limitless/fix/encoder/FieldEncoder.hpp`:

1. ~~The `if constexpr (Required) { // check null value }` blocks in the `encode` overloads
   are stubs — required fields are never validated as present/non-null before encoding.~~
   Resolved: `Required` removed from non-enum overloads. Non-nullable types
   use concepts (`EncodableInteger`, etc.) and always encode. Nullable types
   use `EncodableNullableInteger` etc. and skip when null. Enum overloads
   retain `Required` — optional enums skip on `Null`, required enums assert.