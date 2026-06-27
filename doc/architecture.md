# Chapter 2: Architecture

## 2.1 System Overview

The gateway is a pipeline of four cooperating subsystems, each with a distinct role and isolation boundary:

| Subsystem | Language | Role |
|-----------|----------|------|
| **FIX Gateway** | C++23 | Accepts TCP connections from buy-side clients. Drives socket I/O through `io_uring`. Runs the FIX session layer using simdfix for frame detection and field decoding. Handles both directions of the client TCP connection: inbound orders and outbound execution reports, heartbeats, and session messages. |
| **Aeron Cluster** | Java 21 | Receives raw FIX frames from all gateway nodes and sequences them through a Raft consensus log. Assigns a monotonically increasing, cluster-global sequence number to every message. Owns all FIX session-layer state: sequence numbers, heartbeat timers, resend buffers. |
| **Application Engine** | C++23 | Subscribes to the cluster's ordered output. Decodes application-layer FIX messages using simdfix, performs a fast in-process risk check, then hands orders off to a dedicated risk thread for external validation before routing approved orders to the sell-side gateway. |
| **Risk System** | External | A slow, synchronous external service (mainframe or proprietary risk platform). Contacted per-order via a blocking RPC call. Approval or rejection determines whether the order is forwarded to the sell-side gateway. |

All intra-host communication uses Aeron IPC (shared memory ring buffers). No message crosses a TCP socket between any two components on the same node.

---

## 2.2 Thread Topology

Each box represents a pinned OS thread on an isolated CPU core. Arrows labelled **SPSC** are single-producer / single-consumer lock-free queues in shared memory. Arrows labelled **aeron:ipc** are Aeron shared-memory publications. The dashed horizontal line marks the language boundary between C++ and Java.

