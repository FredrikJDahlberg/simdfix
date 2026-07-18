# todo

## Runtime-built FIX header, replacing compile-time `FixedString` prefix encoding

`PayloadEncoder<Protocol, Target, Sender>` (`src/main/cpp/org/limitless/simdifx/encoder/PayloadEncoder.hpp`)
currently takes `BeginString`/`SenderCompID`/`TargetCompID` as `FixedString` non-type template
parameters (`Types.hpp`) and builds the `8=/9=/35=/49=/56=` header with five separate
`FieldEncoder::encode<Tag, Value>()` calls (`FieldEncoder.hpp`), each doing its own `memcpy` of
compile-time string literals. This means a new `PayloadEncoder`/`Session` template instantiation
per distinct Protocol/Sender/Target combination.

Planned replacement: build the header once per session into a fixed-size buffer on
`SessionContext`, then do a single `memcpy` of that buffer per outbound message.

### Design

- `SessionContext` (`Types.hpp`) gains:
  - `std::array<uint8_t, MaxHeaderLength> m_headerBuffer{}` — value-initialized (zeroed).
  - `uint32_t m_headerLength`, `m_prefixOffset`, `m_bodyLengthOffset`, `m_msgTypeOffset`,
    `m_headerBodyLength`.
- Layout written once into `m_headerBuffer` by a new, `[[gnu::noinline]]`
  `SessionContext::buildHeaderBuffer()` (cold path, runs once at session construction):
  ```
  [ 8=Protocol␁ ] [ 9=0000␁ ] [ 35=?␁ ] [ 49=Sender␁ ] [ 56=Target␁ ]
   ^static          ^zeroed    ^zeroed   ^static         ^static
                     value      value
  ```
  Tag labels (`"9="`, `"35="`) and SOH delimiters are static and written into the buffer;
  only the BodyLength digits and MsgType byte are left zeroed as placeholders. `m_prefixOffset`
  marks the start of that variable region (right after the BeginString field).
- Hot path `PayloadEncoder::wrap()` becomes a single
  `std::memcpy(dst, ctx.m_headerBuffer.data(), sizeof(ctx.m_headerBuffer))` — using
  `sizeof(m_headerBuffer)` (compile-time constant) rather than the runtime `m_headerLength`, so
  the copy size is known at compile time and the call can be fully inlined instead of lowered to
  a runtime-sized `memcpy`. The few extra zero bytes copied past `m_headerLength` are harmless:
  they're immediately overwritten once body encoding starts at `offset() == m_offset +
  ctx.m_headerLength`.
  Mark `wrap()` `[[gnu::always_inline]]`.
- `encode()` no longer builds the header; it just patches the two placeholder slots in place
  once BodyLength/MsgType are known (`m_bodyLengthOffset`, `m_msgTypeOffset`), then proceeds with
  checksum computation as before.
- `PayloadEncoder`/`FixPayloadEncoder` drop the `Protocol`/`Target`/`Sender` NTTPs entirely —
  they take a `const SessionContext&` instead. `Session<Protocol, Sender, Target, ...>` may keep
  its own template parameters for other reasons, but no longer needs to thread them through the
  encoder.

### Why

- Collapses N compile-time template instantiations of `PayloadEncoder`/`FixPayloadEncoder` (one
  per Protocol/Sender/Target combination) down to one type, parameterized at runtime via
  `SessionContext`.
- Keeps the hot send path to exactly one inlined, fixed-size `memcpy` plus two small direct byte
  writes — no per-field function calls, no branching.
- Isolates all string-length-dependent work (building the header, `memcpy`ing Sender/Target text)
  in `buildHeaderBuffer()`, called once per session and explicitly kept out of the inlined hot
  path (`[[gnu::noinline]]`).

### Open questions

- Confirm `MaxHeaderLength` bound (64 bytes) covers realistic CompID lengths with margin.
- `SessionContext` today is also used on the inbound/decode validation path
  (`m_protocol`/`m_senderCompId`/`m_targetCompId` checked by generated `validate()` methods) —
  confirm bundling the outbound header buffer into the same struct is desired versus a separate
  encode-side context.

## Configurable output namespace / include prefix for the FIX generator

`FixGenerator.cpp` (`src/generator/org/limitless/generator/simdifx/`) hardcodes the vendor
namespace into everything it emits: `namespace org::limitless::simdifx::generated::messages`
(≈ line 228) / `::generated::config`, and `#include "org/limitless/simdifx/generated/..."`
prefixes (≈ lines 253–254). It's invoked positionally with no namespace argument —
`Generator <session.xml> <msgOutDir> <config.xml> <cfgOutDir> <appXml>` (`CMakeLists.txt:167`).

Consequence: a downstream project that runs this Generator to produce its **own** FIX codecs
from its own XML (e.g. sibling `phixeron`, which generates into `phixeron_generated/` and includes
via `org/limitless/simdifx/generated/...`) ends up with its own generated code sitting under
simdfix's `org::limitless::simdifx::generated` namespace instead of its own
(e.g. `org::limitless::phixeron::fix::generated`). This is a **naming** problem only — the code is
correct and there is no header collision (simdfix ships no generated headers on the include path) —
but downstream code reads as if the FIX messages belong to the vendor.

### Design

- Add a base-namespace argument to the Generator (e.g. a trailing positional `<baseNamespace>`
  or a `--namespace org::limitless::phixeron::fix` flag), defaulting to
  `org::limitless::simdifx` so existing callers/behaviour are unchanged.
- Derive both from that single value:
  - the emitted `namespace <base>::generated::{messages,config}` declarations, and
  - the emitted `#include "<base-as-path>/generated/..."` prefixes,
  so the include directory layout the caller places on its include path matches the namespace.
- Thread it through the two emission sites (message headers ≈ line 228, config/engine includes
  ≈ lines 252–254) rather than repeating the literal.

### Why

- Lets each consumer's generated FIX codecs live under its own namespace, so downstream source
  reads `org::limitless::phixeron::fix::generated::messages::Side` instead of borrowing the
  vendor's name. simdfix's own build keeps the default and is unaffected.

### Open questions

- One base namespace argument mapped to a path, or two independent knobs (C++ namespace vs.
  include-path prefix) in case a consumer wants them to differ?
- Keep the default at `org::limitless::simdifx` (backward-compatible) vs. make the argument
  required to force every caller to be explicit.
