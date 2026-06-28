# Chapter 6: Detailed Component Architecture

This chapter provides implementation-level detail for each major component in the gateway. The high-level architecture is in Chapter 2; this chapter assumes that material and goes deeper into the internal mechanics, data structures, and invariants of each subsystem.

---

## 6.0 Dependencies

The gateway uses two Real Logic libraries throughout.

**Aeron** (`io.aeron:aeron-all` / C++ `aeron` CMake target) provides the IPC and UDP transport, the Cluster consensus module, and the Archive recording layer. It is a first-class dependency on both the Java cluster side and the C++ process side.

**Agrona** (`org.agrona:agrona` / C++ headers bundled with the Aeron client) is Aeron's own support library. It provides the `OneToOneRingBuffer` used for all intra-process inter-thread queues in this gateway, along with `AtomicBuffer`, `UnsafeBuffer`, and the lock-free utilities that underpin Aeron's own internal ring buffers. Agrona is a **transitive dependency of Aeron** — it is already present on both the Java classpath and the C++ include path whenever Aeron is linked. No separate dependency declaration is required; the Agrona headers are part of the Aeron C++ client distribution and the Agrona JAR is pulled in by `aeron-all`.

| Library | Artifact / target | Language | Used for |
|---------|-------------------|----------|----------|
| Aeron | `io.aeron:aeron-all` / `aeron` | Java + C++ | IPC, UDP transport, Cluster, Archive |
| Agrona | transitive via Aeron | Java + C++ | `OneToOneRingBuffer`, `AtomicBuffer`, lock-free primitives |

---

## 6.1 Clustered Service Sequencer

### 6.1.1 Role and Positioning

The Clustered Service Sequencer is the component that imposes a **total, deterministic order** on every message entering the gateway, regardless of which FIX session, which Ingress node, or which C++ process it originated from. It is the boundary between the unordered, concurrent world of TCP ingress and the ordered, replayable world of the application engine.

No application-level processing — risk checks, order routing, position accounting — occurs before the sequencer commits a message. This is a hard architectural invariant: the sequencer is the single gate through which all state-changing events pass. It has two consequences that are fundamental to the gateway's correctness:

1. **Cross-node consistency.** Every cluster node applies messages in identical order. The C++ AppWorkers on all three nodes receive the same command stream and therefore maintain identical application state. There is no divergence between nodes while any quorum member is alive.

2. **Exact-once failover.** When a new leader is elected, it resumes processing from the exact log position where the old leader left off. No committed message is lost or applied twice at the cluster boundary.

The sequencer is implemented in Java 21 using Aeron Cluster and consists of two cooperating components: the `ConsensusModule` (part of the Aeron Cluster library) and `FixClusteredService` (gateway application code). `FixClusteredService` is itself split into `FixAeronHandler` (which implements `ClusteredService` and owns all Aeron-specific state) and `FixSessionStateMachine` (which contains the FIX session logic and is called synchronously by the handler). See §6.1.6 for the internal structure.

---

### 6.1.2 Inbound Streams

The sequencer receives messages from four distinct Aeron IPC streams, each carrying a different category of event:

| Stream | Source | Content | Format |
|--------|--------|---------|--------|
| 1 | C++ Ingress (FIX Session, Core 2) | TCP lifecycle events (`SESSION_CONNECT` / `SESSION_DISCONNECT`) and validated inbound FIX frames from buy-side clients, in causal order per session | SBE-encoded per event/frame |
| 3 | C++ AppWorker (Core 7) | Application feedback: sent-reports, reject commands | SBE-encoded |
| 5 | C++ Egress (Core 9) | Execution reports received from the sell-side gateway | SBE-encoded |
| 7 | C++ Risk Thread (Core 8) | Risk check results: approved or rejected, with correlationId | SBE-encoded |

All four streams deliver SBE-encoded payloads. No raw FIX bytes enter the sequencer on any stream; the FIX Session component (Core 2) decodes the FIX wire format into a typed SBE message before publication on stream 1. TCP lifecycle events (`SESSION_CONNECT`, `SESSION_DISCONNECT`) are published on stream 1 alongside FIX frames so the cluster sees them in the same committed order as every message from that session. All other streams carry application-layer SBE messages that never had a FIX representation.

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
- **ResendRequest servicing.** `MemoryStorage` indexes outbound messages by `outboundSeqNum` (the FIX MsgSeqNum assigned to each outbound message). When a buy-side ResendRequest arrives, the cluster uses this index to locate stored SBE payloads by the sequence number range requested. For ranges that extend beyond `MemoryStorage`, the cluster also maintains a `LinkedHashMap<outboundSeqNum, ArchivePosition>` that maps each stored sequence number to its recording position in Aeron Archive. The risk-response correlation anchor (`clusterSessionPosition`) is a separate concept used only for matching Risk Thread responses back to the originating order.

**Dual sequence numbering.** Every received message carries two independent sequence numbers that serve different purposes:

| Number | Scope | Assigned by | Used for |
|--------|-------|-------------|----------|
| `clusterSessionPosition` | Global — all sessions combined | `ConsensusModule` at commit time | Idempotency, failover correlation, risk response matching |
| `inboundSeqNum` (FIX tag 34) | Per-connection — one counter per external FIX session | `FixClusteredService` per `SessionState` entry | FIX gap detection, ResendRequest range, session-level acknowledgement |

The two numbers are independent. A message arriving on the buy-side client session might receive global position 10042 and per-connection sequence 17; the next message arriving on the sell-side session might receive global position 10043 and per-connection sequence 892. The global position imposes a total order across all connections simultaneously; the per-connection sequence number tracks continuity within a single FIX session as required by the FIX protocol.

On failover both numbers are recovered from cluster state: the global position from the Raft log tail (the next committed entry continues from the highest position), and the per-connection sequences from the `inboundSeqNum`/`outboundSeqNum` fields in each `SessionState` snapshot entry.

---

### 6.1.5 Timestamp Assignment

Every committed log entry carries a **`cluster.timeMs()`** timestamp — the cluster's logical time at the moment the `ConsensusModule` created the log entry on the leader.

**What it is.** `cluster.timeMs()` is not wall-clock time. It is a Raft-committed logical timestamp that advances in lockstep with the Raft heartbeat. The `ConsensusModule` samples the system clock when it writes each heartbeat log entry and embeds the value. Ordinary message entries inherit the timestamp from the most recent committed heartbeat. This means timestamps advance in discrete steps (heartbeat interval, default 250 ms) rather than continuously.

**Why wall-clock is forbidden.** Inside any `ClusteredService` callback (`onSessionMessage`, `onTimerEvent`, `onRoleChange`), all access to wall-clock time is prohibited. This is a determinism requirement: if the leader and a follower called `System.currentTimeMillis()` at different instants, they would see different values and their state machines would diverge. `cluster.timeMs()` is identical on every node for every callback invocation because it is derived from committed log entries, not from the local system clock.

**Timer scheduling.** Heartbeat timers, test-request timers, and resend timers are all registered via `cluster.scheduleTimer(correlationId, deadlineMs)` where `deadlineMs` is expressed in terms of `cluster.timeMs()`. When the cluster's logical time advances past a timer's deadline, `FixClusteredService.onTimerEvent(correlationId, timestamp)` fires on every node in the same commit order as all other events. The timer fires deterministically and simultaneously on all nodes.

**Relationship to FIX timestamps.** FIX message fields such as `SendingTime` (tag 52) and `TransactTime` (tag 60) carry the client's wall-clock time as text inside the FIX message. These are preserved verbatim in the SBE payload and passed through to the application layer. The cluster does not modify or validate them. The cluster's own `cluster.timeMs()` is separate and is used only for internal timer management and log entry ordering.

---

### 6.1.6 FixClusteredService: Internal Structure

`FixClusteredService` implements the `ClusteredService` interface. Internally it is split into two cooperating classes:

- **`FixAeronHandler`** — owns the `ClusteredService` interface. Receives every Aeron Cluster callback (`onSessionMessage`, `onTimerEvent`, `onTakeSnapshot`, `onLoadSnapshot`, `onNewLeadershipTerm`, `onRoleChange`) and holds all Aeron-specific runtime state. On each callback it decodes the SBE payload and delegates synchronously to `FixSessionStateMachine`.

- **`FixSessionStateMachine`** — contains the FIX session logic: phase transitions, sequence number validation, MemoryStorage maintenance, and command emission. It has no direct dependency on Aeron Cluster APIs; it receives decoded events and emits typed commands that `FixAeronHandler` encodes and publishes.

Because `FixSessionStateMachine` is called synchronously from within `FixAeronHandler`'s callbacks, and all callbacks are invoked by the single Aeron Cluster conductor thread in commit order, there is no concurrency between the two classes. The Raft log's total ordering is fully preserved.

```
onSessionMessage(header, buffer, offset, length)   ← Aeron Cluster conductor thread
    │
    ▼
FixAeronHandler
    · decode SBE discriminator and payload
    · call FixSessionStateMachine.onEvent(event)   ← synchronous, same thread
    │       │
    │       ▼
    │   FixSessionStateMachine
    │       · advance session phase
    │       · validate / update sequence numbers
    │       · update MemoryStorage
    │       · return list of Commands to emit
    │
    · encode each Command → Publication.offer() on stream 2
    · apply isLeader gate and back-pressure retry (§6.1.9)
```

**State split between the two classes:**

`FixAeronHandler` holds Aeron-specific runtime state (not snapshotted):

```
isLeader:      boolean       — set in onNewLeadershipTerm, cleared in onRoleChange
pendingResend: PendingResend — active Archive replay; null when idle
               { image: Image, emitPosition: long, endPosition: long }
```

`FixSessionStateMachine` holds all FIX session state per session. Snapshotted fields survive failover; runtime fields are re-derived:

