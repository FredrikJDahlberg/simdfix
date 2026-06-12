TODO
-----

1. keep decoder/encoder types separate
2. move decoding functions from utils to decoder
3. keep encoding utils in encoder
4. decide on API

       encoder.wrap(buffer)
           .one(1).two(2).three(3)
           .hops(3)
               .next().time(10).string("hepp")
               .next().time(20)
               .next().time(20).string("hepp")

FIXMEs/TODOs in source
-----

Collected from the source tree 2026-06-12. None remaining.

Empty classes
-----

Encoder stubs with empty bodies, placeholders for the encoder work in
items 1–4 above:

1. `src/main/cpp/org/limitless/fix/encoder/FieldEncoder.hpp:13` — `FieldEncoder`
2. `src/main/cpp/org/limitless/fix/encoder/GroupEncoder.hpp:13` — `GroupEncoder`
3. `src/main/cpp/org/limitless/fix/encoder/PayloadEncoder.hpp:13` — `PayloadEncoder`
4. `src/main/cpp/org/limitless/fix/encoder/ComponentEncoder.hpp:13` — `ComponentEncoder`

(`AppHandler` in `MessageDecoderTest.cpp:276` is also empty, but intentionally —
it is a do-nothing test handler relying on the `MessageHandler` defaults.)