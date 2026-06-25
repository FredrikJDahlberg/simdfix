TODO
-----

Unsupported FIX data types
-----

`DataModel.hpp` (`m_types`) currently maps `protocol.xml` primitive types to
`char`, `uint8`, `int32`, `uint32`, `int64`, `uint64`, `decimal`,
`timestamp`, and `string`. The following standard FIX data types have no
mapping/`Category` and cannot yet be used in `protocol.xml`:

- ~~`UTCTimeOnly`~~ — done. Primitive type `timeonly`, `Category::UTCTimeOnly`.
  Decodes "HH:MM:SS" / "HH:MM:SS.sss" to `std::chrono::milliseconds`
  (millis since midnight). Encodes via `encodeUTCTimeOnly`.
- ~~`UTCDateOnly`~~ — done. Primitive type `dateonly`, `Category::UTCDateOnly`.
  Decodes "YYYYMMDD" to `std::chrono::milliseconds` (millis since epoch
  at midnight UTC). Encodes via `encodeUTCDateOnly`.
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

- Logon/Logout handshake — initiate and accept FIX sessions, negotiate
  HeartBtInt, and exchange Logon/Logout messages.
- Heartbeat & TestRequest — send Heartbeat on the negotiated interval,
  respond to TestRequest with the matching TestReqID, and detect missing
  heartbeats.
- Sequence number tracking — maintain inbound/outbound MsgSeqNum,
  detect gaps, and trigger resend requests.
- ResendRequest / SequenceReset — handle gap-fill and full-reset
  scenarios for message recovery.
- Message persistence & replay — store outbound messages for possible
  resend and replay inbound messages after a reconnect.
- Duplicate & PossDupFlag handling — detect and suppress duplicate
  messages, set PossDupFlag / PossResend on retransmissions.
- Keep-alive periods — drive the `keepAlive` timer tick from the configured
  heartbeat period (`DefaultHeartbeatPeriod`, overridable via the `Builder`):
  emit a Heartbeat when the link is idle, escalate to a TestRequest when a
  beat is overdue, and mark the session stale after repeated misses.
- Message handling — route decoded messages by MsgType: classify session vs
  application messages with `isSessionMessage`, handle admin messages in the
  session layer and forward application messages to the handler below.

Storage
-----

- Memory-mapped message store — a `FixStorageStrategy` backed by an mmap'd
  journal file with an in-memory sequence-number -> file-position index (offset
  + length), so a single message or a resend range is located by MsgSeqNum
  without scanning. Append writes the encoded bytes to the journal and records
  the offset; reads return a flyweight `StoredMessage` viewing the mapped pages
  directly (zero-copy, no `read()` syscall — paging/readahead is the OS's job).
- Read API: flyweight getMessage + range cursor — keep `getMessage(seqNum)`
  returning a single flyweight `StoredMessage` (valid until the next store
  call), and replace `getMessages(from, to) -> std::span<const StoredMessage>`
  with a cursor/visitor that yields one `StoredMessage` at a time — a span of N
  can't express N simultaneously-valid flyweights. e.g.
  `replay(from, to, visitor)`, or
  `for (auto c = store.replay(from, to); c.next(); ) send(c.current());`.
- Batched (packet) range reads — the cursor lets the store own the buffer and
  refill transparently: read a packet of messages per I/O and re-point the
  flyweight within it, so the expensive reads are amortized while the caller
  still sees one message at a time. (`MemoryStorage` implements the cursor
  trivially over its resident vector.)
- Aeron Archive backend — a `FixStorageStrategy` over Aeron Archive's
  record/replay (C++ client). Append = `Publication::offer` to a recorded
  publication; capture the pre-offer `position()` as the message start. Aeron
  addresses by stream byte-position, not MsgSeqNum, so keep the same
  seqnum -> position index (`MsgSeqNum -> {recordingId, startPos, length}`) — the
  mmap'd index above, re-cast. Range read = `startReplay(recordingId, fromPos,
  len, ...)` then poll the replay Image with a fragment handler: the handler's
  `(buffer, offset, length)` is a flyweight valid only during the callback and
  `poll()` delivers a term (packet) at a time, so the flyweight + batched-cursor
  contracts map natively (no copies). `clear()` = `truncateRecording` / new
  recording per session. Use `ReplayMerge` for live catch-up on reconnect.
  Defers to the fault-tolerance bullet: `offer()` back-pressure -> fallible
  append, durable-before-wire via `getRecordingPosition`, and Aeron Cluster
  (Raft) for failover.
- Fault tolerance (later) — fallible returns (`Result`/`expected`),
  durable-append-before-wire, recovery metadata (last seqNum / range as source
  of truth after failover), and replication are deferred.

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
  `runner: ubuntu-24.04-arm` / `os: linux`. Both legs then share the
  Linux path (apt + clang + g++-14 + the __cpp_concepts workaround); the
  macOS Homebrew "Install dependencies" step becomes unused and can be
  removed.
- Decommission the self-hosted runner: remove it under Settings ->
  Actions -> Runners, and stop it on the Mac (`./svc.sh uninstall`, or
  stop `./run.sh`).
- No source changes required — hosted arm64 Linux uses the same Clang +
  libstdc++ toolchain as the x86 leg, already covered by the C++23
  portability fixes (forced standard, __cpp_concepts guard, explicit
  includes).
