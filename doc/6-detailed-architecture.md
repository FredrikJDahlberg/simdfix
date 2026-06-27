# Chapter 6: Detailed Component Architecture

This chapter provides implementation-level detail for each major component in the gateway. The high-level architecture is in Chapter 2; this chapter assumes that material and goes deeper into the internal mechanics, data structures, and invariants of each subsystem.

---

## 6.1 Clustered Service Sequencer

### 6.1.1 Role and Positioning

The Clustered Service Sequencer is the component that imposes a **total, deterministic order** on every message entering the gateway, regardless of which FIX session, which Ingress node, or which C++ process it originated from. It is the boundary between the unordered, concurrent world of TCP ingress and the ordered, replayable world of the application engine.

No application-level processing — risk checks, order routing, position accounting — occurs before the sequencer commits a message. This is a hard architectural invariant: the sequencer is the single gate through which all state-changing events pass. It has two consequences that are fundamental to the gateway's correctness:

1. **Cross-node consistency.** Every cluster node applies messages in identical order. The C++ AppWorkers on all three nodes receive the same command stream and therefore maintain identical application state. There is no divergence between nodes while any quorum member is alive.

2. **Exact-once failover.** When a new leader is elected, it resumes processing from the exact log position where the old leader left off. No committed message is lost or applied twice at the cluster boundary.

The sequencer is implemented in Java 21 using Aeron Cluster and consists of two cooperating components: the `ConsensusModule` (part of the Aeron Cluster library) and `FixClusteredService` (gateway application code that implements the `ClusteredService` interface).

---

### 6.1.2 Inbound Streams

The sequencer receives messages from four distinct Aeron IPC streams, each carrying a different category of event:

| Stream | Source | Content | Format |
|--------|--------|---------|--------|
| 1 | C++ Ingress (FIX Session, Core 2) | Validated inbound FIX frames from buy-side clients | SBE-encoded per frame |
| 3 | C++ AppWorker (Core 7) | Application feedback: sent-reports, reject commands | SBE-encoded |
| 5 | C++ Egress (Core 9) | Execution reports received from the sell-side gateway | SBE-encoded |
| 7 | C++ Risk Thread (Core 8) | Risk check results: approved or rejected, with correlationId | SBE-encoded |

All four streams deliver SBE-encoded payloads. No raw FIX bytes enter the sequencer on any stream; the FIX Session component (Core 2) decodes the FIX wire format into a typed SBE message before publication on stream 1. All other streams carry application-layer SBE messages that never had a FIX representation.

In a multi-node deployment each Ingress node has its own stream 1 publication. The Aeron Cluster ingress channel merges all of them into a single ordered log: the `ConsensusModule` acts as the merge point, sequencing offers from all connected Ingress publications into one commit stream.

---

### 6.1.3 Raft Log and the ConsensusModule

The `ConsensusModule` is Aeron Cluster's implementation of the Raft consensus algorithm. It manages the replicated log that underpins the sequencer's ordering guarantee.

**Publication path.** When a C++ process publishes on one of the four inbound streams, the publication lands in the Aeron IPC ring buffer managed by the local Media Driver. The `ConsensusModule` polls this ring buffer on a dedicated conductor thread (one of Cores 3–6) and, on receiving a message, constructs a Raft log entry:

```
LogEntry {
    term:             uint64   — current Raft term
    clusterPosition:  uint64   — byte offset in the log (global sequence number)
    timestamp:        int64    — cluster.timeMs() at the moment of entry creation
    sessionId:        uint64   — Aeron session identifier of the source stream
    payload:          byte[]   — the SBE message verbatim
}
```

**Replication.** The leader sends the entry to all followers via the Aeron Cluster replication channel (a dedicated Aeron UDP publication, not an IPC stream). Each follower appends the entry to its local log file on NVMe and sends an acknowledgement. The leader waits for acknowledgement from at least one follower (quorum of 2/3) before marking the entry committed.