```
— Snapshotted (serialised by FixAeronHandler.onTakeSnapshot) —
inboundSeqNum:        int64
outboundSeqNum:       int64
sessionPhase:         SessionPhase  — { DISCONNECTED, LOGON_PENDING, ACTIVE, LOGOUT_PENDING }
pendingTestReqId:     String        — TestReqID of outstanding probe (null if none)
memoryStorage:        MemoryStorage — circular buffer of last 2500 outbound SBE messages
outboundArchiveIndex: LinkedHashMap<outboundSeqNum, ArchivePosition>
                       — insertion-order; LinkedHashMap guarantees identical snapshot bytes
heartbeatTimer:       long          — correlationId of active heartbeat timer (0 if none)
testReqTimer:         long          — correlationId of active test-request timer (0 if none)
```

`onTakeSnapshot` and `onLoadSnapshot` live in `FixAeronHandler` (they interact with the Aeron Archive API directly) but serialise and restore `FixSessionStateMachine` state.

**Session phase transitions.** The session state machine advances on committed messages:

```
DISCONNECTED
    ├─ on committed SESSION_CONNECT event (stream 1) → LOGON_PENDING
    └─ (stays DISCONNECTED on all other events)

LOGON_PENDING
    ├─ on committed Logon (tag 35=A)   → ACTIVE
    │    emit: SEND Logon-Ack (stream 2), start heartbeat timer
    ├─ on heartbeat timer expiry       → emit SEND TestRequest; start test-req timer
    ├─ on test-req timer expiry        → emit DISCONNECT; → DISCONNECTED
    └─ on committed SESSION_DISCONNECT → DISCONNECTED

ACTIVE
    ├─ on committed app message (D,F,G,…) → emit FORWARD_APP (stream 2)
    ├─ on committed Heartbeat (tag 35=0)  → reset heartbeat timer
    ├─ on committed TestRequest           → emit SEND Heartbeat (stream 2)
    ├─ on committed ResendRequest         → service per §2.5.5
    │    fast path (≤2500): synchronous; emit RESEND/GAP_FILL within onSessionMessage
    │    slow path (>2500): start Archive replay; store in pendingResend; poll across duty cycles
    ├─ on committed Logout (tag 35=5)     → emit SEND Logout-Ack; → LOGOUT_PENDING
    ├─ on heartbeat timer expiry          → emit SEND TestRequest; start test-req timer
    ├─ on test-req timer expiry           → emit SEND Logout; → LOGOUT_PENDING
    └─ on committed SESSION_DISCONNECT    → → DISCONNECTED

LOGOUT_PENDING
    ├─ on committed Logout-Ack            → → DISCONNECTED
    ├─ on logout timer expiry             → → DISCONNECTED (force)
    └─ on committed SESSION_DISCONNECT    → → DISCONNECTED
```

**Sequence number validation.** On each inbound committed message the service compares the FIX `MsgSeqNum` (tag 34, extracted from the SBE payload) against `inboundSeqNum + 1`:

- **Match**: normal case. Increment `inboundSeqNum`. Proceed with phase transition.
- **Too high (gap)**: emit `SEND ResendRequest` on stream 2. Do not advance `inboundSeqNum` until the gap is filled.
- **Too low with `PossDupFlag=Y`** (tag 43): duplicate retransmission. Ignore silently. Do not advance `inboundSeqNum`.
- **Too low without `PossDupFlag`**: sequence reset. Emit `SEND Logout` and transition to LOGOUT_PENDING.

**FixAeronHandler: class outline.**

```java
public final class FixAeronHandler implements ClusteredService
{
    private static final int REPLAY_BATCH        = 10;
    private static final int ENVELOPE_HEADER_LEN = 9; // 1-byte discriminator + 8-byte sessionId

    // session map: LinkedHashMap for deterministic snapshot byte order (§6.1.11)
    private final LinkedHashMap<Long, FixSessionStateMachine> sessions        = new LinkedHashMap<>();
    private final ExpandableDirectByteBuffer                  encodingBuffer  = new ExpandableDirectByteBuffer(4096);

    private Cluster       cluster;
    private Publication   stream2;
    private AeronArchive  aeronArchive;
    private boolean       isLeader      = false;
    private PendingResend pendingResend  = null;  // non-null only during Archive replay

    // ── Lifecycle ─────────────────────────────────────────────────────────────

    @Override
    public void onStart(final Cluster cluster, final Image snapshotImage)
    {
        this.cluster      = cluster;
        this.stream2      = cluster.context().aeron().addPublication(STREAM_2_CHANNEL, STREAM_2_ID);
        this.aeronArchive = AeronArchive.connect(new AeronArchive.Context()
            .aeron(cluster.context().aeron()));

        if (snapshotImage != null)
        {
            loadSnapshot(snapshotImage);
        }
    }

    // ── Committed log entries ─────────────────────────────────────────────────

    @Override
    public void onSessionMessage(
        final ClientSession aeronSession,
        final long          timestamp,
        final DirectBuffer  buffer,
        final int           offset,
        final int           length,
        final Header        header)
    {
        pollPendingResend();   // make progress on any in-flight Archive replay

        final int  discriminator = buffer.getByte(offset) & 0xFF;
        final long fixSessionId  = buffer.getLong(offset + 1);

        final FixSessionStateMachine fsm = sessions.computeIfAbsent(
            fixSessionId, FixSessionStateMachine::new);

        final Command[] commands = fsm.onEvent(
            discriminator, buffer, offset + ENVELOPE_HEADER_LEN,
            length - ENVELOPE_HEADER_LEN, cluster.timeMs());

        emitCommands(commands, header.position());
    }

    @Override
    public void onTimerEvent(final long correlationId, final long timestamp)
    {
        pollPendingResend();

        // each FSM checks the correlationId against its own timers and returns
        // commands only if the timer belongs to it; others return empty
        for (final FixSessionStateMachine fsm : sessions.values())
        {
            final Command[] commands = fsm.onTimer(correlationId, cluster.timeMs());
            if (commands.length > 0)
            {
                emitCommands(commands, 0L);
                break;
            }
        }
    }

    // ── Snapshot ──────────────────────────────────────────────────────────────

    @Override
    public void onTakeSnapshot(final ExclusivePublication snapshotPublication)
    {
        for (final FixSessionStateMachine fsm : sessions.values())
        {
            final int len = fsm.encodeTo(encodingBuffer, 0);
            while (snapshotPublication.offer(encodingBuffer, 0, len) < 0)
            {
                cluster.idleStrategy().idle();
            }
        }
    }

    private void loadSnapshot(final Image snapshotImage)
    {
        final FragmentHandler decoder = (buf, off, len, hdr) ->
        {
            final FixSessionStateMachine fsm = FixSessionStateMachine.decodeFrom(buf, off);
            sessions.put(fsm.sessionId(), fsm);
        };

        while (!snapshotImage.isClosed())
        {
            cluster.idleStrategy().idle(snapshotImage.poll(decoder, REPLAY_BATCH));
        }
    }

    // ── Leadership ────────────────────────────────────────────────────────────

    @Override
    public void onRoleChange(final Cluster.Role newRole)
    {
        isLeader = (newRole == Cluster.Role.LEADER);
        if (!isLeader)
        {
            pendingResend = null;  // follower: discard any in-progress replay (will be restarted
        }                         // by new leader re-processing the committed ResendRequest entry)
    }

    // ── Archive replay (slow-path ResendRequest, §6.1.7) ─────────────────────

    void startArchiveReplay(
        final FixSessionStateMachine fsm,
        final long                   recordingId,
        final long                   startPosition,
        final long                   endPosition)
    {
        final long replaySessionId = aeronArchive.startReplay(
            recordingId, startPosition, endPosition - startPosition,
            REPLAY_CHANNEL, REPLAY_STREAM_ID);

        final Image image = cluster.context().aeron()
            .addSubscription(REPLAY_CHANNEL, REPLAY_STREAM_ID)
            .imageBySessionId((int) replaySessionId);

        pendingResend = new PendingResend(fsm, image, startPosition, endPosition);
    }

    private void pollPendingResend()
    {
        if (pendingResend == null || !isLeader)
        {
            return;
        }

        final int fragments = pendingResend.image.poll(
            (buf, off, len, hdr) ->
            {
                final Command[] commands = pendingResend.fsm
                    .onResendFragment(buf, off, len, cluster.timeMs());
                emitCommands(commands, 0L);
                pendingResend.emitPosition = hdr.position();
            },
            REPLAY_BATCH);

        cluster.idleStrategy().idle(fragments);

        if (pendingResend.image.isClosed())
        {
            // emit the tail: messages in MemoryStorage beyond the Archive window
            final Command[] tail = pendingResend.fsm
                .flushMemoryStorageTail(pendingResend.emitPosition, pendingResend.endPosition);
            emitCommands(tail, 0L);
            pendingResend = null;
        }
    }

    // ── Command emission ──────────────────────────────────────────────────────

    private void emitCommands(final Command[] commands, final long clusterPosition)
    {
        if (!isLeader)
        {
            return;   // suppress all output during log replay on a follower
        }
        for (final Command cmd : commands)
        {
            cmd.encodeInto(encodingBuffer, 0, clusterPosition);
            while (stream2.offer(encodingBuffer, 0, cmd.encodedLength()) < 0)
            {
                cluster.idleStrategy().idle();  // must use cluster idle so consensus housekeeping continues
            }
        }
    }
}
```

**FixSessionStateMachine: class outline.**

`Command` is a sealed interface; `FixAeronHandler` inspects returned commands and handles `ScheduleTimerCommand` and `StartArchiveReplayCommand` locally before emitting the rest on stream 2.

