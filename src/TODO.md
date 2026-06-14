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
4. configure decoder/encoder with protocol constants.


FIXMEs/TODOs in source
-----

Collected from the source tree 2026-06-12. None remaining.

Empty classes
-----

Encoder stubs with empty bodies, placeholders for the encoder work in
items 1–4 above:

1. `src/main/cpp/org/limitless/fix/encoder/GroupEncoder.hpp` — `GroupEncoder::wrap` is a stub
   (comment "encode TAG = VALUE"), `next()` only increments `m_count`.
2. `src/main/cpp/org/limitless/fix/encoder/PayloadEncoder.hpp` — constructor only, no encode API yet.

Open issues in FieldDecoder
-----
1. Signed types

Open issues in FieldEncoder
-----

`src/main/cpp/org/limitless/fix/encoder/FieldEncoder.hpp`:

1. The `std::string_view` `encode` overload (line ~104) is a stub that returns 0 and does
   not write anything.

2. Signed types 