**Commit notification.** Once committed, `ConsensusModule` delivers the entry to `FixClusteredService.onSessionMessage()` on every node — leader and followers alike — in commit order. The `clusterPosition` field in the `Header` parameter is the global sequence number assigned at the moment the entry was created on the leader.

The Raft log is append-only and is never modified after an entry is committed. The log is stored on NVMe as a series of fixed-size term files managed by Aeron Archive. Entries written by a previous term that were not committed (i.e., the leader crashed before quorum) are silently discarded by the new leader's first no-op entry, which establishes the boundary between the previous term and the current one.

---

### 6.1.4 Global Sequence Number Assignment

Every message committed to the Raft log receives a **`clusterSessionPosition`**: a monotonically increasing 64-bit byte offset in the log. This is the gateway's global sequence number.

**Properties:**

- **Monotonically increasing.** Each successive committed entry has a strictly higher `clusterSessionPosition` than all prior entries. There are no gaps: if entry A has position *P* and entry B is the next committed entry, then B's position is *P* + *sizeof(A's log record)*.
- **Globally unique.** The position is unique across all time, all terms, and all leader elections. A position written in term *T* is never reused in term *T+1* because the Raft log is append-only and the new leader continues from the highest committed position.
- **Stable under failover.** Committed entries are never rewritten. A `clusterSessionPosition` that was committed before a crash is identical on the new leader and on every follower.
- **Cross-stream.** Positions are assigned across all four inbound streams in a single shared sequence. Two messages arriving on streams 1 and 3 simultaneously receive adjacent positions in the order the `ConsensusModule` processed them on the leader.

**Uses:**

- **Idempotency key for risk responses.** The Risk Thread publishes each risk result on stream 7 tagged with the `clusterSessionPosition` of the order it is responding to. The cluster commits the result and emits `RISK_APPROVED` or `RISK_REJECTED` on stream 2 carrying the same position. The AppWorker uses this position to match the result to the original order with no ambiguity, even after failover.
- **Failover recovery.** On process restart, the cluster replays the Raft log from the last snapshot position. Outstanding risk requests are identified as `FORWARD_APP` log entries with no corresponding committed `RISK_APPROVED` or `RISK_REJECTED` at a higher position. The new Risk Thread re-submits them using the original `clusterSessionPosition` as the correlation anchor.
- **Resend deduplication.** `MemoryStorage` indexes outbound messages by `clusterSessionPosition`, allowing the cluster to detect and suppress duplicate `SEND` commands that arrive because an AppWorker replayed from an earlier log position.

---

### 6.1.5 Timestamp Assignment

Every committed log entry carries a **`cluster.timeMs()`** timestamp — the cluster's logical time at the moment the `ConsensusModule` created the log entry on the leader.

**What it is.** `cluster.timeMs()` is not wall-clock time. It is a Raft-committed logical timestamp that advances in lockstep with the Raft heartbeat. The `ConsensusModule` samples the system clock when it writes each heartbeat log entry and embeds the value. Ordinary message entries inherit the timestamp from the most recent committed heartbeat. This means timestamps advance in discrete steps (heartbeat interval, default 250 ms) rather than continuously.

**Why wall-clock is forbidden.** Inside any `ClusteredService` callback (`onSessionMessage`, `onTimerEvent`, `onRoleChange`), all access to wall-clock time is prohibited. This is a determinism requirement: if the leader and a follower called `System.currentTimeMillis()` at different instants, they would see different values and their state machines would diverge. `cluster.timeMs()` is identical on every node for every callback invocation because it is derived from committed log entries, not from the local system clock.

**Timer scheduling.** Heartbeat timers, test-request timers, and resend timers are all registered via `cluster.scheduleTimer(correlationId, deadlineMs)` where `deadlineMs` is expressed in terms of `cluster.timeMs()`. When the cluster's logical time advances past a timer's deadline, `FixClusteredService.onTimerEvent(correlationId, timestamp)` fires on every node in the same commit order as all other events. The timer fires deterministically and simultaneously on all nodes.

