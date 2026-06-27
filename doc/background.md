# Chapter 1: Background

## 1.1 The FIX Protocol

### 1.1.1 Origins and Purpose

The Financial Information eXchange (FIX) protocol is an open electronic communications standard for the real-time exchange of securities transaction information. Originally developed in 1992 between Fidelity Investments and Salomon Brothers as a bilateral equity trading link, it has since become the dominant messaging standard for pre-trade and trade communication across virtually every asset class and execution venue globally.

FIX is not a transport protocol — it defines a message format and a session layer. It runs over TCP/IP and relies on a persistent, ordered, full-duplex connection between two parties, referred to as the *initiator* (the client, typically a buy-side firm or algorithmic trading engine) and the *acceptor* (the server, typically an exchange or broker).

The protocol exists in several versions. **FIX 4.2** (1997) introduced the `ExecutionReport` message and remains the most widely deployed version in production equity and derivatives markets. **FIX 4.4** (2003) added support for cross-currency trades, multi-leg instruments, and an expanded set of session management primitives. **FIXT 1.1 / FIX 5.0** decoupled the session layer from the application layer, but adoption has been slow — the majority of institutional venues still accept 4.2 or 4.4.

This gateway targets **FIX 4.2 and FIX 4.4** session semantics.

---

### 1.1.2 Wire Format

A FIX message is a sequence of *tag=value* pairs separated by the **SOH** (Start of Header) character, ASCII `0x01`. The format is entirely text-based and human-readable when SOH is rendered as `|`:

```
8=FIX.4.2|9=65|35=D|49=CLIENT1|56=EXCHANGE|34=42|52=20240101-12:00:00.000|
11=ORD001|55=AAPL|54=1|60=20240101-12:00:00.000|38=100|40=2|44=150.00|10=127|
```

Every message is structured as three logical sections:

#### Standard Header

The header fields appear first in every message, in a mandated order:

| Tag | Name | Description |
|-----|------|-------------|
| 8 | `BeginString` | Protocol version, e.g. `FIX.4.2`. Always the first tag. |
| 9 | `BodyLength` | Byte count from the first byte after `BodyLength` up to and including the delimiter before `CheckSum`. Always the second tag. |
| 35 | `MsgType` | Single or two-character code identifying the message type. Always the third tag. |
| 49 | `SenderCompID` | Identifier of the party sending the message. |
| 56 | `TargetCompID` | Identifier of the intended recipient. |
| 34 | `MsgSeqNum` | Monotonically increasing sequence number for this session. |
| 52 | `SendingTime` | UTC timestamp at time of transmission, format `YYYYMMDD-HH:MM:SS.sss`. |

#### Body

The body contains all application-layer fields specific to the message type. Field ordering within the body is generally non-mandated (with some exceptions for repeating groups), which is why a parser cannot rely on positional assumptions.

#### Standard Trailer

The trailer always closes the message:

| Tag | Name | Description |
|-----|------|-------------|
| 10 | `CheckSum` | Three-digit ASCII representation of the sum of all byte values in the message modulo 256, zero-padded. Always the last field. |

The `BodyLength` (tag 9) and `CheckSum` (tag 10) fields serve as a framing and integrity mechanism. `BodyLength` allows a receiver to know exactly how many bytes to read before expecting the trailer; `CheckSum` provides a basic corruption guard.

---

### 1.1.3 Sequence Numbers and the Session State Machine

The most important concept in the FIX session layer is the **sequence number**. Every message sent on a session carries a `MsgSeqNum` (tag 34) that increments by exactly one per message, starting at 1 when the session is first established. Sequence numbers are *persistent across TCP reconnections* for the lifetime of a logical session — they reset to 1 only on an explicit negotiated reset or at a configurable daily reset time.

This design means the FIX session layer provides an application-level ordered delivery guarantee on top of TCP. A receiver that detects a gap in the incoming sequence (received seq N+2 when expecting N+1) knows that a message was lost and must request retransmission before processing any subsequent messages.

The FIX session state machine governs connection lifecycle. The states and transitions are:

