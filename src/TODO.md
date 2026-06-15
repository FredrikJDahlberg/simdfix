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


FIXMEs/TODOs in source
-----

Collected from the source tree 2026-06-12. None remaining.

Open issues in FieldEncoder
-----

`src/main/cpp/org/limitless/fix/encoder/FieldEncoder.hpp`:

1. The `if constexpr (Required) { // check null value }` blocks in the `encode` overloads
   are stubs — required fields are never validated as present/non-null before encoding.