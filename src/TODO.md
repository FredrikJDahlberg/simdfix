TODO
-----

1. keep decoder/encoder types separate
2. move decoding functions from utils to decoder
3. decide on API

       encoder.wrap(buffer)
           .one(1).two(2).three(3)
           .hops(3)
               .next().time(10).string("hepp")
               .next().time(20)
               .next().time(20).string("hepp")
4. ~~configure decoder/encoder with protocol constants.~~ done via
   `PayloadEncoder<Protocol, Target, Sender>` / `FixPayloadEncoder`.
5. implement fixed decimal type for internal use (`float` / `Qty` / `Price` /
   `PriceOffset` / `Amt` / `Percentage` decoding/encoding without floating point).


FIXMEs/TODOs in source
-----

Collected from the source tree 2026-06-12. None remaining.

Unsupported FIX data types
-----

`DataModel.hpp` (`m_types`) currently maps `protocol.xml` primitive types to
`char`, `uint8`, `int32`, `uint32`, `int64`, `uint64`, `timestamp`, and
`string`. The following standard FIX data types have no mapping/`Category`
and cannot yet be used in `protocol.xml`:

- `float` / `Qty` / `Price` / `PriceOffset` / `Amt` / `Percentage` — decimal
  (fixed/floating point) numbers; only integers are supported.
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

1. The `if constexpr (Required) { // check null value }` blocks in the `encode` overloads
   are stubs — required fields are never validated as present/non-null before encoding.