```
               ┌────────────────────────────────────────────────────┐
               │                  DISCONNECTED                      │
               └───────────────────────┬────────────────────────────┘
                                       │ TCP connect (initiator)
                                       │ or TCP accept (acceptor)
                                       ▼
               ┌────────────────────────────────────────────────────┐
               │              LOGON PENDING                         │
               │   Send/Receive Logon (35=A)                        │
               └───────────────────────┬────────────────────────────┘
                                       │ Logon accepted
                                       ▼
               ┌────────────────────────────────────────────────────┐
               │                   ACTIVE                           │◄──────────────────────┐
               │   Normal message exchange                          │                       │
               └─────────────────┬──────────────────────┬──────────┘                       │
                                 │ No message for        │ Gap detected                     │
                                 │ HeartBtInt seconds    │ (incoming seq skip)              │
                                 ▼                       ▼                                  │
               ┌──────────────────────┐  ┌─────────────────────────┐                       │
               │  Send Heartbeat (0)  │  │  Send ResendRequest (2)  │                       │
               │  or TestRequest (1)  │  │  or SequenceReset (4)    │─────────────────────►─┘
               └──────────────────────┘  └─────────────────────────┘
                                 │
                                 │ Logout initiated
                                 ▼
               ┌────────────────────────────────────────────────────┐
               │                  LOGOUT PENDING                    │
               │   Send/Receive Logout (35=5)                       │
               └───────────────────────┬────────────────────────────┘
                                       │
                                       ▼
               ┌────────────────────────────────────────────────────┐
               │                  DISCONNECTED                      │
               └────────────────────────────────────────────────────┘
```

**Key session-layer messages:**

| MsgType | Name | Purpose |
|---------|------|---------|
| `A` | Logon | Opens a session. Contains `HeartBtInt` (tag 108), the agreed heartbeat interval in seconds, and optionally `ResetSeqNumFlag` (tag 141) to force a sequence reset. |
| `0` | Heartbeat | Sent when no message has been transmitted for `HeartBtInt` seconds. Keeps the TCP connection alive and signals liveness. |
| `1` | TestRequest | Sent when no message has been *received* for `HeartBtInt` seconds. The remote party must respond with a Heartbeat bearing the `TestReqID` (tag 112). If no response arrives within a second `HeartBtInt` window, the session is torn down. |
| `2` | ResendRequest | Requests retransmission of messages in a sequence range. `BeginSeqNo` (tag 7) and `EndSeqNo` (tag 16, where 0 means "to the current end") define the range. |
| `3` | Reject | Session-level reject for a malformed or invalid message. Includes `RefSeqNum` (tag 45) and `Text` (tag 58). |
| `4` | SequenceReset | Used in response to a `ResendRequest` to skip over messages that need not or cannot be retransmitted (e.g. session-level heartbeats). `GapFillFlag` (tag 123) = `Y` means "I am filling a gap"; `NewSeqNo` (tag 36) sets the next expected inbound sequence. |
| `5` | Logout | Initiates an orderly session termination. Either party may send it; the receiver must echo it back before closing the TCP connection. |

---

### 1.1.4 Application-Layer Messages

Beyond the session layer, FIX defines a rich vocabulary of order management and market data messages. The subset relevant to this gateway:

| MsgType | Name | Description |
|---------|------|-------------|
| `D` | NewOrderSingle | The primary order entry message. Carries instrument (`Symbol`, tag 55), side (`Side`, tag 54: `1`=Buy, `2`=Sell), quantity (`OrderQty`, tag 38), order type (`OrdType`, tag 40: `1`=Market, `2`=Limit), price (`Price`, tag 44), and a client-assigned `ClOrdID` (tag 11). |
| `8` | ExecutionReport | The omnibus execution message. Covers order acknowledgement (`ExecType`=`0`), partial fill (`ExecType`=`1`), full fill (`ExecType`=`2`), cancellation (`ExecType`=`4`), and rejection (`ExecType`=`8`). Carries fill price (`LastPx`, tag 31), fill quantity (`LastQty`, tag 32), cumulative filled quantity (`CumQty`, tag 14), and leaves quantity (`LeavesQty`, tag 151). |
| `F` | OrderCancelRequest | Request to cancel an open order, identified by `OrigClOrdID` (tag 41) or `OrderID` (tag 37). |
| `G` | OrderCancelReplaceRequest | Request to modify quantity or price on an open order. |
| `9` | OrderCancelReject | Rejection of a cancel or replace attempt, with `CxlRejReason` (tag 102). |