```
  Buy-side clients (FIX 4.2 / 4.4, TCP)
          │  inbound FIX (orders, logon, cancel, heartbeats)
          ▼
          ▲  outbound FIX (acks, exec reports, heartbeats, resends)
          │
╔═══════════════════════════════════════════════════════════════════════════╗
║  INGRESS PROCESS  (C++23)                                                ║
║                                                                           ║
║  ┌────────────────────────┐  RX SPSC  ┌─────────────────────────────┐   ║
║  │  Core 1                │ ────────► │  Core 2                     │   ║
║  │  io_uring Reactor      │           │  FIX Session                │   ║
║  │                        │ ◄──────── │                             │   ║
║  │  · IORING_OP_RECV (rx) │  TX SPSC  │  · FrameScanner (simdfix)   │   ║
║  │  · IORING_OP_SEND (tx) │           │  · Session state machine    │   ║
║  │  · Registered buffers  │           │  · Admission / rate limit   │   ║
║  │  · CQE busy-spin       │           │  · Outbound FIX formatting  │   ║
║  └────────────────────────┘           └─────────────────────────────┘   ║
║                                             │                 ▲          ║
╚═════════════════════════════════════════════╪═════════════════╪══════════╝
                                              │                 │
                               aeron:ipc stream 1    aeron:ipc stream 6
                               (inbound FIX →)       (← outbound FIX cmds)
                                              │                 │
  ═ ═ ═ ═ ═ ═ ═ ═ ═ ═ ═ ═ ═ ═ C++ / Java boundary ═ ═ ═ ═ ═ ═ ═ ═ ═ ═ ═
                                              │                 │
                                              ▼                 │
╔═══════════════════════════════════════════════════════════════════════════╗
║  CLUSTER PROCESS  (Java 21)                                              ║
║                                                                           ║
║  ┌───────────────────────────────────────────────────────────────────┐   ║
║  │  Cores 3-6                                                        │   ║
║  │  Aeron ConsensusModule + FixClusteredService                      │   ║
║  │                                                                   │   ║
║  │  · Raft log replication (3-node quorum)                          │   ║
║  │  · Assigns cluster-global sequence number per committed message   │   ║
║  │  · FIX session state: inbound/outbound seq, heartbeat timers     │   ║
║  │  · Resend buffer (MemoryStorage, last 2500 messages)             │   ║
║  │  · Deterministic time: cluster.timeMs() only                     │   ║
║  │  · Snapshot to Aeron Archive on NVMe                             │   ║
║  └───────────────────────────────────────────────────────────────────┘   ║
║                │                                         ▲               ║
╚════════════════╪═════════════════════════════════════════╪══════════════╝
                 │                                         │
       aeron:ipc stream 2                       aeron:ipc stream 3
       ordered commands (FORWARD_APP,           sent-reports and
       CONNECT, DISCONNECT)                     reject commands
                 │                              (C++ → Java for
  ═ ═ ═ ═ ═ ═ ═ ╪ ═ ═ ═ ═ ═ ═ ═ C++ / Java boundary ═ outbound seq)
                 │                                         │
                 ▼                                         │
╔═══════════════════════════════════════════════════════════════════════════╗
║  APPLICATION PROCESS  (C++23)                                            ║
║                                                                           ║
║  ┌──────────────────────────────────────┐                                ║
║  │  Core 7                              │──► aeron:ipc stream 6          ║
║  │  AppWorker                           │    (outbound FIX to clients)   ║
║  │                                      │                                ║
║  │  · Aeron IPC subscriber (stream 2)   │                                ║
║  │  · Command dispatch                  │                                ║
║  │  · PayloadDecoder (simdfix NEON)     │                                ║
║  │  · Fast risk check (< 100 ns)        │                                ║
║  └──────────────────────────────────────┘                                ║
║                    │ SPSC (order + decoded fields)                        ║
║                    ▼                                                      ║
║  ┌──────────────────────────────────────┐                                ║
║  │  Core 8                              │──────────────────────────────► ║
║  │  Risk Thread                         │   Multiplexed RPC              ║
║  │                                      │   (max 25 in-flight)           ║
║  │  · Drains order SPSC (if slots free) │◄────────────────────────────── ║
║  │  · Max 25 outstanding requests       │   approved / rejected          ║
║  │  · Correlation ID table              │   (100–500 ms per response)    ║
║  └──────────────────────────────────────┘                                ║
║                    │ aeron:ipc stream 4            ▲ (exec reports,      ║
║                    │ (approved order commands)     │  stream 5)          ║
╚════════════════════╪══════════════════════════════╪════════════════════════╝
                     │                              │
                     ▼                              │
╔═══════════════════════════════════════════════════════════════════════════╗
║  EGRESS PROCESS  (C++23)                                                 ║
║                                                                           ║
║  ┌─────────────────────────────┐  SPSC  ┌─────────────────────────────┐ ║
║  │  Core 9                     │──────► │  Core 10                    │ ║
║  │  EgressWorker               │        │  io_uring Reactor           │ ║
║  │                             │        │                             │ ║
║  │  · Order routing            │        │  · SQE submission           │ ║
║  │  · FIX encode (simdfix)     │        │  · CQE drain (exec reports) │ ║
║  │  · Egress risk accounting   │        │  · Registered send buffers  │ ║
║  └─────────────────────────────┘        └─────────────────────────────┘ ║
╚═══════════════════════════════════════════════════════════════════════════╝
                     │
                     │  TCP (FIX)
                     ▼
           Sell-side Gateway


                              ┌─────────────────────────────────────┐
                              │  Big Iron Risk System               │
                              │  (mainframe / proprietary platform) │
                              │                                     │
                              │  · Synchronous RPC (100–500 ms)    │
                              │  · Per-order approval              │
                              │  · Credit limit, position check    │
                              └─────────────────────────────────────┘
```

**Aeron Media Driver** (`aeronmd`) runs as a separate process pinned to Cores 11–14, managing all IPC ring buffers and acting as the shared-memory intermediary between all four processes.

---

## 2.3 Process and Core Assignment

| Core | Thread | Process | Language |
|------|--------|---------|----------|
| 1 | io_uring Ingress Reactor | Ingress | C++ |
| 2 | FIX Session | Ingress | C++ |
| 3–6 | Aeron Cluster (Conductor, Raft, Archive) | Cluster | Java |
| 7 | AppWorker | Application | C++ |
| 8 | Risk Thread | Application | C++ |
| 9 | EgressWorker | Egress | C++ |
| 10 | io_uring Egress Reactor | Egress | C++ |
| 11–14 | Aeron Media Driver | aeronmd | C |
| 15+ | OS / system services | — | — |