```java
// ── Command types returned to FixAeronHandler ─────────────────────────────────
sealed interface Command permits
    SendCommand, ForwardAppCommand, ResendCommand, GapFillCommand,
    DisconnectCommand, ScheduleTimerCommand, StartArchiveReplayCommand
{
    Command[] NONE = new Command[0];
}

record SendCommand              (long sessionId, DirectBuffer payload, int length)           implements Command {}
record ForwardAppCommand        (long sessionId, DirectBuffer payload, int length)           implements Command {}
record ResendCommand            (long sessionId, long origSeqNum, DirectBuffer payload, int length) implements Command {}
record GapFillCommand           (long sessionId, long fromSeqNum, long newSeqNo)             implements Command {}
record DisconnectCommand        (long sessionId)                                              implements Command {}
record ScheduleTimerCommand     (long correlationId, long deadlineMs)                        implements Command {}
record StartArchiveReplayCommand(long recordingId,  long startPosition, long endPosition)    implements Command {}

// ── FixSessionStateMachine ────────────────────────────────────────────────────
public final class FixSessionStateMachine
{
    private final long sessionId;

    // ── Snapshotted state (serialised by FixAeronHandler.onTakeSnapshot) ──────
    private SessionPhase sessionPhase     = SessionPhase.DISCONNECTED;
    private long         inboundSeqNum    = 0;
    private long         outboundSeqNum   = 0;
    private String       pendingTestReqId = null;
    private long         heartbeatTimer   = 0;   // correlationId; 0 = none
    private long         testReqTimer     = 0;
    private final MemoryStorage                        memoryStorage        = new MemoryStorage(2500);
    private final LinkedHashMap<Long, ArchivePosition> outboundArchiveIndex = new LinkedHashMap<>();

    private long nextTimerId = 1;   // monotonically increasing; snapshotted

    FixSessionStateMachine(final long sessionId) { this.sessionId = sessionId; }
    long sessionId() { return sessionId; }

    // ── Event dispatch ────────────────────────────────────────────────────────

    Command[] onEvent(
        final int          discriminator,
        final DirectBuffer buffer,
        final int          offset,
        final int          length,
        final long         nowMs)
    {
        return switch (discriminator)
        {
            case DISC_SESSION_CONNECT    -> onSessionConnect(nowMs);
            case DISC_SESSION_DISCONNECT -> onSessionDisconnect();
            case DISC_LOGON              -> onInboundLogon(buffer, offset, nowMs);
            case DISC_LOGOUT             -> onInboundLogout(buffer, offset, nowMs);
            case DISC_HEARTBEAT          -> onInboundHeartbeat(buffer, offset, nowMs);
            case DISC_TEST_REQUEST       -> onInboundTestRequest(buffer, offset);
            case DISC_RESEND_REQUEST     -> onInboundResendRequest(buffer, offset);
            case DISC_SEQUENCE_RESET     -> onInboundSequenceReset(buffer, offset);
            default                      -> onInboundApplicationMessage(discriminator, buffer, offset, length);
        };
    }

    Command[] onTimer(final long correlationId, final long nowMs)
    {
        if (correlationId == heartbeatTimer) return onHeartbeatTimerExpiry(nowMs);
        if (correlationId == testReqTimer)   return onTestReqTimerExpiry(nowMs);
        return Command.NONE;
    }

    // ── Session phase transitions ─────────────────────────────────────────────

    private Command[] onSessionConnect(final long nowMs)
    {
        if (sessionPhase != SessionPhase.DISCONNECTED) return Command.NONE;
        sessionPhase   = SessionPhase.LOGON_PENDING;
        heartbeatTimer = nextTimerId++;
        return new Command[]{ new ScheduleTimerCommand(heartbeatTimer, nowMs + LOGON_TIMEOUT_MS) };
    }

    private Command[] onSessionDisconnect()
    {
        sessionPhase     = SessionPhase.DISCONNECTED;
        heartbeatTimer   = 0;
        testReqTimer     = 0;
        pendingTestReqId = null;
        return Command.NONE;
    }

    private Command[] onInboundLogon(
        final DirectBuffer buffer, final int offset, final long nowMs)
    {
        if (sessionPhase != SessionPhase.LOGON_PENDING) return Command.NONE;
        final Command[] seqCheck = validateAndAdvanceSeqNum(buffer, offset, nowMs);
        if (seqCheck != null) return seqCheck;

        sessionPhase   = SessionPhase.ACTIVE;
        heartbeatTimer = nextTimerId++;
        return new Command[]{
            new SendCommand(sessionId, buildLogonAck(buffer, offset), LOGON_ACK_LEN),
            new ScheduleTimerCommand(heartbeatTimer, nowMs + heartbeatIntervalMs)
        };
    }

    private Command[] onInboundHeartbeat(
        final DirectBuffer buffer, final int offset, final long nowMs)
    {
        if (sessionPhase != SessionPhase.ACTIVE) return Command.NONE;
        final Command[] seqCheck = validateAndAdvanceSeqNum(buffer, offset, nowMs);
        if (seqCheck != null) return seqCheck;

        pendingTestReqId = null;   // heartbeat satisfies any outstanding TestRequest
        heartbeatTimer   = nextTimerId++;
        return new Command[]{ new ScheduleTimerCommand(heartbeatTimer, nowMs + heartbeatIntervalMs) };
    }

    private Command[] onInboundTestRequest(final DirectBuffer buffer, final int offset)
    {
        if (sessionPhase != SessionPhase.ACTIVE) return Command.NONE;
        final Command[] seqCheck = validateAndAdvanceSeqNum(buffer, offset, 0L);
        if (seqCheck != null) return seqCheck;

        final String testReqId = extractTestReqId(buffer, offset);
        return new Command[]{ new SendCommand(sessionId, buildHeartbeat(testReqId), HEARTBEAT_LEN) };
    }

    private Command[] onInboundResendRequest(final DirectBuffer buffer, final int offset)
    {
        if (sessionPhase != SessionPhase.ACTIVE) return Command.NONE;
        final Command[] seqCheck = validateAndAdvanceSeqNum(buffer, offset, 0L);
        if (seqCheck != null) return seqCheck;

        final long beginSeqNo = extractBeginSeqNo(buffer, offset);
        final long endSeqNo   = extractEndSeqNo(buffer, offset);   // 0 means open-ended

        if (memoryStorage.covers(beginSeqNo, endSeqNo))
        {
            return buildResendCommandsFromMemory(beginSeqNo, endSeqNo);   // fast path
        }

        // slow path: ask FixAeronHandler to start an async Archive replay
        final ArchivePosition pos = outboundArchiveIndex.get(beginSeqNo);
        return new Command[]{ new StartArchiveReplayCommand(
            pos.recordingId(), pos.startPosition(), pos.endPosition()) };
    }

    private Command[] onInboundLogout(
        final DirectBuffer buffer, final int offset, final long nowMs)
    {
        if (sessionPhase == SessionPhase.DISCONNECTED) return Command.NONE;
        final Command[] seqCheck = validateAndAdvanceSeqNum(buffer, offset, nowMs);
        if (seqCheck != null) return seqCheck;

        if (sessionPhase == SessionPhase.LOGOUT_PENDING)
        {
            // peer confirms our Logout → clean close
            sessionPhase = SessionPhase.DISCONNECTED;
            return new Command[]{ new DisconnectCommand(sessionId) };
        }

        // peer-initiated Logout → ack and wait for TCP close
        sessionPhase = SessionPhase.LOGOUT_PENDING;
        return new Command[]{ new SendCommand(sessionId, buildLogoutAck(), LOGOUT_ACK_LEN) };
    }

    private Command[] onInboundSequenceReset(final DirectBuffer buffer, final int offset)
    {
        final boolean gapFill = extractGapFillFlag(buffer, offset);
        final long    newSeqNo = extractNewSeqNo(buffer, offset);
        if (gapFill)
        {
            // advance inboundSeqNum past session messages we do not expect
            inboundSeqNum = newSeqNo - 1;
        }
        else
        {
            // hard reset: accept unconditionally
            inboundSeqNum = newSeqNo - 1;
        }
        return Command.NONE;
    }

    private Command[] onInboundApplicationMessage(
        final int discriminator, final DirectBuffer buffer, final int offset, final int length)
    {
        if (sessionPhase != SessionPhase.ACTIVE) return Command.NONE;
        final Command[] seqCheck = validateAndAdvanceSeqNum(buffer, offset, 0L);
        if (seqCheck != null) return seqCheck;

        return new Command[]{ new ForwardAppCommand(sessionId, buffer, length) };
    }

    // ── Timer expiry ──────────────────────────────────────────────────────────

    private Command[] onHeartbeatTimerExpiry(final long nowMs)
    {
        return switch (sessionPhase)
        {
            case LOGON_PENDING ->
            {
                sessionPhase = SessionPhase.DISCONNECTED;
                yield new Command[]{ new DisconnectCommand(sessionId) };
            }
            case ACTIVE ->
            {
                pendingTestReqId = String.valueOf(nowMs);   // unique per cluster.timeMs()
                testReqTimer     = nextTimerId++;
                yield new Command[]{
                    new SendCommand(sessionId, buildTestRequest(pendingTestReqId), TEST_REQUEST_LEN),
                    new ScheduleTimerCommand(testReqTimer, nowMs + TEST_REQ_TIMEOUT_MS)
                };
            }
            default -> Command.NONE;
        };
    }

    private Command[] onTestReqTimerExpiry(final long nowMs)
    {
        if (sessionPhase != SessionPhase.ACTIVE) return Command.NONE;
        sessionPhase     = SessionPhase.LOGOUT_PENDING;
        pendingTestReqId = null;
        testReqTimer     = 0;
        heartbeatTimer   = nextTimerId++;
        return new Command[]{
            new SendCommand(sessionId, buildLogout(), LOGOUT_LEN),
            new ScheduleTimerCommand(heartbeatTimer, nowMs + LOGOUT_TIMEOUT_MS)
        };
    }

    // ── Sequence number validation ────────────────────────────────────────────

    /**
     * Returns null on success (caller continues); returns a non-null Command[]
     * if a gap, duplicate, or fatal sequence error was detected.
     */
    private Command[] validateAndAdvanceSeqNum(
        final DirectBuffer buffer, final int offset, final long nowMs)
    {
        final long    msgSeqNum = extractMsgSeqNum(buffer, offset);
        final boolean possDup   = extractPossDupFlag(buffer, offset);
        final long    expected  = inboundSeqNum + 1;

        if (msgSeqNum == expected)
        {
            inboundSeqNum = msgSeqNum;
            return null;   // normal
        }
        else if (msgSeqNum > expected)
        {
            // gap: request retransmission; do not advance inboundSeqNum
            return new Command[]{ new SendCommand(
                sessionId, buildResendRequest(expected, msgSeqNum - 1), RESEND_REQUEST_LEN) };
        }
        else if (possDup)
        {
            return Command.NONE;   // duplicate — discard silently
        }
        else
        {
            // too low without PossDupFlag: fatal
            sessionPhase = SessionPhase.LOGOUT_PENDING;
            return new Command[]{ new SendCommand(sessionId, buildLogout(), LOGOUT_LEN) };
        }
    }

    // ── Archive replay helpers (called by FixAeronHandler) ───────────────────

    /** Classify one replayed fragment and return RESEND or GAP_FILL commands. */
    Command[] onResendFragment(
        final DirectBuffer buffer, final int offset, final int length, final long nowMs)
    {
        final int msgType = extractMsgType(buffer, offset);
        if (isApplicationMessage(msgType))
        {
            return new Command[]{ new ResendCommand(
                sessionId, extractMsgSeqNum(buffer, offset), buffer, length) };
        }
        // session message: collapse to GAP_FILL (details deferred to buildGapFill)
        return buildGapFillForSessionMessage(buffer, offset);
    }

    /** Emit RESEND/GAP_FILL for any tail entries in MemoryStorage. */
    Command[] flushMemoryStorageTail(final long fromSeqNum, final long toSeqNum)
    {
        return buildResendCommandsFromMemory(fromSeqNum, toSeqNum);
    }

    // ── Snapshot serialisation ────────────────────────────────────────────────

    int encodeTo(final MutableDirectBuffer buf, final int offset)
    {
        // write: sessionId, sessionPhase, inboundSeqNum, outboundSeqNum,
        //        pendingTestReqId, heartbeatTimer, testReqTimer, nextTimerId,
        //        memoryStorage contents, outboundArchiveIndex (insertion order)
        return ENCODED_LENGTH;   // bytes written
    }

    static FixSessionStateMachine decodeFrom(final DirectBuffer buf, final int offset)
    {
        // mirror of encodeTo
        return new FixSessionStateMachine(buf.getLong(offset));
    }
}
```