---

### 1.1.5 Repeating Groups

FIX supports *repeating groups*, a structured list of fields embedded within a message body. A group begins with a *delimiter tag* that carries the count of repetitions, followed by that many instances of a sub-block of fields. The first tag of each instance is defined as the *group delimiter* and must appear exactly once per instance to allow parsing.

An example is the `NoAllocs` (tag 78) group in `NewOrderSingle`, used to specify allocation split across multiple accounts. A parser must track group nesting depth and delimiter boundaries — a flat tag-scanning approach breaks down on any message containing groups.

---

### 1.1.6 Performance Characteristics and Why Text Parsing Is Expensive

Despite its simplicity in concept, parsing FIX at high throughput is non-trivial:

- **Variable field ordering**: fields (other than the mandatory header sequence) may appear in any order, requiring a tag-keyed lookup rather than positional reads.
- **Variable field lengths**: numeric and string values have no fixed byte width; a scanner must locate the `=` and `|` delimiters around each value.
- **No binary framing**: unlike SBE or Protobuf, there is no length prefix on individual fields. The only framing mechanism is the `BodyLength` header and the SOH delimiters.
- **Dense allocation pressure**: naive implementations build `std::string` or `std::map<int,std::string_view>` structures per message, creating significant heap pressure on hot paths.

The `simdfix` library (the FIX codec component of this gateway) addresses these costs by using ARM NEON SIMD instructions to vectorize the delimiter scan, zero-copy `std::string_view` indexing over the raw wire buffer, and a compile-time tag lookup table to bind field offsets statically.

---

## 1.2 Aeron

### 1.2.1 What Aeron Is

Aeron is an open-source, high-performance messaging transport library developed by Adaptive Financial Consulting. It is designed for environments where **latency predictability and throughput** are more important than ease of use — specifically algorithmic trading, market data distribution, and financial order management systems.

Aeron's key properties are:

- **Lock-free data structures throughout** — no mutexes on any hot path; progress guarantees derived from hardware-level compare-and-swap (CAS) primitives.
- **Zero-copy delivery** — subscribers read directly from shared memory-mapped log files; the message bytes are never copied between producer and consumer.
- **Back-pressure over loss** — unlike UDP multicast systems that simply drop messages, Aeron applies explicit flow control: a slow subscriber causes the publisher to slow down rather than lose data.
- **Pluggable transports** — Aeron supports `aeron:udp` (point-to-point and multicast over the network) and `aeron:ipc` (inter-process via shared memory on a single host). This gateway uses `aeron:ipc` exclusively for all intra-host communication.

---

### 1.2.2 The Media Driver

The **Aeron Media Driver** (`aeronmd`) is a background process (or embedded thread group) that owns all shared-memory resources and all network sockets. Client applications (whether C++ or Java) connect to the media driver via a *conductor* ring buffer in shared memory and issue commands (subscribe, publish, close) through it. The media driver executes all I/O, handles flow control, and manages the lifecycle of publications and subscriptions.

The media driver runs three internal threads:

| Thread | Role |
|--------|------|
| **Conductor** | Processes client commands, manages publication/subscription lifecycle, coordinates metadata updates. |
| **Sender** | Reads from publication log buffers and transmits data (for UDP channels). For `aeron:ipc`, data is already in shared memory and this thread is largely idle. |
| **Receiver** | Receives network data (UDP), writes into subscriber log buffers, sends NAKs and SMs. For `aeron:ipc`, data arrives via shared memory and this thread handles status message bookkeeping. |

In this gateway all Aeron endpoints run on a single physical host. The media driver is deployed as a standalone process (`aeronmd`) pinned to a dedicated core set (Cores 5–8 in the thread topology). C++ components link against `libaeron` and communicate with the driver through the `CncFile` (Command and Control file), a memory-mapped region at a well-known path (`/dev/shm/aeron-<uid>/` by default).

---

### 1.2.3 Publications and Subscriptions

