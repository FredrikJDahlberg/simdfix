# Changelog

Every release of simdfix, newest first. A release's section is its GitHub Release notes.

Until 1.0, a minor release (0.x.0) may break the API and a patch release (0.x.y) does not.
The API is the headers outside `detail/`, the code the Generator emits, `simdfix_generate()`,
what an install contains, the XML spec format and the Generator's command line.
Breaking changes come with the steps to migrate.

`.github/tag-release.sh` turns the Unreleased section into the release's section.

## [Unreleased]

### Breaking

- **`simdifx` is now `simdfix`** in every path and namespace: headers are included as
  `org/limitless/simdfix/...` and the namespace is `org::limitless::simdfix`.
  Migrate by replacing `simdifx` with `simdfix` in includes, namespaces and CMake paths.
- **Messages are generated with `simdfix_generate()`.** The `SIMDFIX_SESSION_XML`, `SIMDFIX_APP_XML`
  and `SIMDFIX_CONFIG_XML` variables and the `GenerateMessages` target are gone, and linking
  `SimdFix::SimdFix` no longer brings in generated headers. Migrate:
  ```cmake
  # before
  set(SIMDFIX_APP_XML "${CMAKE_CURRENT_SOURCE_DIR}/app.xml")
  add_subdirectory(simdfix EXCLUDE_FROM_ALL)
  target_link_libraries(app PRIVATE SimdFix::SimdFix)
  add_dependencies(app GenerateMessages)
  # after
  add_subdirectory(simdfix EXCLUDE_FROM_ALL)
  simdfix_generate(AppMessages APP_XML app.xml)
  target_link_libraries(app PRIVATE AppMessages)
  ```
- **Include `<namespace as a path>/messages/FixMessages.hpp`** for a spec's messages, e.g.
  `org/limitless/simdfix/generated/messages/FixMessages.hpp`. `org/limitless/simdfix/Fix.hpp` now
  holds only the hand-written API.
- **`PayloadDecoder` and `encodeResend` are per spec.** The hand-written ones are now
  `decoder::BasicPayloadDecoder<Protocol, MaxFields, DataFields>` and `encoder::basicEncodeResend`.
  Each spec's generated namespace has `PayloadDecoder<Protocol>` and `encodeResend<Protocol>`, which
  fill in its field capacity and data fields. Code that uses the generated namespace, as the README
  examples do, keeps compiling. Migrate `decoder::PayloadDecoder<...>` to the generated
  `PayloadDecoder<...>`.
- **The decoder skips data fields by default.** `PayloadDecoder` takes the spec's data fields
  (e.g. `XmlDataLen`/`XmlData`) instead of none, so a payload holding SOH or `=` is read as one field.
  Pass `decoder::NoDataFields` as the second argument for the old behavior.
- **The decoder's trace is opt-in**: define `SIMDFIX_TRACE`. It used to print in every build without
  `NDEBUG`.
- **An absent repeating group reads as empty.** `GroupDecoder::wrap` no longer throws
  `std::invalid_argument`. `FieldDecoder::popGroupScope` is replaced by `truncateGroupScopes`, and
  `pushGroupScope` returns whether the scope was entered.
- **Generated headers carry a version check**: they fail to compile against the headers of a different
  simdfix release, which `org/limitless/simdfix/Version.hpp` identifies. Regenerate them after an
  upgrade; `simdfix_generate()` does so on the next build.
- **Calling the Generator directly:** its output directories must be `<root>/<namespace as a path>/messages`
  and `<root>/<namespace as a path>/config`, with `<root>` on the include path, and it writes a sixth
  header, `messages/FixMessages.hpp`. Prefer `simdfix_generate()`, which does this for you.
- **Installing** no longer installs generated headers. It installs the Generator
  (`libexec/simdfix`), the default specs (`share/simdfix`: `session.xml`, `config.xml`, `protocol.xml`)
  and `simdfix_generate()`, so a `find_package(SimdFix)` consumer generates its own messages.

### Added

- `simdfix_generate(<name> [APP_XML] [SESSION_XML] [CONFIG_XML] [NAMESPACE] [OUTPUT_DIR])`, in the
  source tree and in an install. Each spec can live in its own namespace, so one program can use several.
- The Generator's `--namespace <ns>` and `--version` options.
- A `VERSION` file; `org/limitless/simdfix/Version.hpp` (`SIMDFIX_VERSION`, `SIMDFIX_VERSION_MAJOR`, ...);
  a `SimdFixConfigVersion.cmake`, so `find_package(SimdFix 0.2)` accepts 0.2.x only.
- The headers compile with `-fno-exceptions`, which the build checks.
- A release process: `CHANGELOG.md`, `.github/tag-release.sh` and the `release` workflow, which tests,
  compares benchmarks with the previous release and publishes a source tarball.
- README sections on FetchContent, the compiler flags a consumer needs, and releases. CI builds the
  example against an installed simdfix too.

### Fixed

- Nested repeating groups: a nested group was read from the first outer entry whatever the current
  one was, and an outer entry's fields read wrongly once its nested group had been iterated.
- `encoder/ResendEncoder.hpp` is compiled by the header check.

### Performance

- Decoding is as fast as in 0.1.0 (within ±3% on the benchmarks) while skipping data fields by default:
  a message is scanned without data-field checks, and only one that holds a data field is corrected.

## [0.1.0] - 2026-09-25

The first tagged release: commit `4608793`, before release management existed.