---

### 6.1.7 MemoryStorage and ResendRequest Servicing

`MemoryStorage` is a circular buffer of the last 2500 SBE-encoded outbound messages. It is updated every time the cluster emits a `SEND` command on stream 2: the SBE payload is written into the circular buffer indexed by `outboundSeqNum`.

When a `ResendRequest` (tag 35=2) arrives, `FixClusteredService` scans `MemoryStorage` for the requested sequence range:

- **Application messages** (ExecutionReport, OrderCancelReject): emitted as `RESEND` commands on stream 2, each carrying `PossDupFlag=Y` (tag 43) added to the SBE payload by the cluster before publication.
- **Session messages** (Heartbeat, TestRequest, Logon, etc.): never retransmitted individually. The entire run of session messages within the requested range is replaced by a single `GAP_FILL` command that instructs the AppWorker's FIX Session component to emit a FIX SequenceReset-GapFill (tag 123=Y).

If the requested range extends beyond what `MemoryStorage` holds, the cluster falls back to Aeron Archive for the older entries using an **asynchronous state machine** stored in `pendingResend`:

1. **`onSessionMessage` (start):** call `AeronArchive.startReplay()` to start the replay subscription. `startReplay()` returns immediately — it does not block until data arrives. Store the returned `Image`, the current emit position, and the end position in `pendingResend`. Return from `onSessionMessage`.

2. **Subsequent duty cycles (poll):** on each service duty cycle, poll `pendingResend.image` for available fragments. Emit each replayed message as a `RESEND` or `GAP_FILL` command on stream 2 (respecting the §6.1.9 back-pressure pattern), then advance `emitPosition`. Bound the per-cycle fragment batch to avoid starving other streams.

3. **Completion:** when `pendingResend.image.isClosed()` is true, the Archive replay has delivered all requested entries. Emit any tail entries that fall within `MemoryStorage` (the most recent messages, beyond the Archive window), then clear `pendingResend` (set to null).

`pendingResend` is runtime state — it is not snapshotted (see §6.1.10). On failover the incoming leader finds `pendingResend` null and re-starts any in-progress Archive replay from the beginning by re-processing the committed ResendRequest log entry. Because every emitted RESEND frame carries `PossDupFlag=Y`, re-delivery of frames already sent by the previous leader is idempotent at the FIX protocol level.

The Archive slow path is triggered only for ranges older than 2500 messages. In normal operation this does not occur; it is a resilience path for unusual scenarios such as a lengthy disconnect followed by a large gap fill.

---

### 6.1.8 Multi-Session Sequencing

`FixClusteredService` maintains a session context for **every external FIX connection** — not only buy-side client sessions, but also the sell-side exchange session. In the minimum deployment there are at least two contexts: one for the buy-side client and one for the sell-side exchange. In practice there will be one sell-side context plus one context per connected buy-side client.

The service maintains a `LinkedHashMap<Long, SessionState>` keyed by a `sessionId` that is unique per external connection. The `LinkedHashMap` preserves insertion order, guaranteeing identical snapshot byte sequences across all cluster nodes. Each entry holds the full per-session state described in §6.1.6 plus a role indicator:

```
sessionRole:  SessionRole — ACCEPTOR (buy-side) or INITIATOR (sell-side)
(plus all fields from §6.1.6: inboundSeqNum, outboundSeqNum, sessionPhase,
 pendingTestReqId, memoryStorage, outboundArchiveIndex, heartbeatTimer, testReqTimer)
```

The `sessionRole` field distinguishes the two session types. Buy-side (acceptor) sessions wait for the client to send a Logon; sell-side (initiator) sessions emit the Logon themselves after `CONNECT`. The phase transition diagrams differ accordingly, but the underlying state fields — sequence numbers, timers, `MemoryStorage` — are identical in structure. Both session types are replicated across all Raft nodes and included in the cluster snapshot.

This is the property that makes sell-side failover as clean as buy-side failover: when a new leader is elected, it inherits the committed sell-side `outboundSeqNum` and `inboundSeqNum` from the cluster state and uses them immediately when establishing the new exchange connection, without requiring any local recovery on the EgressWorker.

**Session map at startup (minimum configuration):**

| sessionId | Role | CompID pair | Notes |
|-----------|------|-------------|-------|
| `SELL_SIDE` (well-known constant) | INITIATOR | GatewayCompID / ExchangeCompID | Sell-side exchange session |
| per buy-side client | ACCEPTOR | ClientCompID / GatewayCompID | One entry per connected client |

The sell-side `sessionId` is a well-known constant agreed at configuration time, not an Aeron-assigned value, because the sell-side connection is initiated by the gateway rather than received on a shared Aeron ingress channel.

Total ordering across all sessions is provided by the Raft log: every message from every external connection — buy-side and sell-side — is committed to a unique `clusterSessionPosition` before any application processing occurs. Concurrent messages from different connections are serialised by the `ConsensusModule` on the leader and applied in the same order on every node.

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
- A single committed log entry may produce **multiple commands** on stream 2. A `ResendRequest` that spans 10 messages produces 10 `RESEND` (or `GAP_FILL`) commands in sequence. When the requested range exceeds `MemoryStorage` capacity (slow path, §6.1.7), those commands are emitted across multiple duty cycles via `pendingResend` rather than within a single `onSessionMessage` invocation.
- `FORWARD_APP` is emitted only for application-layer FIX messages (MsgType D, F, G, 8, 9, …). Session-layer messages (A, 0, 1, 2, 5, …) are handled entirely within `FixClusteredService` and do not produce a `FORWARD_APP` command.
- All `Publication.offer()` calls on stream 2 are gated on `isLeader`. During Raft log replay on a follower node, `isLeader` is false and no commands are emitted; the follower rebuilds its state silently. When `isLeader` is true and an offer returns a negative value (back-pressure), the service retries using `cluster.idleStrategy().idle()` until the offer succeeds — the cluster-provided idle strategy must be used so that the consensus module's own housekeeping continues during the retry spin.

---

### 6.1.10 Snapshot and Log Replay

**Snapshot trigger.** The cluster takes a snapshot of `FixClusteredService` state periodically — by default, every N committed entries (configurable) or on a manual trigger — and writes it to Aeron Archive on the local NVMe. The snapshot position is the `clusterSessionPosition` of the last committed entry at the time of the snapshot.

**Snapshot contents.** The snapshot serialises all snapshotted `FixClusteredService` state. Runtime-only fields (`pendingResend`, `isLeader`) are excluded.

```
for each sessionId in sessionMap (in insertion order — LinkedHashMap):
    sessionId:            uint64
    inboundSeqNum:        int64
    outboundSeqNum:       int64
    sessionPhase:         uint8
    pendingTestReqId:     String  (empty string if no outstanding TestRequest probe)
    heartbeatTimer:       int64   (correlationId, 0 if none)
    testReqTimer:         int64   (correlationId, 0 if none)
    memoryStorage:        MemoryStorage (2500 × SBE message slots)
    outboundArchiveIndex: LinkedHashMap entries in insertion order
                          { outboundSeqNum: int64, recordingId: int64,
                            startPosition: int64, length: int64 }

NOT included in snapshot (runtime state, re-derived on recovery):
    pendingResend:        (null on fresh leader; re-started from committed ResendRequest log entry)
    isLeader:             (re-derived from first onNewLeadershipTerm callback)
```