The Aeron messaging model is a **unidirectional channel** between one publisher and one or more subscribers. A *channel* is identified by a URI string (`aeron:ipc` for shared memory, `aeron:udp?endpoint=host:port` for UDP). A *stream ID* is an integer that multiplexes multiple independent logical streams over a single channel. The combination of channel and stream ID uniquely identifies a data flow.

**Publication lifecycle:**

1. A client calls `aeron::addPublication(channel, streamId)` through the conductor ring buffer.
2. The media driver allocates a **log buffer** — a memory-mapped file composed of three equally-sized *term buffers* that rotate in round-robin. The default term buffer size is 16 MB (configurable).
3. The driver replies with a publication image handle. The client holds a `Publication` object and calls `offer(buffer, offset, length)` to write messages.
4. `offer` performs a single CAS on the *term tail counter* to claim a slot, then writes the header and payload into the term buffer directly. If the next available position would overflow the flow-control window (the subscriber has not kept up), `offer` returns a back-pressure code rather than writing.

**Subscription lifecycle:**

1. A client calls `aeron::addSubscription(channel, streamId)` with an `onAvailableImageHandler` callback.
2. When a matching publication connects, the driver invokes the callback with an `Image` object.
3. The subscriber calls `image.poll(fragmentHandler, fragmentLimit)` on each duty cycle. `poll` reads from the current position in the log buffer, invokes the `fragmentHandler` for each complete message, and advances the *subscriber position counter*.
4. The subscriber position counter (in shared memory) is read by the driver's flow control mechanism to determine how far the publisher is allowed to advance.

The absence of message copying is fundamental: both publisher and subscriber operate on the **same physical memory pages**, mapped into their respective address spaces by the OS. A `memcpy` never occurs between producer and consumer for `aeron:ipc`.

---

### 1.2.4 Log Buffer Architecture

The Aeron log buffer is the central data structure underpinning all publications. It consists of:

- **Three term buffers**: each `termBufferLength` bytes (power of two, default 16 MB). At any moment, one term is *active* (being written), one is *clean* (subscribers are still reading), and one is *dirty* (being zeroed for future use by a background cleaner thread).
- **Log meta-data buffer**: a small region appended after the terms that holds the *active term count*, *initial term ID*, per-term tail counters, and subscriber position counters.

```
┌─────────────────────────────────────────────────────────────┐
│                    Aeron Log Buffer                         │
│                                                             │
│  ┌─────────────┐  ┌─────────────┐  ┌─────────────┐         │
│  │   Term 0    │  │   Term 1    │  │   Term 2    │         │
│  │  (16 MB)    │  │  (16 MB)    │  │  (16 MB)    │         │
│  └─────────────┘  └─────────────┘  └─────────────┘         │
│                                                             │
│  ┌─────────────────────────────────────────────────────┐   │
│  │               Log Meta-Data                         │   │
│  │  activeTermCount | tailCounters[3] | subPositions   │   │
│  └─────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

Within each term, messages are laid out contiguously with a fixed-size **frame header** (32 bytes) preceding each payload:

| Bytes | Field |
|-------|-------|
| 0–3 | Frame length (signed; negative = padding) |
| 4 | Version |
| 5 | Flags (`BEGIN_FRAG`, `END_FRAG` for fragmented messages) |
| 6–7 | Type (`DATA` = 0x01, `PAD` = 0x02) |
| 8–11 | Term offset |
| 12–15 | Session ID |
| 16–19 | Stream ID |
| 20–23 | Term ID |
| 24–31 | Reserved (used for SBE schema ID in clustered deployments) |

The claim-and-write model means concurrent publishers on the same stream each CAS the tail counter independently and then write their frames into their claimed slots. The subscriber's `poll` loop skips any slot whose frame length is still zero (CAS won but write not yet complete), providing a natural ordering guarantee.

---

### 1.2.5 Simple Binary Encoding (SBE)

SBE is the binary serialization format used between all Aeron Cluster participants and between the C++ gateway and the Java sequencer. It is the antithesis of FIX text encoding: a message is a fixed-layout, zero-copy, pointer-overlay over a raw byte buffer. There is no tag scanning, no delimiter searching, and no heap allocation.

An SBE schema is an XML file that defines messages as groups of typed fields with fixed offsets and sizes:

```xml
<sbe:messageSchema ...>
  <types>
    <composite name="messageHeader">
      <type name="blockLength" primitiveType="uint16"/>
      <type name="templateId"  primitiveType="uint16"/>
      <type name="schemaId"    primitiveType="uint16"/>
      <type name="version"     primitiveType="uint16"/>
    </composite>
  </types>

  <sbe:message name="NewOrderSingle" id="1" blockLength="64">
    <field name="clOrdId"   id="11"  type="uint64"  offset="0"/>
    <field name="symbol"    id="55"  type="char"    length="8" offset="8"/>
    <field name="side"      id="54"  type="SideEnum" offset="16"/>
    <field name="orderQty"  id="38"  type="int64"   offset="24"/>
    <field name="ordType"   id="40"  type="OrdTypeEnum" offset="32"/>
    <field name="price"     id="44"  type="int64"   offset="40"/>
    <field name="transactTime" id="60" type="uint64" offset="48"/>
  </sbe:message>