Cores 1–14 are isolated from the Linux scheduler via `isolcpus=1-14 nohz_full=1-14 rcu_nocbs=1-14` kernel boot parameters. Each thread calls `pthread_setaffinity_np` to pin itself at startup.

---

## 2.4 Ingress Path

### 2.4.1 io_uring Reactor (Core 1)

The ingress reactor owns all TCP file descriptors for accepted client connections. It handles both directions of each connection simultaneously:

- **Receive path**: submits `IORING_OP_RECV` operations to the kernel submission queue (SQ) using `IORING_SETUP_SQPOLL`, eliminating the `io_uring_enter()` syscall. Receive buffers are pre-registered with the kernel via `io_uring_register_buffers`, so data arrives directly into process-owned memory without a kernel copy. Completed receives are written into the RX SPSC toward the FIX Session thread.

- **Send path**: drains the TX SPSC from the FIX Session thread and submits `IORING_OP_SEND` operations using the same pre-registered buffer pool. CQEs for completed sends are discarded unless they indicate an error (TCP disconnect).

The reactor thread never allocates heap memory and never blocks on either path.

### 2.4.2 SPSC Queues: Reactor ↔ FIX Session

Two SPSC queues decouple the reactor from session processing. Head and tail counters on each queue are on separate cache lines (`alignas(64)`) to prevent false sharing.

- **RX SPSC** (reactor → session): carries raw byte spans and source file descriptors for inbound TCP data. The reactor is the sole producer; the session thread is the sole consumer.
- **TX SPSC** (session → reactor): carries encoded outbound FIX byte spans and destination file descriptors. The session thread is the sole producer; the reactor is the sole consumer.

Capacity on both queues is sized to absorb a full burst across all connected sessions without stalling the reactor.

### 2.4.3 FIX Session Thread (Core 2): Inbound Path

On the receive side, the session thread runs simdfix's `FrameScanner` on each byte span: a NEON SIMD scan for SOH (`0x01`) delimiters that locates complete FIX frames without field-level parsing. Parsing at this stage is intentionally minimal — only enough to verify that `8=FIX` appears at the start and `10=` appears at the end.

Session-layer admission decisions (rate limiting, maximum frame size) are made here. All other session state — sequence numbers, heartbeat timers, resend logic — is owned by the Java `FixClusteredService` and is authoritative only after Raft commits the message.

Complete frames are offered to the Aeron IPC publication (stream 1) as `{ uint64 sessionId | uint16 length | uint8[] rawFix }`. The offer call is non-blocking; if the ring is full, back-pressure propagates to the SPSC consumer loop, which naturally throttles the reactor's receive submissions.

### 2.4.4 FIX Session Thread (Core 2): Outbound Path

The session thread also subscribes to Aeron IPC stream 6, which carries outbound FIX messages routed back from the Application Engine. These include:

- Session-layer messages generated by the cluster: Logon acknowledgement, Heartbeat, TestRequest, Logout, Reject, SequenceReset, ResendRequest responses.
- Application-layer messages relayed from the exchange: ExecutionReport, OrderCancelReject.

On each stream 6 message the session thread writes the pre-encoded FIX bytes into the TX SPSC toward the reactor, tagging them with the destination `sessionId`. The reactor submits the send to the client's file descriptor via `IORING_OP_SEND`.

No FIX encoding happens in the session thread on the outbound path. All FIX encoding — for both session-layer and application-layer outbound messages — is performed by the AppWorker in the Application Process before the bytes reach stream 6. The session thread is a transparent conduit on the send side: it reads pre-encoded FIX bytes from the stream 6 subscription and forwards them to the TX SPSC without inspection.

---

## 2.5 Aeron Cluster: Global Sequencing

### 2.5.1 Role of the Cluster

The Java Aeron Cluster is the **single source of truth** for the order of every FIX message received by the gateway. No application-level processing occurs before the cluster commits a message. This has two consequences:

1. Every node in the cluster applies messages in identical order. The C++ AppWorkers on all nodes receive the same command stream, ensuring consistent risk state and consistent position tracking across the cluster.
2. After a failover, the new leader resumes processing from the exact position where the old leader left off — no messages are lost or processed twice at the cluster boundary.