The session map itself is a `LinkedHashMap` (insertion order) to guarantee identical snapshot byte sequences across all cluster nodes. Any non-deterministic iteration order (e.g. `HashMap`) would produce different snapshot bytes on different nodes, violating the determinism invariant described in §6.1.11.

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

This constraint is what makes failover seamless. When the cluster elects a new leader, `FixClusteredService.onNewLeadershipTerm()` fires with the full committed session state already in place and sets `isLeader = true`. The new leader's C++ handler starts consuming stream 6 from that point and immediately knows the correct session phase, the correct sequence positions, and the correct outbound queue — without any recovery step of its own. `onRoleChange` clears `isLeader` when this node ceases to be leader.

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
6. Write TxDescriptor { buf, len, fd } to TX ring → reactor.
```

Step 3 (SBE → FIX internal format) is required because `PayloadEncoder` operates on FIX internal format fields, not directly on SBE. This is the translation step noted in §2.4.4. An alternative design (a dedicated SBE-aware encoder path) would eliminate the intermediate format at the cost of a more complex encoder interface.

The handler never buffers outbound messages. Each command on stream 6 produces exactly one outbound FIX frame (or zero, if the command is informational — e.g. a shadow-phase update with no associated outbound message). The cluster is responsible for ordering: commands arrive on stream 6 in commit order and are processed in arrival order.

### 6.2.6 Failover Behaviour

During follower operation the handler on a non-leader node receives no commands on stream 6 (the leader publishes stream 6; followers do not). Its TCP acceptor is not open. `shadowPhase` remains `DISCONNECTED` throughout.

On `onNewLeadershipTerm()` (`isLeader` becomes true) the cluster publishes `CONNECT` on stream 6. The handler responds:

1. Opens the TCP acceptor.
2. Sets `shadowPhase` to `LOGON_PENDING`.
3. Begins offering inbound frames from newly accepted connections on stream 1.

The handler does not need to recover any prior state — it inherits the session's committed state entirely from the cluster's subsequent stream 6 commands. The first command after `CONNECT` is typically `SEND Logon` (the cluster-initiated outbound Logon to the reconnecting client), which the handler encodes and sends verbatim.

---

## 6.3 Session Component (Ingress Process)

### 6.3.1 Role and Internal Structure

The Session Component is the C++ Ingress process. It is the gateway's sole point of contact with buy-side clients over TCP and the sole producer of SBE-encoded inbound messages on stream 1. Internally it is two cooperating threads connected by a pair of Agrona `OneToOneRingBuffer` queues:

```
TCP (buy-side clients)
        │ inbound FIX bytes
        ▼
┌───────────────────────┐  RX Ring  ┌─────────────────────────────┐
│  Core 1               │ ────────► │  Core 2                     │
│  io_uring Reactor     │           │  FIX Session Handler        │
│                       │ ◄──────── │                             │
│  · TCP accept/recv    │  TX Ring  │  · PayloadDecoder (SIMD)    │
│  · Registered buffers │           │  · Admission filter         │
│  · SQPOLL kernel poll │           │  · SBE encode → stream 1    │
│  · CQE busy-spin      │           │  · stream 6 poll → FIX enc  │
└───────────────────────┘           └─────────────────────────────┘
        ▲ outbound FIX bytes
        │
  TX Ring → Core 1 → IORING_OP_SEND
```

§6.2 describes the design principles of the FIX Session Handler as a cluster-driven proxy. This section covers the implementation detail of both threads and their interaction.

### 6.3.2 Ingress Reactor (Core 1)

The reactor is a single-threaded busy-spin loop that owns all TCP file descriptors. It never blocks and never allocates heap memory. The default implementation uses **epoll** with non-blocking sockets and `SO_BUSY_POLL`; on bare-metal deployments **io_uring** with `IORING_SETUP_SQPOLL` and registered buffers is available as a higher-performance alternative (see §6.3.2.1).

**Accept path.** A non-blocking listen socket is registered with `epoll_ctl(EPOLL_CTL_ADD, EPOLLIN | EPOLLET)`. When `epoll_wait` returns an event on the listen socket, the reactor calls `accept4(SOCK_NONBLOCK)` in a loop until it returns `EAGAIN`, accepting all pending connections in a single wake-up. Each accepted fd is set to `O_NONBLOCK`, has `TCP_NODELAY` and `SO_BUSY_POLL` applied, and is registered with `epoll_ctl(EPOLL_CTL_ADD, EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLET)`. The fd is recorded in a flat `fd → SessionContext*` table for O(1) lookup.

**Reactor loop.** The reactor calls `epoll_wait(epfd, events, MAX_EVENTS, 0)` with a timeout of zero on every iteration — non-blocking poll. When no events are ready it returns immediately with `n = 0` and the loop spins. `SO_BUSY_POLL` on each accepted fd instructs the NIC driver to busy-poll the receive queue for a configurable number of microseconds before returning to the kernel, reducing interrupt-driven wakeup latency to near-zero at the cost of CPU.

```cpp
while (running) {
    int n = epoll_wait(epfd, events, MAX_EVENTS, /*timeout=*/0);
    for (int i = 0; i < n; ++i) {
        int  fd  = events[i].data.fd;
        auto ev  = events[i].events;
        if (fd == listenFd)                  { acceptAll();          }
        else if (ev & (EPOLLHUP | EPOLLERR)) { handleDisconnect(fd); }
        else {
            if (ev & EPOLLIN)  { drainReceive(fd); }
            if (ev & EPOLLOUT) { flushPendingSend(fd); }
        }
    }
    drainTxRing();   // submit any outbound data Core 2 has queued
}
```

**Receive path.** `drainReceive(fd)` calls `recv(fd, buf, BUF_SIZE, MSG_DONTWAIT)` in a loop until `EAGAIN`. Each successful `recv` writes an `RxDescriptor { buf_ptr, length, fd }` to the RX ring toward Core 2 via `rxRing.write(MSG_RX_DATA, ...)`. The receive buffer is a fixed-size slab; Core 2 must consume the descriptor before the reactor's next `recv` on the same fd overwrites that buffer slot (on the epoll path). On the io_uring registered-buffer path the slot is not reused until Core 2 signals completion.

**Send path.** `drainTxRing()` polls the TX ring and calls `send(fd, buf, len, MSG_DONTWAIT)` for each `TxDescriptor`. If `send` returns `EAGAIN` (kernel send buffer full), the unsent remainder is held in a per-fd pending send slot and `EPOLLOUT` is armed on the fd via `epoll_ctl(EPOLL_CTL_MOD)`. `flushPendingSend(fd)` is called when `EPOLLOUT` fires, resuming the stalled send. Once the pending send is drained, `EPOLLOUT` is disarmed.

**Disconnect handling.** `EPOLLRDHUP` or a zero-length `recv` signals a clean client close. `EPOLLERR` or a negative `recv`/`send` result signals an error. In either case the reactor calls `epoll_ctl(EPOLL_CTL_DEL)`, closes the fd, removes it from the fd table, and writes an `MSG_RX_DISCONNECT` message (`RxDisconnect { fd }`) to the RX ring. Core 2 interprets this as a disconnect for that session.

#### 6.3.2.1 io_uring on Bare Metal

On bare-metal deployments where `CAP_SYS_NICE` and kernel ≥ 6.1 are available, the reactor can be replaced with an io_uring implementation that eliminates syscall overhead and kernel data copies on the hot path:

| Mechanism | epoll (default) | io_uring (bare metal) |
|-----------|----------------|----------------------|
| Receive syscall overhead | `recv()` per burst | Zero (`IORING_SETUP_SQPOLL`) |
| Data copy on receive | Kernel → user buffer on each `recv` | None — registered buffers filled in place |
| Send syscall overhead | `send()` per frame | Zero — SQE submission only |
| Accept | `accept4()` per connection | `IORING_OP_ACCEPT` with multishot |
| Capability required | None | `CAP_SYS_NICE` for SQPOLL |

The io_uring reactor uses `IORING_SETUP_SQPOLL` (kernel SQ polling thread), `io_uring_register_buffers` (fixed registered buffer pool), `IORING_RECV_MULTISHOT` (one SQE serves multiple receives), and `IORING_OP_SEND` with the registered buffer index. Core 2 reads directly from the registered buffer without an extra copy. On the M1 Pro the measured receive-to-ring latency is approximately 150 ns with io_uring vs. 400 ns with epoll, a difference that is visible at the sub-microsecond pipeline budget level.

Select the io_uring reactor by setting `reactor.mode = io_uring` in the gateway configuration. The epoll reactor is the default and is required in virtualised environments where `IORING_SETUP_SQPOLL` is not available.

### 6.3.3 Ring Buffer Layout

Both inter-thread queues use **`aeron::concurrent::ringbuffer::OneToOneRingBuffer`** from the Agrona C++ library. Agrona's ring provides cache-line-padded head and tail counters, `memory_order_release`/`acquire` semantics, variable-length message framing with alignment padding, and a `MessageHandler` polling interface — eliminating the need for a hand-rolled SPSC implementation.

Each ring is backed by a heap-allocated `AtomicBuffer` sized to absorb a worst-case burst. The ring buffer header occupies the first `RingBufferDescriptor::TRAILER_LENGTH` bytes of the backing store; the remaining bytes are the message region. In practice the backing store is sized at 256 KB per direction, providing headroom for bursts of several hundred maximum-length FIX frames.

**Buffer-descriptor message pattern.** The rings carry *descriptors*, not raw bytes. Raw receive bytes live in the registered buffer pool (io_uring path) or a thread-local slab (epoll path); raw send bytes live in an encoding slab owned by Core 2. Copying them through the ring would defeat the zero-copy purpose of the io_uring registered-buffer path. Only the 16-byte descriptor is written into the ring:

```cpp
// Message type IDs written into the ring (msgTypeId field of Agrona record header)
static constexpr int32_t MSG_RX_DATA       = 1;  // inbound data available
static constexpr int32_t MSG_RX_DISCONNECT = 2;  // client closed connection
static constexpr int32_t MSG_TX_DATA       = 3;  // outbound frame ready to send