</sbe:messageSchema>
```

The `sbe-tool` generator (a Java program invoked at build time) reads this schema and emits:

- **C++ flyweight classes**: thin wrappers that `reinterpret_cast` a pointer into the buffer at the known offset. Accessing `clOrdId()` is a single load instruction with a literal byte offset.
- **Java flyweight classes**: equivalent off-heap flyweights using `sun.misc.Unsafe` for direct memory access without GC pressure.

The same schema file governs both C++ and Java codecs, making the wire format a compile-time contract shared across the language boundary. There is no marshalling step — the encoder writes directly into the Aeron log buffer, and the decoder reads from it.

---

### 1.2.6 Aeron Cluster

Aeron Cluster is a fault-tolerant, replicated state machine built on top of Aeron transport. It uses the **Raft consensus algorithm** to maintain a consistent, ordered log of commands across a cluster of nodes, ensuring that the cluster continues operating correctly as long as a majority quorum (`⌊N/2⌋ + 1` nodes out of `N`) remains available.

The cluster presents a clean abstraction:

- **Ingress**: clients submit commands to the cluster leader via a regular Aeron publication to a well-known cluster ingress channel.
- **Sequencer**: the leader appends each received command to the Raft log, assigns it a monotonically increasing *cluster session sequence position*, and replicates it to followers before acknowledging.
- **Application**: each cluster node runs a `ClusteredService` implementation that applies the committed log entries in order. Because every node sees the same log in the same order, all state machines remain identical.
- **Egress**: the cluster sends responses back to clients via a dedicated egress publication.

```
                ┌──────────────────────────────────────────────────┐
                │              Aeron Cluster (3 nodes)             │
                │                                                  │
  Client ──────►│  Ingress ──► [Leader]  ──► Raft Log             │──────► Egress ──► Client
  (Publication)  │              │                                  │        (Subscription)
                │              ├──► Replicate ──► [Follower 1]    │
                │              └──► Replicate ──► [Follower 2]    │
                │                                                  │
                │  ClusteredService.onSessionMessage() called on   │
                │  each node when log entry is committed           │
                └──────────────────────────────────────────────────┘