**Relationship to FIX timestamps.** FIX message fields such as `SendingTime` (tag 52) and `TransactTime` (tag 60) carry the client's wall-clock time as text inside the FIX message. These are preserved verbatim in the SBE payload and passed through to the application layer. The cluster does not modify or validate them. The cluster's own `cluster.timeMs()` is separate and is used only for internal timer management and log entry ordering.

---

### 6.1.6 FixClusteredService: Session State Machine

`FixClusteredService` implements the `ClusteredService` interface and contains all gateway application logic that must be replicated across the cluster. It is the only place in the system where FIX session state is durably maintained.

**State held by FixClusteredService:**

```
inboundSeqNum:    int64       — last FIX inbound sequence number committed to the log
outboundSeqNum:   int64       — last FIX outbound sequence number committed to the log
sessionPhase:     SessionPhase — { DISCONNECTED, LOGON_PENDING, ACTIVE, LOGOUT_PENDING }
memoryStorage:    MemoryStorage — circular buffer of last 2500 outbound SBE messages
heartbeatTimer:   long        — correlationId of the active heartbeat timer
testReqTimer:     long        — correlationId of the active test-request timer (if any)
```

All of this state is replicated to every follower as it changes and is serialised into the cluster snapshot for fast recovery after restart.

**Session phase transitions.** The session state machine advances on committed messages:

```
DISCONNECTED
    ├─ on CONNECT command              → LOGON_PENDING
    └─ (stays DISCONNECTED on all other events)

LOGON_PENDING
    ├─ on committed Logon (tag 35=A)   → ACTIVE
    │    emit: SEND Logon-Ack (stream 2), start heartbeat timer
    ├─ on heartbeat timer expiry       → send TestRequest; start test-req timer
    ├─ on test-req timer expiry        → emit DISCONNECT; → DISCONNECTED
    └─ on DISCONNECT command           → DISCONNECTED

ACTIVE
    ├─ on committed app message (D,F,G,…) → emit FORWARD_APP (stream 2)
    ├─ on committed Heartbeat (tag 35=0)  → reset heartbeat timer
    ├─ on committed TestRequest           → emit SEND Heartbeat (stream 2)
    ├─ on committed ResendRequest         → service from MemoryStorage
    │    emit: RESEND for app messages, GAP_FILL for session messages
    ├─ on committed Logout (tag 35=5)     → emit SEND Logout-Ack; → LOGOUT_PENDING
    ├─ on heartbeat timer expiry          → emit SEND TestRequest; start test-req timer
    ├─ on test-req timer expiry           → emit SEND Logout; → LOGOUT_PENDING
    └─ on DISCONNECT command              → → DISCONNECTED

LOGOUT_PENDING
    ├─ on committed Logout-Ack            → → DISCONNECTED
    ├─ on logout timer expiry             → → DISCONNECTED (force)
    └─ on DISCONNECT command              → → DISCONNECTED
```

**Sequence number validation.** On each inbound committed message the service compares the FIX `MsgSeqNum` (tag 34, extracted from the SBE payload) against `inboundSeqNum + 1`:

- **Match**: normal case. Increment `inboundSeqNum`. Proceed with phase transition.
- **Too high (gap)**: emit `SEND ResendRequest` on stream 2. Do not advance `inboundSeqNum` until the gap is filled.
- **Too low with `PossDupFlag=Y`** (tag 43): duplicate retransmission. Ignore silently. Do not advance `inboundSeqNum`.
- **Too low without `PossDupFlag`**: sequence reset. Emit `SEND Logout` and transition to LOGOUT_PENDING.

---

### 6.1.7 MemoryStorage and ResendRequest Servicing

`MemoryStorage` is a circular buffer of the last 2500 SBE-encoded outbound messages. It is updated every time the cluster emits a `SEND` command on stream 2: the SBE payload is written into the circular buffer indexed by `outboundSeqNum`.

When a `ResendRequest` (tag 35=2) arrives, `FixClusteredService` scans `MemoryStorage` for the requested sequence range:

- **Application messages** (ExecutionReport, OrderCancelReject): emitted as `RESEND` commands on stream 2, each carrying `PossDupFlag=Y` (tag 43) added to the SBE payload by the cluster before publication.
- **Session messages** (Heartbeat, TestRequest, Logon, etc.): never retransmitted individually. The entire run of session messages within the requested range is replaced by a single `GAP_FILL` command that instructs the AppWorker's FIX Session component to emit a FIX SequenceReset-GapFill (tag 123=Y).

If the requested range extends beyond what `MemoryStorage` holds, the cluster falls back to Aeron Archive for the older entries. Archive reads are synchronous and are only triggered for ranges older than 2500 messages — a condition that should not occur in normal operation.

---

### 6.1.8 Multi-Session Sequencing

A single `FixClusteredService` instance handles all connected FIX sessions across all Ingress nodes. Each FIX client session is identified by an Aeron `sessionId` embedded in the SBE message header on stream 1.

The service maintains a `Map<Long, SessionState>` keyed by `sessionId`. Each entry holds the per-session `inboundSeqNum`, `outboundSeqNum`, `sessionPhase`, and associated timers. Session entries are created on `CONNECT` and removed on `DISCONNECT`.

Total ordering across sessions is provided by the Raft log: if two Ingress nodes publish simultaneously on their respective stream 1 publications, the `ConsensusModule` on the leader serialises them into adjacent log entries. The order in which they are serialised is the authoritative order for the entire cluster lifetime — followers apply them in the same order, and the new leader after a failover replays them in the same order.

This means that even in a multi-node Ingress deployment, there is no cross-session race: every FIX message from every client is committed to a unique, globally ordered position before any application processing occurs.

---

### 6.1.9 Output: Stream 2 Command Generation

After processing each committed log entry, `FixClusteredService` emits zero or more commands on Aeron IPC stream 2 toward the C++ AppWorker. Commands are emitted in this fixed binary envelope:

```
Byte 0:      uint8    discriminator (CMD_FORWARD_APP, CMD_SEND, CMD_RESEND, …)
Bytes 1–8:   uint64   clusterSessionPosition of the originating log entry
Bytes 9–N:   byte[]   SBE payload (command-specific)
```

The `clusterSessionPosition` is included in every command envelope so the AppWorker can always correlate a command back to the Raft log entry that produced it. This is the anchor for idempotency, deduplication, and failover re-dispatch.

The full command set and their triggers are described in §2.5.4. Key points regarding command generation order:

- Commands are emitted **strictly in commit order**. The AppWorker's duty cycle processes them in the order they arrive; it never reorders or batches across multiple log positions.
- A single committed log entry may produce **multiple commands** on stream 2. A `ResendRequest` that spans 10 messages produces 10 `RESEND` (or `GAP_FILL`) commands in sequence.
- `FORWARD_APP` is emitted only for application-layer FIX messages (MsgType D, F, G, 8, 9, …). Session-layer messages (A, 0, 1, 2, 5, …) are handled entirely within `FixClusteredService` and do not produce a `FORWARD_APP` command.

---

### 6.1.10 Snapshot and Log Replay

**Snapshot trigger.** The cluster takes a snapshot of `FixClusteredService` state periodically — by default, every N committed entries (configurable) or on a manual trigger — and writes it to Aeron Archive on the local NVMe. The snapshot position is the `clusterSessionPosition` of the last committed entry at the time of the snapshot.

**Snapshot contents.** The snapshot serialises the entire `FixClusteredService` state:

```
for each sessionId in sessionMap:
    sessionId:         uint64
    inboundSeqNum:     int64
    outboundSeqNum:    int64
    sessionPhase:      uint8
    heartbeatTimer:    int64   (correlationId, 0 if none)
    testReqTimer:      int64   (correlationId, 0 if none)
    memoryStorage:     MemoryStorage (2500 × SBE message slots)
```

**Recovery sequence.** On restart (fresh start or failover), each cluster node:

1. Loads the most recent snapshot from Aeron Archive into `FixClusteredService`.
2. Replays Raft log entries from the snapshot position to the end of the committed log, re-running `onSessionMessage()` for each. This fast-forwards `FixClusteredService` state to the exact point of the last committed entry.
3. Once replay is complete, the node is eligible to participate in leader election.
4. On becoming leader, emits `CONNECT` on stream 2 to activate the C++ AppWorker.