### 2.5.2 Sequence Number Assignment

`ConsensusModule` appends each inbound FIX frame to the Raft log and assigns it a `clusterSessionPosition` — a monotonically increasing byte offset in the log that uniquely identifies the message across all time and all nodes. `FixClusteredService.onSessionMessage()` fires on every node in commit order, carrying this position in the `Header` parameter.

The Java service maintains its own `inboundSeqNum` counter (the FIX application-layer sequence number) and increments it on each committed message. This counter is part of the cluster snapshot and survives failover exactly.

### 2.5.3 FIX Session State in Java

The `FixClusteredService` holds:

- `inboundSeqNum` / `outboundSeqNum` — FIX sequence counters, replicated across all nodes via Raft.
- `SessionPhase` — current state in the FIX session state machine (Disconnected / LogonPending / Active / LogoutPending).
- `MemoryStorage` — a circular buffer of the last 2500 outbound messages, used to service `ResendRequest` without touching the archive.
- Heartbeat and test-request timers, scheduled via `cluster.scheduleTimer()` so they fire at the same cluster-global time on every node.

All wall-clock access is prohibited inside `ClusteredService` callbacks. Time is sourced exclusively from `cluster.timeMs()`, which returns the Raft-committed logical timestamp — identical on every node.

### 2.5.4 Output Streams to C++

After processing each committed message, the Java service emits a command to the C++ AppWorker via Aeron IPC (stream 2):

| Command | Trigger | Payload |
|---------|---------|---------|
| `FORWARD_APP` | Inbound application message (D, F, G, …) | raw FIX bytes |
| `SEND` | Outbound message ready for TCP | SBE-encoded message + outbound seq |
| `RESEND` | ResendRequest being serviced | SBE-encoded message with PossDupFlag |
| `GAP_FILL` | Session messages skipped in resend | begin/end seq range |
| `CONNECT` | Node elected leader | SenderCompID / TargetCompID |
| `DISCONNECT` | Node lost leadership | — |

`FORWARD_APP` carries inbound application-layer content for the C++ risk and routing engine. `SEND` and `RESEND` carry SBE-encoded outbound messages — the Java cluster never produces FIX text; all FIX encoding is done by the C++ AppWorker before the bytes reach the client TCP socket.

---

## 2.6 Application Engine

### 2.6.1 AppWorker (Core 7)

The AppWorker busy-spins on stream 2, polling up to 10 commands per duty cycle. On a `FORWARD_APP` command it:

1. Passes the raw FIX bytes to `PayloadDecoder`, which uses simdfix's NEON-accelerated tokenizer to locate all field offsets without copying the data.
2. Dispatches to `FixMessageHandler<AppHandler>` via CRTP — the handler fires `onNewOrderSingle`, `onOrderCancelRequest`, etc. with fully decoded accessor objects.
3. Runs a fast in-process risk check (position and rate-limit accounting using atomic loads, < 100 ns).
4. If the in-process check passes, enqueues the decoded order into the SPSC toward the Risk Thread.

On `SEND` and `RESEND` commands the AppWorker decodes the SBE payload to extract the message fields, encodes a FIX text frame using simdfix's `PayloadEncoder`, and publishes the result on stream 6 toward the Ingress FIX Session thread. This covers both session-layer messages (Logon, Heartbeat, Reject, Logout) and application-layer messages (ExecutionReport) that have been routed back through the cluster from the sell-side gateway. The risk path is not touched.

The AppWorker is intentionally **stateless between leader elections**. It holds no persistent position state of its own. On a `DISCONNECT` command it resets its transient decode state; on a `CONNECT` command it resumes. Durable state lives exclusively in the Java cluster.

### 2.6.2 Risk Thread (Core 8) and the Slow External Call

tThe Risk Thread is the architectural accommodation for an external risk system that cannot be made asynchronous. It dedicates a full isolated core to managing up to **25 concurrent in-flight requests** to the risk platform using a multiplexed, correlation-ID-based protocol over a single persistent TCP connection.

