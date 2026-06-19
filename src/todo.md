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

`DataModel.hpp` (`m_types`) currently maps `protocol.xml` primitive types to
`char`, `uint8`, `int32`, `uint32`, `int64`, `uint64`, `decimal`,
`timestamp`, and `string`. The following standard FIX data types have no
mapping/`Category` and cannot yet be used in `protocol.xml`:

- ~~`float` / `Qty` / `Price` / `PriceOffset` / `Amt` / `Percentage` — decimal
  (fixed/floating point) numbers; only integers are supported.~~
  `decimal` primitive type now supported via `Category::Decimal` /
  `FixedDecimal`. Additional FIX type aliases (`float`, `Qty`, `Price`,
  `PriceOffset`, `Amt`, `Percentage`) can be added as derived types in
  `protocol.xml` with `primitiveType="decimal"`.
- `Boolean` — single-character 'Y'/'N'; would need a bool `Category` (currently
  falls back to `char`/`String`).
- `data` / `XMLData` / `Length`-prefixed raw bytes.
- `UTCTimeOnly`, `UTCDateOnly`, `LocalMktDate`, `MonthYear`, `TZTimeOnly`,
  `TZTimestamp`, `Tenor` — only the full `UTCTimestamp` (21-byte) format is
  supported via `Category::Timestamp`.
- `MultipleCharValue`, `MultipleStringValue` — space-delimited multi-value
  fields.

Open issues in FieldEncoder
-----

`src/main/cpp/org/limitless/fix/encoder/FieldEncoder.hpp`:

1. ~~The `if constexpr (Required) { // check null value }` blocks in the `encode` overloads
   are stubs — required fields are never validated as present/non-null before encoding.~~
   Resolved: `Required` removed from non-enum overloads. Non-nullable types
   use concepts (`EncodableInteger`, etc.) and always encode. Nullable types
   use `EncodableNullableInteger` etc. and skip when null. Enum overloads
   retain `Required` — optional enums skip on `Null`, required enums assert.