// RX ring payload (MSG_RX_DATA): pointer into registered buffer pool + metadata
struct RxDescriptor {
    const uint8_t* buf;   // pointer into registered buffer pool; valid until next recv on fd
    uint32_t       len;   // bytes valid at buf
    int32_t        fd;    // source file descriptor
};

// RX ring payload (MSG_RX_DISCONNECT): sentinel — Core 2 tears down the session
struct RxDisconnect {
    int32_t fd;
};

// TX ring payload (MSG_TX_DATA): pointer into encoding slab + metadata
struct TxDescriptor {
    const uint8_t* buf;   // pointer into Core 2 encoding slab; valid until Core 1 completes send
    uint32_t       len;   // bytes to send
    int32_t        fd;    // destination file descriptor
};
```

**Write path (Core 1 → RX ring):**
```cpp
RxDescriptor desc{ recvBuf, bytesRead, fd };
rxRing.write(MSG_RX_DATA,
             AtomicBuffer{ reinterpret_cast<uint8_t*>(&desc), sizeof(desc) },
             0, sizeof(desc));
```

**Poll path (Core 2 draining RX ring):**
```cpp
rxRing.poll(
    [](int32_t msgTypeId, AtomicBuffer& buf, int32_t offset, int32_t length) {
        if (msgTypeId == MSG_RX_DATA) {
            auto& d = *reinterpret_cast<RxDescriptor*>(buf.buffer() + offset);
            processFrame(d.buf, d.len, d.fd);     // raw bytes read directly from pool
        } else if (msgTypeId == MSG_RX_DISCONNECT) {
            auto& d = *reinterpret_cast<RxDisconnect*>(buf.buffer() + offset);
            tearDownSession(d.fd);
        }
    },
    FRAGMENT_LIMIT);

### 6.3.4 PayloadDecoder: SIMD Frame Detection

`PayloadDecoder` operates on each `RxDescriptor` delivered by the RX ring. It has two phases: SOH scan and field decode.

**SOH scan.** The scanner uses ARM NEON to locate SOH (`0x01`) delimiter bytes 16 at a time:

```cpp
const uint8x16_t soh = vdupq_n_u8(0x01);

for (size_t i = 0; i + 16 <= len; i += 16) {
    uint8x16_t chunk = vld1q_u8(buf + i);
    uint8x16_t eq    = vceqq_u8(chunk, soh);
    uint64_t   mask  = vget_lane_u64(vreinterpret_u64_u8(vshrn_n_u16(
                           vreinterpretq_u16_u8(eq), 4)), 0);
    // each nibble in mask corresponds to one byte in chunk
    while (mask) {
        int pos = __builtin_ctzll(mask) >> 2;
        tokens[tokenCount++] = { i + pos, ... };
        mask &= mask - 1;
    }
}
```

Each SOH position marks the end of one `tag=value` field. The scan fills a `TokenArray` — a flat array of `(offset, length)` pairs — at the rate of one NEON instruction per 16 bytes of input. On the M1 Pro this processes approximately 10 GB/s, consuming a maximum-length FIX frame (typically 200–500 bytes) in under 50 ns.

**Frame boundary detection.** A complete FIX frame begins with `8=FIX` and ends with `10=NNN|`. The scanner maintains a lightweight state machine across `RxSpan` boundaries to handle frames that span multiple TCP receive buffers. When a complete frame is detected, the token array is handed to the field decoder.

**Field decode.** The field decoder iterates the token array and, for each `(tag, value)` pair, performs:

1. Tag parse: integer parse of the tag number from the byte range. Tags are at most 4 digits; this is a 4-branch switch or SWAR integer parse.
2. Value parse: type-specific decode depending on the tag:
   - `MsgType` (tag 35): single character or short string → enum lookup
   - `MsgSeqNum` (tag 34): integer parse
   - `SenderCompID` (tag 49), `TargetCompID` (tag 56): string copy into FIX internal format buffer
   - Application fields (tag 55 symbol, tag 38 qty, tag 44 price, etc.): type-specific parse into the internal format field union

The output is a `FIXMessage` in FIX internal format: a fixed-size struct of decoded field values with a presence bitmask indicating which tags were found in the frame.

### 6.3.5 Inbound Pipeline: Frame to Stream 1

**TCP lifecycle events.** In addition to FIX frames, the handler publishes two TCP lifecycle events on stream 1:

- **`SESSION_CONNECT`**: published immediately when a client TCP connection is accepted by the reactor and the fd is added to the known-connection table. The SBE payload carries the `sessionId` (an opaque identifier for this connection) and the peer address.
- **`SESSION_DISCONNECT`**: published immediately when the reactor detects a client disconnect (zero-length `recv`, `EPOLLRDHUP`, or error), before the fd is removed from the known-connection table.

Both events are SBE-encoded and published on stream 1 via the same `AeronCluster.offer()` path as FIX frames. Because stream 1 is a single ordered publication per Ingress process, the `SESSION_CONNECT` and `SESSION_DISCONNECT` events arrive at the cluster in strict causal order relative to every FIX frame from that session. The same pre-commit loss window that applies to FIX frames applies to these events — a leader crash between `offer()` and Raft commit loses the event; the client's subsequent Logon sequence makes the cluster's session state self-consistent regardless.

**FIX frame inbound processing.** For each complete `FIXMessage` produced by `PayloadDecoder`:

```
1. Admission check:
   a. Frame size ≤ maximum configured size.
   b. Checksum arithmetic: sum of all bytes before tag 10 mod 256 == tag 10 value.
   c. fd is in the known-connection table (i.e. the TCP accept has been processed and SESSION_CONNECT will be/has been published).
   → If any check fails: discard silently. No response sent. No cluster notification.

2. SBE encode:
   Extract MsgType, MsgSeqNum, SenderCompID, TargetCompID, and application fields.
   Write into SBE flyweight layout in the Aeron publication claim buffer.

3. Aeron offer (stream 1):
   publication.offer(claimBuffer, offset, length)
   → Returns: BACK_PRESSURED | NOT_CONNECTED | CLOSED | position

4. If BACK_PRESSURED:
   Spin on offer until it succeeds.
   The spin on offer blocks Core 2 from polling the RX ring.
   The stalled RX ring consumer stops the reactor from re-arming receives.
   The reactor's registered receive buffers fill and TCP flow control kicks in.
   → Back-pressure propagates from the cluster ring to the buy-side TCP socket.
```

Step 4 is the back-pressure chain. No explicit flow control message is sent; the TCP window naturally closes as the kernel receive buffers fill behind the stalled reactor.

### 6.3.6 Outbound Pipeline: Stream 6 to TCP

Core 2 runs a concurrent polling loop on Aeron stream 6 interleaved with its RX ring poll. Each fragment received from stream 6:

```
1. Decode SBE discriminator byte.
2. Decode clusterSessionPosition from the envelope header.
3. Decode SBE payload into FIX internal format fields.
4. Update shadowPhase if the discriminator implies a state change.
5. Encode outbound FIX frame using PayloadEncoder:
   a. Claim a slot in the encoding slab (pre-allocated per-frame buffer).
   b. Write Standard Header into the slot (BeginString, BodyLength placeholder,
      MsgType, SenderCompID, TargetCompID, MsgSeqNum, SendingTime).
   c. Write body fields.
   d. Write CheckSum trailer; back-fill BodyLength.
6. Write TxDescriptor { buf, len, fd } to TX ring → Core 1 picks up and submits IORING_OP_SEND.
```

The outbound path never blocks. If the TX ring is full (Core 1 is not draining fast enough), `txRing.write()` returns `INSUFFICIENT_CAPACITY` and step 6 spins. In practice the TX ring drains faster than it fills — a single `IORING_OP_SEND` submission takes under 100 ns, and outbound FIX frames arrive at most once per committed cluster entry.

### 6.3.7 Multi-Session Multiplexing

The reactor accepts connections from multiple buy-side clients on the same listen socket. Each client fd gets a `sessionTag` — a compact integer assigned at accept time. The `sessionTag` is embedded in the `RxSpan.fd` field and passed through to the SBE message on stream 1, where it becomes the Aeron `sessionId` that the cluster uses to route commands back on stream 6.

Core 2 maintains a `fd → SessionContext` table. Each context stores the `fd` field that is embedded in `RxDescriptor` messages to route received frames to the correct session:

```cpp
struct SessionContext {
    int32_t      fd;
    uint32_t     sessionTag;
    SessionPhase shadowPhase;
    uint32_t     partialFrameOffset;  // for frames spanning RxSpan boundaries
    uint8_t      partialFrameBuf[MAX_FIX_FRAME];
};
```

Multiple sessions share the same `PayloadDecoder` instance but each has its own `partialFrameOffset` and `partialFrameBuf` to handle TCP fragmentation independently. The token array is allocated on the stack per decode call; there is no heap allocation on the decode path.

---

## 6.4 Application Component (Application Process)

### 6.4.1 Role and Internal Structure

The Application Component is the C++ application process. It receives ordered commands from the cluster on stream 2 and is the pipeline stage that performs application-level decision-making: dispatching order events, running fast pre-checks, and managing the external risk system interaction. It consists of two threads:

```
stream 2 (cluster → AppWorker)
        │
        ▼
┌───────────────────────────────┐
│  Core 7                       │──► stream 3 (reject commands → cluster)
│  AppWorker                    │──► stream 4 (approved OrderCommand → Egress)
│                               │──► stream 6 (SEND/RESEND → Ingress FIX Session)
│  · stream 2 busy-spin poll    │
│  · Command dispatch (CRTP)    │
│  · SBE decode (zero-copy)     │
│  · Fast risk check (< 100 ns) │
└───────────────────────────────┘
        │ Ring (OrderRecord)
        ▼
┌───────────────────────────────┐
│  Core 8                       │──► stream 7 (risk responses → cluster)
│  Risk Thread                  │◄──► Risk System (proprietary TCP, 100–500 ms)
│                               │
│  · 25-slot correlation table  │
│  · Per-client FIFO queues     │
│  · Multiplexed RPC            │
└───────────────────────────────┘
```

### 6.4.2 AppWorker Duty Cycle

The AppWorker executes a tight busy-spin loop on Core 7. Each iteration:

```
1. Poll stream 2 (up to 10 fragments per poll call).
   For each fragment: dispatch on discriminator byte.

2. No fragments: continue spinning — no yield, no sleep.
```

The fragment limit of 10 per poll prevents a burst of cluster commands from starving the stream 6 publication path (outbound direction). After processing 10 fragments the loop re-enters its top, checking all subscriptions again.

**Command dispatch.** The discriminator byte in the stream 2 envelope selects the handler:

| Discriminator | Handler |
|---------------|---------|
| `FORWARD_APP` | Decode SBE payload → fast risk → ring to Risk Thread |
| `RISK_APPROVED` | Encode `OrderCommand` → publish on stream 4 |
| `RISK_REJECTED` | Encode reject → publish on stream 3 |
| `SEND` / `RESEND` | Forward SBE payload → publish on stream 6 |
| `GAP_FILL` | Forward sequence range → publish on stream 6 |
| `CONNECT` | Reset transient state; activate processing |
| `DISCONNECT` | Suppress all non-structural output; clear transient state |

### 6.4.3 SBE Decode: Zero-Copy Flyweight

`FORWARD_APP` payload decoding uses a generated SBE flyweight decoder that overlays the Aeron `DirectBuffer` directly — no copy of the message bytes is made. The flyweight exposes field accessors as inline functions:

```cpp
NewOrderSingleDecoder nos;
nos.wrapAndApplyHeader(buffer, offset, length);

ClOrdId  clOrdId  = nos.clOrdId();   // returns StringView into DirectBuffer
Side     side     = nos.side();       // returns enum from uint8
Qty      qty      = nos.orderQty();   // returns int64 scaled fixed-decimal
Price    price    = nos.price();      // returns int64 scaled fixed-decimal
Symbol   symbol   = nos.symbol();     // returns StringView
```

All accessors are `[[nodiscard]] constexpr` and resolve at compile time to a single memory read at a fixed offset from the flyweight base. There is no tag scanning, no string parsing, and no heap allocation on this path.

The CRTP dispatch template `FixMessageHandler<AppHandler>` selects the correct flyweight type from the SBE `templateId` field in the message header and invokes the corresponding `onNewOrderSingle`, `onOrderCancelRequest`, or `onOrderCancelReplaceRequest` method on the `AppHandler` implementation.

### 6.4.4 Fast Risk Check

Before enqueuing an order to the Risk Thread, the AppWorker performs an in-process pre-check using atomic load operations on shared position and rate counters. The check is designed to reject obviously invalid orders without consuming a risk slot:

```cpp
bool AppHandler::fastRiskCheck(const NewOrderSingleDecoder& nos) {
    auto& limits = riskLimits[nos.symbol()];
    int64_t pos     = limits.netPosition.load(std::memory_order_relaxed);
    int64_t notional = limits.notionalUsed.load(std::memory_order_relaxed);
    int64_t rate    = limits.ordersThisSecond.load(std::memory_order_relaxed);

    return pos     + nos.sideSign() * nos.orderQty() <= limits.maxNetPosition
        && notional + nos.price() * nos.orderQty()    <= limits.maxNotional
        && rate                                        <  limits.maxOrderRate;
}
```

All three reads are `memory_order_relaxed` — they read the last value visible to this core without a fence. A stale read is acceptable: the fast check is a best-effort filter, not a guarantee. The external risk system provides the authoritative decision. The check is bounded at **under 100 ns** (three cache-warm atomic loads).

Orders that fail the fast check are rejected immediately without entering the Risk Thread. The AppWorker publishes a reject command on stream 3; the cluster commits it and emits `SEND ExecutionReport(Rejected)` on stream 2.

### 6.4.5 Ring Buffer to Risk Thread: OrderRecord

Orders that pass the fast check are written to an Agrona `OneToOneRingBuffer` between Core 7 and Core 8. Unlike the ingress RX/TX rings, this ring carries the full decoded record as the message payload — there is no separate buffer pool because the data is already decoded field values, not raw bytes:

```cpp
struct OrderRecord {
    uint64_t clusterSessionPosition;  // idempotency key from stream 2 envelope
    uint64_t sessionId;               // identifies the buy-side client session
    ClOrdId  clOrdId;                 // client order ID
    Symbol   symbol;                  // instrument
    Side     side;                    // buy / sell
    Qty      orderQty;                // quantity (scaled integer)
    Price    price;                   // limit price (scaled integer, 0 for market)
    OrdType  ordType;                 // market / limit / stop
};
```

The Risk Thread polls the ring using Agrona's `MessageHandler` callback:

```cpp
orderRing.poll(
    [this](int32_t, AtomicBuffer& buf, int32_t offset, int32_t) {
        auto& rec = *reinterpret_cast<const OrderRecord*>(buf.buffer() + offset);
        dispatchToRiskSystem(rec);
    },
    FRAGMENT_LIMIT);
```

The record contains decoded field values; the Risk Thread requires no FIX or SBE awareness. The `clusterSessionPosition` is the anchor that connects the risk response back to the originating order in the Raft log.

If the ring is full (all 25 risk slots are occupied and the Risk Thread is not draining), `orderRing.write()` returns `INSUFFICIENT_CAPACITY` and the AppWorker spins retrying. This stalls the stream 2 poll loop, causing Aeron back-pressure to build on the cluster's stream 2 publication. The cluster's conductor thread detects the back-pressure and stops committing new messages until the ring clears. This is the back-pressure chain from the external risk system to the Raft log.

### 6.4.6 Risk Thread Event Loop

The Risk Thread on Core 8 runs a single-threaded event loop with three phases executed in sequence on each iteration:

**Phase 1 — Send.** If the global in-flight count is below 25, poll the inbound ring:

```
For each OrderRecord from ring:
    if inFlightByClient[sessionId] is occupied:
        push to pendingByClient[sessionId]   // preserve per-client order
    else:
        correlationId = nextCorrelationId()
        correlationTable[correlationId % 25] = { record, clusterSessionPosition }
        inFlightByClient[sessionId]          = correlationId
        send risk request to Big Iron:       // proprietary TCP frame
            { correlationId, symbol, side, qty, price, clOrdId }
        inFlightCount++

if inFlightCount == 25: skip to Phase 2 (no more sends until a slot frees)
```

**Phase 2 — Receive.** Poll the risk system TCP socket for responses (non-blocking `recv`):

```
For each response received:
    correlationId = response.correlationId
    record        = correlationTable[correlationId % 25]
    result        = response.approved ? APPROVED : REJECTED

    remove correlationTable[correlationId % 25]
    remove inFlightByClient[record.sessionId]
    inFlightCount--

    publish on stream 7 → cluster:
        { correlationId, clusterSessionPosition, result, reasonCode }

    if pendingByClient[record.sessionId] non-empty:
        dispatch head of queue immediately (without waiting for ring)
        → re-enters Phase 1 logic for that order only
```

The cluster receives the stream 7 message, commits it to the Raft log, and emits `RISK_APPROVED` or `RISK_REJECTED` on stream 2 back to the AppWorker. No application state changes before this commit.

**Phase 3 — Back-pressure.** If `inFlightCount == 25`, Phase 1 is skipped. The ring from the AppWorker stops being polled. `orderRing.write()` returns `INSUFFICIENT_CAPACITY` and the AppWorker spins retrying, stalling its stream 2 poll, which propagates back-pressure to the cluster.

### 6.4.7 Per-Client Order Serialisation

The `inFlightByClient` and `pendingByClient` structures enforce that at most one risk request per client session is in-flight at any time:

```
inFlightByClient:  HashMap<sessionId, correlationId>  — size ≤ 25
pendingByClient:   HashMap<sessionId, Queue<OrderRecord>>
```

When a response arrives for client A, the Risk Thread immediately dispatches the head of `pendingByClient[A]` (if non-empty) before returning to the main send loop. This minimises head-of-line blocking within a single client's order stream. Across different clients, responses arrive and are dispatched independently in whatever order the risk system returns them.

The per-client serialisation guarantee is what makes risk evaluation correct: the risk system always sees orders from a given client in the order they were committed to the Raft log, and it accumulates position against the result of every prior order for that client before approving or rejecting the next.

### 6.4.8 SEND and RESEND Handling

When the cluster emits `SEND` or `RESEND` on stream 2 (for session-layer or application-layer messages routed back to a buy-side client), the AppWorker does not decode or modify the payload. It reads the SBE envelope from stream 2 and publishes it verbatim on stream 6 toward the Ingress FIX Session component. The FIX Session component (Core 2) decodes the SBE, encodes the FIX frame, and delivers it to the client's TCP connection.

The AppWorker is the routing hop between the cluster's ordered output and the session handler; it does not participate in the content of outbound session or application messages.

---

## 6.5 Egress Component (Egress Process)