```
AppWorker               Risk Thread                         Big Iron
    │                        │                                  │
    │── SPSC: order ────────►│                                  │
    │                        │── request (correlId=1) ─────────►│
    │── SPSC: order ────────►│── request (correlId=2) ─────────►│  (up to 25
    │── SPSC: order ────────►│── request (correlId=3) ─────────►│   in-flight)
    │                        │                                  │
    │                        │◄─ response (correlId=2, pass) ───│
    │                        │── OrderCommand → stream 4         │
    │                        │◄─ response (correlId=1, reject) ─│
    │                        │── reject cmd → stream 3           │
```

The Risk Thread runs a single-threaded event loop on Core 8. A critical constraint shapes its design: **orders from the same client must be checked in sequence**. The risk system evaluates each order against the client's running position; if two orders for the same client are in-flight simultaneously, both see the same pre-trade position and both may be approved even if together they breach the limit. The Risk Thread enforces per-client serialisation using two data structures:

- `inFlightByClient`: maps `sessionId → correlationId` for clients with a request currently outstanding.
- `pendingByClient`: a per-client FIFO queue for orders that arrived while that client already had a request in-flight.

The event loop has three phases:

1. **Send phase**: drain the inbound SPSC while the global in-flight count is below 25. For each order:
   - If `inFlightByClient` already contains the order's `sessionId`: push the order to `pendingByClient[sessionId]` and continue draining.
   - Otherwise: send the request to the risk system, assign a `correlationId`, record `inFlightByClient[sessionId] = correlationId`, and store the order in `correlationTable[correlationId % 25]`.
2. **Receive phase**: poll the risk system TCP connection for responses. Each response carries the `correlationId` of the answered request. Look up the pending order in `correlationTable`, remove the entry, erase `sessionId` from `inFlightByClient` (decrementing global count), and publish the result. Then check `pendingByClient[sessionId]`: if non-empty, immediately dispatch the next queued order for that client without waiting for the SPSC.
3. **Back-pressure**: when all 25 global slots are occupied, the send phase is skipped entirely. The inbound SPSC stops being drained.

Across different clients, responses may arrive out of order. This is acceptable: the EgressWorker and the sell-side gateway identify each order independently by `clOrdId`.

The SPSC between AppWorker and Risk Thread carries decoded order fields (clOrdId, side, quantity, price, symbol) rather than raw FIX bytes, so the Risk Thread needs no FIX awareness. On an approved result it encodes an `OrderCommand` and writes it to stream 4. On a rejection it writes a reject command back to the Java service (stream 3) for encoding as a FIX `ExecutionReport` with `OrdStatus=Rejected`.

---

## 2.7 Egress Path

### 2.7.1 EgressWorker (Core 9)

The EgressWorker subscribes to stream 4 (approved order commands from the Risk Thread) and polls execution reports arriving from the sell-side gateway TCP session. For each order command it:

1. Performs a final egress-side risk check (net position and notional value accounting, updated by fill events).
2. Routes to the appropriate `SellSideSession` via a hash lookup on instrument or client.
3. Encodes a FIX `NewOrderSingle` (or cancel/replace) using simdfix's `PayloadEncoder`.
4. Writes the encoded bytes to the SPSC toward the io_uring Egress Reactor.

Execution reports from the sell-side gateway travel in the opposite direction: the reactor drains the receive CQEs, the EgressWorker decodes the report with simdfix, updates position, and publishes the encoded client-side FIX `ExecutionReport` on stream 5 (back to the Application Engine, which routes it to the correct client session via stream 6 → Ingress → client TCP).

### 2.7.2 SPSC Queue: EgressWorker → io_uring Reactor (Core 10)

The egress reactor mirrors the ingress reactor: it submits `IORING_OP_SEND` operations from pre-registered buffers, batch-drains completion events, and refills the SPSC consumer position. It is the only thread that calls into the kernel for network I/O on the exchange-facing path.

---

## 2.8 Language Boundaries

There are two language boundaries in the system.

### 2.8.1 C++ → Java (Ingress, Stream 1)

The FIX Session thread publishes raw FIX text bytes into an Aeron IPC ring buffer. No serialization format is applied — the bytes are the FIX wire format verbatim, prefixed with a `uint64 sessionId` and a `uint16 length`. Java reads these using an Aeron `DirectBuffer` (off-heap, no GC pressure) and a targeted byte scanner (`FixHeaderScanner`) that extracts only `MsgType` (tag 35) and `MsgSeqNum` (tag 34) from the raw bytes without a full parse.