Log replay is bounded by the distance between the last snapshot and the end of the committed log. With frequent snapshots the replay window is small; in the worst case (no snapshot ever taken), the entire log is replayed from the beginning.

---

### 6.1.11 Determinism Constraints

All code executing inside `ClusteredService` callbacks is subject to strict determinism requirements. Any operation that could produce different results on different nodes will cause state divergence and corrupt the cluster. The following operations are prohibited inside any `ClusteredService` callback:

| Prohibited operation | Reason | Safe alternative |
|----------------------|--------|-----------------|
| `System.currentTimeMillis()` / `System.nanoTime()` | Returns different values on different nodes | `cluster.timeMs()` |
| `new Random()` / `Math.random()` | Non-deterministic seed | Seeded PRNG with deterministic state in snapshot |
| `HashMap` / `HashSet` with non-deterministic iteration order | JVM identity hashCode is non-deterministic | `LinkedHashMap`, `TreeMap`, or sorted collections |
| Blocking I/O (file read, network call) | Blocks one node longer than another | None — I/O must happen outside callbacks |
| `Thread.sleep()` or any blocking wait | Same as blocking I/O | None |
| Reading environment variables or system properties at callback time | May differ between nodes | Read at startup, store in snapshot-safe state |
| Floating-point arithmetic with JVM JIT non-determinism | JIT may produce different rounding on different compilations | Integer arithmetic only, or `StrictMath` |
| Wall-clock-based timer scheduling | `System.currentTimeMillis()` is forbidden | `cluster.scheduleTimer()` with `cluster.timeMs()` deadline |
| Logging inside hot-path callbacks | May block on I/O | Log asynchronously via a dedicated log buffer outside the callback |

Violations of these constraints cause follower state to diverge from the leader silently. Divergence is detected only at the next snapshot comparison (if snapshot verification is enabled) or at the next leader election when the new leader's state does not match the followers'. For this reason the `FixClusteredService` test suite includes a determinism harness that runs the same event sequence on two independent instances and asserts byte-identical state after each committed entry.

---

## 6.2 C++ FIX Session Handler

### 6.2.1 Design Principle: Cluster-Driven Proxy

The C++ FIX Session component on Core 2 is not an autonomous state machine. It is a cluster-driven proxy: it performs FIX framing and encoding at the wire boundary and forwards all session decisions to the Aeron Cluster, which is the sole location of the session state machine.

Every piece of local state the handler maintains is a **shadow** of the cluster's committed state. It is updated exclusively by commands arriving on stream 6 — commands that are themselves derived from Raft-committed log entries. The handler never makes a session decision independently. It never decides to send a Logon-Ack, a Heartbeat, or a SequenceReset on its own initiative. It only acts when the cluster tells it to, and only in the way the cluster specifies.

This constraint is what makes failover seamless. When the cluster elects a new leader, `FixClusteredService.onRoleChange(LEADER)` fires with the full committed session state already in place. The new leader's C++ handler starts consuming stream 6 from that point and immediately knows the correct session phase, the correct sequence positions, and the correct outbound queue — without any recovery step of its own.

### 6.2.2 What the Handler Does Not Do

To make the boundary precise, the following are explicitly **not** responsibilities of the C++ session handler:

| Not the handler's responsibility | Where it lives |
|----------------------------------|----------------|
| Sequence number validation (tag 34 in range?) | `FixClusteredService.onSessionMessage()` |
| Gap detection and ResendRequest generation | `FixClusteredService` |
| Heartbeat and test-request timer management | `cluster.scheduleTimer()` in `FixClusteredService` |
| Session phase transitions (DISCONNECTED → ACTIVE) | `FixClusteredService` session state machine |
| Logon parameter validation (CompID match, HeartBtInt) | `FixClusteredService` |
| MemoryStorage maintenance (outbound resend buffer) | `FixClusteredService` |
| Decision to send or not send any outbound message | `FixClusteredService` command emission |