### 6.5.1 Role and Internal Structure

The Egress Component is the C++ egress process. It owns the sell-side FIX session — the single persistent, bi-directional TCP connection to the exchange — and is the only process that sends orders to and receives execution reports from the sell-side gateway. It consists of two cooperating threads:

```
stream 4 (AppWorker → EgressWorker: approved OrderCommands)
        │
        ▼
┌───────────────────────────────┐  TX Ring  ┌─────────────────────────────┐
│  Core 9                       │ ────────► │  Core 10                    │
│  EgressWorker                 │           │  Egress Reactor             │
│                               │ ◄──────── │                             │
│  · stream 4 subscriber        │  RX Ring  │  · epoll event loop         │
│  · Egress risk accounting     │           │  · Single exchange TCP fd   │
│  · FIX encode (PayloadEncoder)│           │  · recv / send              │
│  · FIX decode (PayloadDecoder)│           │  · SO_BUSY_POLL             │
│  · Endpoint failover          │           │  · io_uring on bare metal   │
│  · stream 5 publisher         │           └─────────────────────────────┘
└───────────────────────────────┘                       │  ▲
                                              TCP send  │  │ TCP recv
                                              (orders)  │  │ (exec reports)
                                                        ▼  │
                                              Sell-side Exchange
```

Unlike the ingress reactor, which manages many client fds, the egress reactor manages a **single TCP connection** to the exchange at any time. This simplifies the event loop considerably — there is no fd routing table, no per-connection partial-frame buffer complexity across multiple sessions, and no accept path in normal operation.

### 6.5.2 Egress Reactor (Core 10)

The egress reactor uses the same epoll-based design as the ingress reactor (§6.3.2), with three differences reflecting the single-connection, initiator role:

**Connect path.** The reactor is responsible for establishing the TCP connection. It calls `connect(fd, endpoint, ...)` in non-blocking mode and monitors the fd with `EPOLLOUT` to detect completion. On `EPOLLOUT` after a `connect`, it checks `getsockopt(SO_ERROR)` to confirm success, then arms `EPOLLIN | EPOLLOUT | EPOLLRDHUP | EPOLLET` for normal operation and signals the EgressWorker that the connection is ready.

**Single-fd loop.** With only one exchange fd active, the `epoll_wait` loop is simplified: there is no fd dispatch table. Events on the single registered fd are handled directly:

```cpp
while (running) {
    int n = epoll_wait(epfd, &event, 1, /*timeout=*/0);
    if (n > 0) {
        auto ev = event.events;
        if (ev & (EPOLLHUP | EPOLLERR | EPOLLRDHUP)) { handleDisconnect(); }
        else {
            if (ev & EPOLLIN)  { drainReceive(); }
            if (ev & EPOLLOUT) { flushPendingSend(); }
        }
    }
    drainTxSpsc();   // forward encoded FIX frames from Core 9
}
```

**Reconnect path.** On disconnect the reactor closes the fd, removes it from epoll, and signals the EgressWorker via a flag. The EgressWorker selects the next endpoint from its priority list and instructs the reactor to initiate a new `connect`. The reactor creates a fresh non-blocking socket, registers it with epoll, and initiates the connect. No existing state is carried from the old fd.

**io_uring on bare metal.** The same io_uring alternative described in §6.3.2.1 applies to the egress reactor. With a single fd the io_uring advantage is less pronounced than on the ingress (fewer concurrent receives), but the registered-buffer zero-copy benefit on the receive path (execution reports) is still measurable under high fill rates. Configure with `egress.reactor.mode = io_uring`.

### 6.5.3 EgressWorker: Outbound Path (Orders)

The EgressWorker busy-spins on stream 4, which carries `OrderCommand` entries published by the AppWorker after the cluster commits `RISK_APPROVED`. For each `OrderCommand`:

```
1. Decode the OrderCommand SBE payload:
   { clusterSessionPosition, sessionId, clOrdId, symbol, side, qty, price, ordType }

2. Egress risk check (position and notional accounting):
   net_position[symbol]  += side_sign × qty
   notional_used[symbol] += price × qty
   If either breaches the egress limit: drop order, publish reject on stream 5.

3. Route to SellSideSession:
   Look up the active SellSideSession via hash on symbol or client.

4. FIX encode using PayloadEncoder:
   a. Write Standard Header (MsgType=D, SenderCompID, TargetCompID,
      MsgSeqNum = sellSideSession.nextOutboundSeqNum++, SendingTime).
   b. Write body: ClOrdId, Symbol, Side, OrderQty, Price, OrdType, TransactTime.
   c. Write CheckSum trailer; back-fill BodyLength.

5. Write TxDescriptor { buf_ptr, length, exchangeFd } to TX ring → Egress Reactor.
```

The egress risk check (step 2) uses non-atomic counters — there is only one writer and one reader (both on Core 9). It is an egress-side position gate, not a substitute for the external risk system. Its counters are updated by ExecutionReport fills (step covered in §6.5.4), ensuring position tracks actual fills rather than pending orders.

The sell-side FIX session sequence numbers are owned by the cluster, not by Core 9. The EgressWorker reads the current `outboundSeqNum` from the `SEND` command envelope emitted by the cluster on stream 2; it writes this sequence number into the FIX Standard Header verbatim. It does not maintain its own sequence counter. After sending, it publishes a sent-confirmation back to the cluster on stream 3 so the cluster can advance `outboundSeqNum` in the committed log. On reconnect the cluster supplies the correct `outboundSeqNum` in the first `SEND Logon` command, and the exchange continues from that position.

### 6.5.4 EgressWorker: Inbound Path (Execution Reports)

Execution reports arrive from the exchange on the same TCP connection and travel in the opposite direction. The Egress Reactor delivers received-byte descriptors to the EgressWorker via the RX ring. The EgressWorker polls the ring and decodes each report using `PayloadDecoder`:

```
1. PayloadDecoder: SIMD SOH scan → token array → field decode.
   Extract: MsgType (tag 35), ExecType (tag 150), OrdStatus (tag 39),
            ClOrdId (tag 11), Symbol (tag 55), LastQty (tag 32),
            LastPx (tag 31), CumQty (tag 14), LeavesQty (tag 151).

2. Sequence number validation:
   Check inbound MsgSeqNum (tag 34) against sellSideSession.nextInboundSeqNum.
   Gap → send ResendRequest to exchange. Duplicate → discard.

3. Position update (for fills):
   If ExecType = TRADE (F): adjust position counters.
   If ExecType = CANCELED / REJECTED: release reserved notional.

4. SBE encode ExecutionReport for the cluster:
   Write into Aeron publication claim buffer (stream 5).
   Carry: clOrdId (matches original OrderCommand), sessionId (buy-side routing),
          execType, ordStatus, lastQty, lastPx, cumQty, leavesQty.

5. Publish on stream 5 → Aeron Cluster.
   The cluster routes the ExecutionReport to the correct buy-side client session
   via stream 2 SEND command → stream 6 → Ingress FIX Session → client TCP.
```

The buy-side routing key is `clOrdId`, which maps back to the `sessionId` of the originating buy-side client. The EgressWorker maintains a `clOrdId → sessionId` table populated when each order is sent in step 5 of §6.5.3. On receiving the ExecutionReport, it looks up the buy-side `sessionId` and embeds it in the stream 5 SBE message.

### 6.5.5 Endpoint Failover

The EgressWorker maintains a prioritised endpoint list loaded from session configuration at startup:

```
endpoints:
  - host: primary.exchange.com   port: 9000   priority: 1
  - host: backup.exchange.com    port: 9000   priority: 2
  - host: dr.exchange.com        port: 9001   priority: 3
```

On disconnect the EgressWorker executes the following reconnect loop, always beginning at the top of the priority list:

```
currentIndex = 0
loop:
    endpoint = endpoints[currentIndex]
    signal reactor: connect to endpoint
    wait for connect result (timeout: connectTimeoutMs)
    if connected:
        send FIX Logon (MsgSeqNum negotiated or reset)
        wait for Logon-Ack (timeout: logonTimeoutMs)
        if Logon-Ack received → session ACTIVE, resume stream 4 processing
        else → disconnect; advance currentIndex
    else:
        advance currentIndex
    if currentIndex == endpoints.size():
        currentIndex = 0
        sleep backoffMs   // back-off before cycling again
```

During the reconnect window, stream 4 is not drained. `OrderCommand` entries accumulate in the Aeron IPC ring. Back-pressure builds on the AppWorker's stream 4 publication after the ring fills, which propagates back to the risk slot mechanism and ultimately to the buy-side TCP receive window. No orders are lost — they queue in Aeron IPC until the exchange connection is restored.

### 6.5.6 Outbound Message Buffer

The EgressWorker maintains a local circular buffer of the last N outbound FIX messages encoded for the exchange (the sell-side equivalent of `MemoryStorage` in the cluster). This buffer is used to service the exchange's `ResendRequest` during reconnect sequence negotiation.

```
sellSideOutboundBuffer: CircularBuffer<EncodedFIXMessage, N>
  indexed by sellSide MsgSeqNum
  capacity N = 2500 messages (same as cluster MemoryStorage)
```

When the exchange sends a `ResendRequest`, the EgressWorker reads the requested range from `sellSideOutboundBuffer` and retransmits with `PossDupFlag=Y` (tag 43=Y). Session-level messages (Heartbeat, TestRequest) within the resend range are replaced by a SequenceReset-GapFill, consistent with standard FIX ResendRequest servicing.

The buffer is local to Core 9 and is not replicated or snapshotted. On cluster failover (new leader), the new leader's EgressWorker starts with an empty buffer and negotiates sequence continuity with the exchange via the Logon handshake. If the exchange requests a range the new EgressWorker cannot service, it sends a SequenceReset-Reset to re-synchronise sequence numbers.