### 2.8.2 Java → C++ (Application, Streams 2 and 3)

Commands from Java to the C++ AppWorker use a compact binary envelope:

```
Byte 0:      discriminator (CMD_FORWARD_APP, CMD_SEND, CMD_RESEND, …)
Bytes 1–N:   payload (command-specific, fixed layout per discriminator)
```

For `FORWARD_APP` the payload is raw inbound FIX bytes. For `SEND` and `RESEND` the payload is an **SBE-encoded message** — the cluster populates the relevant fields (message type, sequence number, instrument, quantities, timestamps) into a typed SBE flyweight and publishes the binary result. The C++ AppWorker decodes this SBE payload and encodes the outbound FIX text frame using simdfix's `PayloadEncoder`. Java never produces FIX text.

Stream 3 carries responses in the same envelope format in the opposite direction (C++ → Java).

### 2.8.3 What Crosses the Boundary

The language boundary is deliberately asymmetric in format:

- **Inbound direction (C++ → Java)**: raw FIX text bytes. Java extracts only `MsgType` and `MsgSeqNum` with a targeted scanner; it never fully parses FIX.
- **Outbound direction (Java → C++)**: SBE for messages to be sent to clients; raw FIX bytes for `FORWARD_APP` messages to be decoded by C++. Java populates typed SBE fields; C++ owns all FIX text production.

The SBE schema therefore governs two distinct uses: outbound message encoding on stream 2, and cluster snapshot serialization. C++ never participates in Raft consensus or FIX session state management. Java never produces FIX wire format.

---

## 2.9 Risk System Integration

### 2.9.1 Characteristics of the External Risk System

The external risk system (referred to here as *big iron*) is a synchronous, request-response service. Representative characteristics:

| Property | Value |
|----------|-------|
| Protocol | Proprietary TCP, synchronous (request blocks until response) |
| Typical latency | 100–300 ms (p50), up to 500 ms (p99) |
| Response type | Binary approval/rejection with reason code |
| Concurrency model | One connection, up to 25 outstanding requests (multiplexed by correlation ID) |
| Failure mode | TCP disconnect → reconnect, pending order treated as rejected |

These characteristics are irreconcilable with a sub-microsecond hot path. The dedicated Risk Thread is the isolation layer.

### 2.9.2 Ordering and Idempotency

Orders from the **same client** are checked in strict arrival sequence: the Risk Thread holds subsequent orders for a client in `pendingByClient` until the outstanding request for that client returns. This guarantees that the risk system evaluates each order against the position already committed by all preceding orders from that client.

Orders from **different clients** may be in-flight simultaneously (up to 25 globally) and their responses may arrive out of order. The Risk Thread makes no cross-client ordering guarantee to downstream consumers: `OrderCommand` entries on stream 4 arrive in response-receipt order. The EgressWorker and sell-side gateway identify each order independently by `clOrdId` and tolerate cross-client reordering.

The `clusterSessionPosition` from the Aeron log header (carried through the command envelope) serves as an idempotency key. If the TCP connection to the risk system drops mid-flight, any in-flight requests whose responses have not been received are treated as rejected, and the connection is re-established. The Risk Thread does not retry automatically; the client receives an `ExecutionReport` with `OrdStatus=Rejected` and `OrdRejReason=Other` for each dropped in-flight order.

### 2.9.3 Back-Pressure Propagation

When the risk system is slow, the following chain of back-pressure propagates automatically without any explicit signalling:

```
Risk system slow (all 25 in-flight slots occupied)
    → Risk Thread stops draining inbound SPSC
    → SPSC producer (AppWorker) sees SPSC full
    → AppWorker stops forwarding orders to SPSC
    → Orders queue in Aeron stream 2 log buffer
    → AppWorker subscriber position stops advancing
    → Java cluster publication sees flow-control window exhausted
    → Java offer() returns BACK_PRESSURED
    → Java service defers command (does not block onSessionMessage callback)
    → Cluster ingress to Java slows
    → C++ ingress SPSC fills
    → FIX Session thread stops offering to Aeron stream 1
    → Ingress reactor applies TCP back-pressure to client socket
```

