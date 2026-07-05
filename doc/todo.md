TODO
-----

Unsupported FIX data types
-----

`DataModel.hpp` (`m_types`) currently maps `protocol.xml` primitive types to
`char`, `uint8`, `int32`, `uint32`, `int64`, `uint64`, `decimal`,
`timestamp`, and `string`. The following standard FIX data types have no
mapping/`Category` and cannot yet be used in `protocol.xml`:

- `LocalMktDate` — same wire format as `UTCDateOnly`, different semantic
  (local timezone). Can share the same `Category::UTCDateOnly` codec.
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

Session handling
-----

- Add runtime configuration of session context: memcpy(dst, src, 64)
calculate offset for remaining payload, use fixed length buffer, 
add compId max length to config

Continuous integration — hosted NEON runner
-----

The arm64 (NEON) CI leg currently runs on a self-hosted Apple Silicon
Mac (runner labels: self-hosted, macOS, ARM64). GitHub's free hosted
arm64 Linux runners (ubuntu-24.04-arm) are only free on *public* repos,
and this repo is private, so a hosted NEON leg would otherwise queue
forever waiting for a runner. To move the NEON leg onto a GitHub-hosted
runner instead of the local Mac:

- Make the repo public (then ubuntu-24.04-arm is free), OR enable paid
  GitHub-hosted arm64 runners (larger/arm64 hosted runners are billed on
  private repos; configure under Settings -> Actions -> Runners).
- In .github/workflows/ci.yml, change the arm64 matrix entry from
  `runner: [self-hosted, macOS, ARM64]` / `os: macos` to
  `runner: ubuntu-24.04-arm` / `os: linux`. 
- Decommission the self-hosted runner: remove it under Settings ->
  Actions -> Runners, and stop it on the Mac (`./svc.sh uninstall`, or
  stop `./run.sh`).