The handler never reads or increments a sequence number counter of its own. It encodes whatever the cluster commands it to encode, with whatever sequence number the cluster supplies in the SBE payload.

### 6.2.3 Structural Admission (Pre-Cluster Validation)

The handler does perform one category of local check before offering a frame to the cluster: **structural admission**. This is not session logic — it is basic sanity checking to prevent malformed data from entering the Raft log:

- FIX framing integrity: `BeginString` (tag 8) present, `BodyLength` (tag 9) matches actual field length, `CheckSum` (tag 10) is the last field.
- Checksum arithmetic: the sum of all bytes before tag 10 modulo 256 must equal the value in tag 10.
- Maximum frame size: frames exceeding a configured byte limit are dropped without being offered to the cluster.
- Connection-level admission: source `sessionId` must correspond to a known, accepted TCP connection.

None of these checks involve session state. A frame that passes admission is offered to stream 1 unconditionally, regardless of MsgType, sequence number, or current session phase. The cluster determines whether the frame is acceptable in the current session context.

### 6.2.4 Local Shadow State

The handler maintains a small amount of local state that mirrors the cluster's committed state. This state is used only for operational purposes (routing outbound messages to the correct TCP connection) and for the admission filter:

```
sessionId:      uint64   — Aeron session identifier, set on TCP accept
fd:             int      — client TCP file descriptor
peerAddress:    sockaddr — remote peer address
shadowPhase:    SessionPhase — current phase as last reported by the cluster
```

`shadowPhase` is the only piece of shadow state that influences the handler's behaviour: if the cluster has not yet committed a `CONNECT` for this session, the handler does not offer inbound frames on stream 1 (because the cluster has not yet assigned a `sessionId` for this TCP connection). Once `CONNECT` is received, all frames are forwarded. `shadowPhase` is not used for any other filtering decision.

### 6.2.5 Outbound Processing: Command → FIX Frame

When a command arrives on stream 6, the handler executes the following fixed sequence:

```
1. Decode SBE discriminator byte from command header.
2. Decode clusterSessionPosition from command header (for diagnostic logging only).
3. Decode SBE payload into FIX internal format fields.
4. If shadowPhase update is implied by this command type, update shadowPhase.
5. FIX-encode the outbound frame using simdfix PayloadEncoder:
       a. Write Standard Header fields (BeginString, BodyLength placeholder,
          MsgType, SenderCompID, TargetCompID, MsgSeqNum, SendingTime).
       b. Write body fields from the SBE payload.
       c. Write Trailer (CheckSum).
       d. Back-fill BodyLength.
6. Write encoded byte span and destination fd to TX SPSC → reactor.
```

Step 3 (SBE → FIX internal format) is required because `PayloadEncoder` operates on FIX internal format fields, not directly on SBE. This is the translation step noted in §2.4.4. An alternative design (a dedicated SBE-aware encoder path) would eliminate the intermediate format at the cost of a more complex encoder interface.

The handler never buffers outbound messages. Each command on stream 6 produces exactly one outbound FIX frame (or zero, if the command is informational — e.g. a shadow-phase update with no associated outbound message). The cluster is responsible for ordering: commands arrive on stream 6 in commit order and are processed in arrival order.

### 6.2.6 Failover Behaviour

During follower operation the handler on a non-leader node receives no commands on stream 6 (the leader publishes stream 6; followers do not). Its TCP acceptor is not open. `shadowPhase` remains `DISCONNECTED` throughout.

On `onRoleChange(LEADER)` the cluster publishes `CONNECT` on stream 6. The handler responds:

1. Opens the TCP acceptor.
2. Sets `shadowPhase` to `LOGON_PENDING`.
3. Begins offering inbound frames from newly accepted connections on stream 1.

The handler does not need to recover any prior state — it inherits the session's committed state entirely from the cluster's subsequent stream 6 commands. The first command after `CONNECT` is typically `SEND Logon` (the cluster-initiated outbound Logon to the reconnecting client), which the handler encodes and sends verbatim.