No thread is blocked. Core 8 spins on its event loop even when all 25 slots are occupied — it simply skips the send phase and continues polling the risk system receive path for responses. All other threads continue spinning on their respective duty cycles and service non-order traffic (heartbeats, execution reports, cancel requests) without interruption.

---

## 2.10 Aeron IPC Stream Map

```
Stream 1  C++ Ingress → Java Cluster       raw inbound FIX bytes (per frame)
Stream 2  Java Cluster → C++ AppWorker     ordered commands
                                             · FORWARD_APP: raw inbound FIX bytes
                                             · SEND/RESEND:  SBE-encoded outbound message
                                             · GAP_FILL, CONNECT, DISCONNECT: control
Stream 3  C++ AppWorker → Java Cluster     sent-reports, reject commands
Stream 4  C++ Risk Thread → C++ Egress     approved OrderCommands
Stream 5  C++ Egress → Java Cluster        execution reports (for client routing)
Stream 6  C++ AppWorker → C++ Ingress      FIX-encoded outbound frames for client TCP
```

All streams share a single Aeron Media Driver instance on the host. The media driver is launched before any gateway process and exits last. Its `aeron.dir` (`/dev/shm/aeron`) is mapped into every process's address space at startup.

---

## 2.11 Latency Budget and Throughput

### 2.11.1 End-to-End Latency

The table below covers the **leader hot path** for a `NewOrderSingle` that passes both risk checks. Raft replication latency (2–5 µs) adds to the cluster stage on committed messages.

| Stage | Thread | Typical |
|-------|--------|---------|
| TCP receive → SPSC write | Core 1 (io_uring) | 0.5 µs |
| SPSC read → Aeron offer | Core 2 (FIX Session) | 0.5 µs |
| Aeron IPC → Java dispatch | Core 3 (Cluster) | 0.5 µs |
| Raft commit (3-node, co-lo) | Core 3 | 2–5 µs |
| Java → stream 2 → AppWorker | Core 7 | 0.5 µs |
w| SIMD decode NOS (tokenize + getters) | Core 7 | **~130 ns** |
| Fast risk check (atomic loads) | Core 7 | ~30 ns |
| Risk Thread RPC (in-flight slot wait + response) | Core 8 | **100–500 ms** |
| SPSC → EgressWorker | Core 9 | ~150 ns |
| FIX encode NOS (sell-side order) | Core 9 | **~37 ns** |
| io_uring send → NIC | Core 10 | 0.5 µs |

Decode and encode figures are measured values from `SimdFixBenchmark` (release build, ARM NEON, Apple M1 Pro): NOS GETTERS 128.5 ns, NOS ENCODE 37.3 ns. The risk RPC dominates the end-to-end budget by six orders of magnitude. All pipeline stages outside the risk call total under 10 µs; the individual order latency is determined almost entirely by the risk system RTT plus any queuing delay waiting for a free in-flight slot.

---

### 2.11.2 Throughput Analysis

The system throughput ceiling is set by the risk system, which is the sole bottleneck. All other pipeline stages have headroom orders of magnitude above the risk-constrained limit.

#### Risk System: Little's Law

By Little's Law, in a stable system the average throughput λ, average number of in-flight requests N, and average service time W are related by:

```
N = λ × W   →   λ = N / W   (orders/sec)
```

The table below gives the throughput ceiling for combinations of outstanding-request limit and risk system RTT. The current system is configured for 25 outstanding requests; the table shows neighbouring values to illustrate sensitivity.

| Outstanding \ RTT | 1 ms | 50 ms | 100 ms | 250 ms | 500 ms |
|-------------------|-----:|------:|-------:|-------:|-------:|
| **10** | 10 000 | 200 | 100 | 40 | 20 |
| **50** | 50 000 | 1 000 | 500 | 200 | 100 |
| **100 ★** | **100 000** | **2 000** | **1 000** | **400** | **200** |

All figures are orders/sec per gateway instance. The ★ row represents the theoretical optimum for this architecture: 100 outstanding requests against a risk system with 1 ms RTT. At that operating point the Raft commit ceiling (≈ 50 000 ops/sec) becomes the binding constraint before the risk system does — see the headroom table below.