```

**Key Aeron Cluster concepts relevant to this gateway:**

**Cluster Session**: When a client connects to the cluster ingress, the cluster establishes a *cluster session* identified by a `clusterSessionId`. The gateway's C++ ingress engine uses a single cluster session to submit all inbound FIX orders.

**Raft Log**: The Raft log is the durable, ordered record of all commands. Each entry carries the `clusterSessionId`, a `clusterSessionSequence`, and the raw payload bytes. The cluster guarantees that once an entry is committed (acknowledged by a quorum), it will be applied in that position on every live and future node, making the log the single source of truth for cluster state.

**Snapshots**: Periodically (or on request), the `ClusteredService` may take a *snapshot* of its current state to a persistent store managed by Aeron Archive. On a cold start or failover, a node loads the most recent snapshot and then replays only the log entries after the snapshot position, rather than replaying the entire log from position zero.

**Timer Service**: The `ClusteredService` interface exposes a cluster-managed timer that fires at a cluster-global time, ensuring deterministic behavior even after failover — the timer state is part of the snapshotted cluster state.

---

### 1.2.7 Aeron Archive

Aeron Archive is a recording and replay service for Aeron streams. It runs as a thread group within the media driver process and is controlled via its own command-and-control channel.

**Recording**: A publication can be marked for recording. The archive writes every message to a local *recording* on NVMe storage, identified by a `recordingId`. The recording grows continuously as new messages arrive.

**Replay**: A client issues an `archiveProxy.replay(recordingId, position, length, channel, streamId)` command. The archive daemon seeks to `position` in the recording file, reads `length` bytes (or streams to the end if `length == Long.MAX_VALUE`), and publishes them onto the requested `channel`/`streamId`. The replay is entirely asynchronous — the requesting thread immediately regains control while the archive daemon streams data independently.

**Relevance to this gateway**: Aeron Archive serves two critical roles:

1. **Egress journaling**: every outbound FIX message (formatted `ExecutionReport`, order acknowledgement, etc.) published on the egress channel is continuously archived. This creates a durable, replayable record of every byte ever sent to every FIX client.

2. **Gap fill delivery**: when a FIX client sends a `ResendRequest`, the gateway does not re-scan application state to reconstruct the missing messages. Instead, it fires an asynchronous archive replay of the relevant egress recording segment directly to a dedicated socket, keeping the primary egress path completely unblocked.

---

### 1.2.8 Flow Control and Back-Pressure

Aeron's flow control is implemented via the *subscriber position counter*, a 64-bit monotonically increasing byte offset stored in the log meta-data buffer (shared memory). The publisher's `offer` call checks whether the position it wishes to claim would exceed the flow-control window beyond the slowest subscriber's position. If it would, `offer` returns `Publication.BACK_PRESSURED` without writing anything.

This mechanism provides an explicit, cooperative back-pressure signal that the application can act on — spinning, yielding, or alerting an operator — without silently dropping messages. For this gateway, back-pressure from the Aeron Cluster ingress is treated as a critical condition: the FIX session layer will stop consuming inbound orders and will eventually apply TCP back-pressure to the client.

The flow-control window is controlled by the `aeron.publication.window.length` system property (default: 1/4 of the term buffer length, i.e. 4 MB for 16 MB terms). Tuning this window is one of the primary levers for balancing throughput against memory usage under burst conditions.

---

### 1.2.9 Why Aeron for This Gateway

The choice of Aeron over alternatives (Kafka, NATS, ZeroMQ, shared-memory ring buffers) is driven by a combination of properties no other library offers simultaneously:

| Property | Aeron | Kafka | ZeroMQ |
|----------|-------|-------|--------|
| Sub-microsecond `aeron:ipc` latency | Yes | No | No |
| Built-in Raft consensus | Aeron Cluster | Via external ZK | No |
| Integrated replay / journaling | Aeron Archive | Yes (log retention) | No |
| Zero-copy shared memory | Yes | No | Partial |
| Flow control (no loss) | Yes | Yes | Yes (HWM drop) |
| Cross-language (C++/Java) | Yes | Yes | Yes |
| Cluster failover without client reconnect | Yes | Partial | No |

The combination of `aeron:ipc` for sub-microsecond intra-host delivery, Aeron Cluster for durable sequencing without a separate consensus dependency, and Aeron Archive for gap-fill replay makes Aeron the natural fit for a latency-sensitive, fault-tolerant FIX gateway.

---

## 1.3 Distributed Systems

### 1.3.1 The Replicated State Machine Model

The theoretical foundation of Aeron Cluster — and of most fault-tolerant distributed systems — is the **Replicated State Machine (RSM)** model. The idea is simple: if every replica in a group starts from the same initial state and applies the same sequence of inputs in the same order, every replica will arrive at the same state after each input. A consensus protocol such as Raft is the mechanism that ensures all replicas agree on both the content and the order of each input.

A state machine in this model consists of:

- **State** — the complete set of variables that determine the machine's behaviour at a given point in time. For the FIX gateway this is: inbound and outbound FIX sequence numbers, session phase, and the resend buffer.
- **Inputs** — externally submitted commands. In Aeron Cluster these arrive via the cluster ingress channel. Each input is a byte buffer; the state machine interprets it.
- **Transition function** — the deterministic function that maps `(currentState, input) → nextState`. This is the `ClusteredService.onSessionMessage()` callback.
- **Outputs** — side effects produced during the transition: publishing commands to C++ workers, scheduling timers, taking snapshots.

The critical invariant is **determinism**: given the same state and the same input, the transition function must always produce exactly the same next state and the same outputs, on every node, without exception.

Raft enforces agreement on inputs and their order. It does not enforce determinism of the application logic — that is entirely the programmer's responsibility. A single non-deterministic operation inside `onSessionMessage` will silently cause replicas to diverge. Divergence is not immediately visible; the cluster continues operating, but follower state drifts away from leader state. On the next failover, the new leader produces incorrect outputs.

---

### 1.3.2 Snapshots and Log Replay

Because replaying the entire Raft log from position zero on every restart is prohibitively expensive, Aeron Cluster supports **snapshots**: a point-in-time serialisation of the complete state machine state, written to Aeron Archive. On restart, the node loads the most recent snapshot and replays only the log entries committed after the snapshot position.

This has a direct consequence for application design: **every piece of state that affects the transition function must be included in the snapshot**. State that is reconstructable from the log (e.g. counters that are incremented once per message) does not need to be stored explicitly if the full replay is cheap enough; all other state must be explicitly serialised.

The snapshot is itself subject to the determinism constraint: the serialisation must produce identical bytes on every node that snapshots at the same log position. Non-deterministic serialisation (e.g. iterating a hash map with undefined ordering) produces divergent snapshots, which will cause nodes that restore from a follower snapshot to start from a different state than nodes that restore from the leader snapshot.

---

### 1.3.3 Unsafe Application Operations

The following categories of operations are **non-deterministic** and must never be used inside a `ClusteredService` callback (`onSessionMessage`, `onTimerEvent`, `onRoleChange`, `onTakeSnapshot`, `onNewLeadershipTermKey`). Each category explains what the violation looks like and why it causes divergence.

#### Wall-Clock Time

**Violation**: reading the system clock inside a callback.

```java
// Wrong
long now = System.currentTimeMillis();
long now = System.nanoTime();
Instant.now();
```

**Why it diverges**: clocks on separate physical machines are never perfectly synchronised, even with PTP/GPS disciplining. Two nodes calling `System.currentTimeMillis()` at the same logical position in the Raft log will read different values. Any state derived from that value — FIX `SendingTime`, heartbeat deadlines, timer registration — will differ between nodes.

**Correct alternative**: use `cluster.timeMs()`, which returns the Raft-committed timestamp embedded in each log entry. This value is identical on every node for the same log position.

```java
// Correct
long now = cluster.timeMs();
```

#### Timer Scheduling

**Violation**: scheduling a timer via any mechanism other than the cluster timer service.

```java
// Wrong
ScheduledExecutorService scheduler = Executors.newSingleThreadScheduledExecutor();
scheduler.schedule(() -> sendHeartbeat(), 30, TimeUnit.SECONDS);
```

**Why it diverges**: an OS-level timer fires based on wall-clock time, which differs between nodes. The leader fires its heartbeat at `t₀`; a follower fires at `t₀ + ε`. If the cluster fails over between those two moments, the new leader's state machine is at a different phase of the heartbeat cycle than the old leader was.

**Correct alternative**: use `cluster.scheduleTimer(correlationId, deadlineMs)`. The deadline is in cluster logical time. The timer event is delivered via `onTimerEvent` as a committed log entry, so it fires at exactly the same logical position on every node.

#### Unordered Collections

**Violation**: iterating a collection whose iteration order is not deterministic.

```java
// Wrong — HashMap iteration order is undefined and varies by JVM, load factor, and GC state
Map<Long, byte[]> storage = new HashMap<>();
for (var entry : storage.entrySet()) { ... }
```

**Why it diverges**: `HashMap` in Java does not guarantee insertion order. The iteration sequence can differ between JVM instances, between GC cycles, and between JVM versions. Any output whose content or order depends on iterating such a collection will differ between nodes.

**Correct alternative**: use `LinkedHashMap` (insertion order), `TreeMap` (sorted order), or any collection with a defined, reproducible iteration order.

#### Random Number Generation

**Violation**: generating random numbers without a shared, deterministic seed.

```java
// Wrong
long id = ThreadLocalRandom.current().nextLong();
UUID.randomUUID();
```

**Why it diverges**: each JVM instance seeds its random generator independently. The leader generates `id = 42`; the follower generates `id = 7`. Any state keyed on that value is immediately inconsistent.

**Correct alternative**: derive identifiers deterministically from values already in the log — for example, the `clusterSessionPosition` (a unique monotonic value per committed message) or a hash of the message content.

#### Blocking I/O

**Violation**: any blocking system call inside a callback.

```java
// Wrong
Thread.sleep(100);
socket.read(buffer);          // blocks until data arrives
Files.write(path, bytes);     // synchronous file write
riskSystem.check(order);      // synchronous RPC to external system
```

**Why it is dangerous**: a blocked `ClusteredService` thread stops processing log entries. The leader stops sending Raft heartbeats. Followers time out, trigger an election, and elect a new leader before the original leader's callback returns. When the original leader recovers from the block it believes it is still leader and may publish duplicate commands or corrupt state.

The correct pattern is to offload all blocking work to a separate thread (as this gateway does with the Risk Thread on Core 8) and communicate results back via Aeron IPC. The `ClusteredService` callback must return in microseconds.

#### Non-Deterministic Logging and Side Effects

**Violation**: writing to shared external state inside a callback.

```java
// Wrong
logger.info("Received order: " + clOrdId);   // synchronous file I/O
database.insert(order);                        // external write
```

**Why it diverges** (for external writes): every node executes the callback, so every node writes to the external system. The external system receives `N` writes for every one Raft-committed message (where `N` is the cluster size). Idempotency is not guaranteed.

**Why it is dangerous** (for synchronous logging): even fast local writes can block under OS buffer pressure, causing the same leader-eviction scenario described under Blocking I/O.

**Correct alternative**: buffer log records in a lock-free ring buffer and drain them on a dedicated logging thread. Perform all external writes on the leader only, gated on `cluster.role() == LEADER`, and treat them as best-effort side effects (not part of the replicated state).

#### Floating-Point Arithmetic

**Violation**: using floating-point operations whose results depend on FPU rounding mode or hardware-specific precision.

The IEEE 754 standard permits hardware implementations to use extended precision internally. Operations that appear equivalent in source may produce different bit-level results on CPUs with different FPU configurations.

**Correct alternative**: represent all monetary values, prices, and quantities as scaled integers (e.g. price in tenths of a basis point as `int64_t`). Perform all arithmetic in integer space. This is also required for FIX protocol conformance, where prices are represented as decimal strings with explicit precision.

#### Identity-Based Operations

**Violation**: using object identity (memory address, default `hashCode()`, reference equality) as part of application logic.

```java
// Wrong — default hashCode() is derived from object identity
if (orderA.hashCode() < orderB.hashCode()) { ... }
```

**Why it diverges**: object addresses differ between JVM instances. The same logical object has a different identity on each node.

**Correct alternative**: define `equals()` and `hashCode()` explicitly based on business-key fields (e.g. `clOrdId`, `sessionId`).

---

### 1.3.4 Consequences of Divergence

When a non-deterministic operation causes two nodes to reach different states, the cluster does not immediately detect the problem. Raft only ensures agreement on the log; it does not hash-compare state machines after each transition. Divergence therefore manifests indirectly and often much later:

- **Silent correctness failures**: the new leader after a failover has a different `inboundSeqNum` or `outboundSeqNum` than the old leader. FIX clients receive a sequence number gap or an unexpected reset.
- **Snapshot incompatibility**: a follower snapshot restores a node to a state that is inconsistent with the leader. The node rejoins the cluster with incorrect position state.
- **Timer drift**: heartbeat timers fire at different times on different nodes, causing unnecessary `TestRequest` messages or even session teardown on the leader change.

These failures are difficult to reproduce in testing because they require multiple nodes to diverge under specific timing conditions. The only reliable mitigation is strict discipline: treat every line of code inside a `ClusteredService` callback as subject to the determinism constraint, and enforce this in code review.