The outstanding-request limit and the risk system RTT are the only two levers available to change this ceiling without modifying the rest of the pipeline. Halving the RTT doubles throughput; doubling the outstanding limit doubles throughput. Multiple independent gateway instances scale linearly, subject to the risk system's own concurrency capacity.

#### Queuing: Slot Wait Time

When the risk system RTT is variable, orders arriving faster than the risk system can clear them accumulate in the inbound SPSC. The time an order waits for a free slot is:

```
slot_wait ≈ (in_flight / N) × avg_RTT
```

At steady-state throughput equal to the ceiling (all N slots continuously occupied), the average slot wait approaches zero — each slot is freed just as a new order needs it. When the order arrival rate exceeds the ceiling, the SPSC fills and back-pressure propagates to the client TCP socket (§ 2.9.3).

#### Other Pipeline Stages: Headroom

The following figures show headroom at two reference points: the current production configuration (25 outstanding, 100 ms RTT = 250 orders/sec) and the theoretical optimum row (100 outstanding ★, 1 ms RTT = 100 000 orders/sec).

| Stage | Capacity | Headroom @ 250/sec | Headroom @ 100 000/sec |
|-------|----------:|-------------------:|-----------------------:|
| io_uring TCP receive | > 500 000 frames/sec | > 2 000× | ~5× |
| Aeron IPC (aeron:ipc) | > 5 000 000 msgs/sec | > 20 000× | ~50× |
| Raft commit (3-node co-lo) | ~ 50 000 ops/sec | ~ 200× | **< 1× ⚠** |
| simdfix SIMD decode | > 1 000 000 msgs/sec | > 4 000× | ~10× |
| io_uring TCP send (egress) | > 500 000 frames/sec | > 2 000× | ~5× |

At the optimum row the Raft commit ceiling (≈ 50 000 ops/sec) becomes the bottleneck before the risk system does. Reaching 100 000 orders/sec would require either a higher-throughput consensus layer or sharding clients across multiple independent cluster instances. At all other entries in the table Raft provides comfortable headroom.

#### Summary

Throughput scales linearly with both the outstanding-request limit and the inverse of the risk RTT. At the current configuration of 25 outstanding requests, the ceiling ranges from 50 orders/sec (500 ms RTT) to 500 orders/sec (50 ms RTT). The theoretical optimum of 100 outstanding requests at 1 ms RTT yields 100 000 orders/sec, at which point the Raft commit ceiling becomes the binding constraint rather than the risk system. Individual order latency is not improved by raising the concurrency limit — each order still waits one full RTT for a risk response — but the pipeline never idles: all N slots progress through the risk system in parallel at all times.

---

#### 2.11.3 Per-Client Throughput

Because orders from the same client are checked in sequence, each client is limited to one outstanding risk request at a time regardless of the global slot count. Per-client throughput is therefore:

```
per-client throughput = 1 / RTT
```

| RTT | Per-client throughput |
|-----|-----------------------:|
| 1 ms | 1 000 orders/sec |
| 50 ms | 20 orders/sec |
| 100 ms | 10 orders/sec |
| 500 ms | 2 orders/sec |

#### Aggregate Throughput as a Function of Client Count

Let C be the number of concurrently active clients (each submitting orders at the maximum per-client rate) and N the global outstanding-slot limit. The aggregate throughput is:

```
aggregate throughput = min(C, N) / RTT
```

When C < N each client occupies its own slot and the global limit is not reached; the system is **client-count limited**. When C ≥ N the global limit fills and additional clients yield no further aggregate gain; the system is **slot-limited**.

| Active clients (C) | Outstanding limit (N) | RTT = 100 ms | Limiting factor |
|--------------------|----------------------:|-------------:|-----------------|
| 5 | 25 | 50 orders/sec | Client count |
| 10 | 25 | 100 orders/sec | Client count |
| 25 | 25 | 250 orders/sec | Slot limit |
| 100 | 25 | 250 orders/sec | Slot limit |
| 100 | 100 ★ | 1 000 orders/sec | Slot limit |

The figures in the aggregate throughput table in § 2.11.2 assume C ≥ N — enough concurrently active clients to keep all slots occupied. When the client population is small (C < N), the aggregate throughput is C × (1/RTT) and the outstanding-slot limit has no effect.
