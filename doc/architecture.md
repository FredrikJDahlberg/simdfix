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

### 2.1.1 Deployment Requirements

The gateway is designed for bare-metal Linux with dedicated CPU cores, but it is a supported requirement to run in virtualized and containerized environments (cloud VMs, Docker, Kubernetes), subject to the constraints below.

| Requirement | Bare-metal | Cloud VM / container |
|-------------|-----------|----------------------|
| Linux kernel ≥ 6.1 with `io_uring` | required | required |
| `CAP_SYS_NICE`, `CAP_IPC_LOCK`, `CAP_NET_ADMIN` | implicit | must be granted explicitly |
| `/dev/shm` ≥ 1 GB | host default | must be configured (`--shm-size`) |
| Huge pages pre-allocated on host | `vm.nr_hugepages` in sysctl | host-level only; cannot be done from within a container |
| CPU isolation (`isolcpus`, `nohz_full`) | kernel boot parameter | not available on vCPU; use cgroup `cpuset` instead |
| PTP or invariant TSC clock source | hardware-provided | provider-dependent; verify before deployment |
| No vCPU steal on busy-spin cores | guaranteed | requires bare-metal or dedicated-host instance |

Cloud and containerized deployments must satisfy all rows marked "required" and address the remaining rows through configuration. See [virtual.md](virtual.md) for step-by-step instructions, and § 2.12 for the issues each requirement mitigates.

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

End-to-end latency estimates and throughput analysis are in [estimated-performance.md](estimated-performance.md). The risk system RPC (100–500 ms) dominates the latency budget by six orders of magnitude; all other pipeline stages total under 10 µs. System throughput is bounded by Little's Law applied to the risk slot limit: at the current configuration of 25 outstanding requests the ceiling ranges from 50 orders/sec (500 ms RTT) to 500 orders/sec (50 ms RTT).

---

## 2.12 Cloud and Virtualized Environments

The gateway is designed for bare-metal deployment with dedicated, isolated CPU cores, direct hardware access, and predictable memory subsystem behaviour. Running in a cloud or containerized environment degrades several of these assumptions. This section catalogs the specific issues and their impact.

---

### 2.12.1 vCPU Preemption and CPU Steal

**Issue**: Cloud VMs run on vCPUs — logical threads multiplexed across physical cores by the hypervisor. The hypervisor may preempt a vCPU at any moment to service another tenant, introducing *CPU steal time*: periods during which the guest OS believes it is running but the physical core is allocated elsewhere. On commodity cloud instances, steal commonly reaches 5–20% under contention.

**Impact on this system**:
- Busy-spin threads (Cores 1, 7, 8, 9, 10) stall invisibly during steal, inflating every stage of the latency budget by an unpredictable amount.
- The Risk Thread's 25-slot event loop continues spinning but makes no progress during steal. In-flight risk requests accumulate latency; the effective RTT seen by the gateway increases.
- Aeron Cluster heartbeats may be delayed, triggering spurious Raft elections if steal exceeds the election timeout.
- `isolcpus`, `nohz_full`, and `rcu_nocbs` kernel boot parameters, which eliminate OS scheduler interference, are ineffective on vCPUs because the hypervisor scheduler operates below the guest kernel.

**Mitigation**: Use bare-metal cloud instances (AWS `metal`, GCP `n2-metal`, Azure `Lsv3`) or VM types with pinned, dedicated physical cores and NUMA-aware placement. Disable SMT (hyperthreading) at the hypervisor level for the cores used by busy-spin threads.

---

### 2.12.2 io_uring Restrictions

**Issue**: `io_uring` with `IORING_SETUP_SQPOLL` spawns a kernel thread that busy-polls the submission queue, requiring `CAP_SYS_NICE` and kernel 5.10 or later. Several environments restrict or disable io_uring entirely:

- **Docker / containerd**: default seccomp profiles block many io_uring opcodes. `IORING_OP_RECV`, `IORING_OP_SEND`, and `IORING_SETUP_SQPOLL` are restricted or absent in the default allowlist.
- **Kubernetes**: pods run with restricted seccomp by default since Kubernetes 1.27. Privileged containers or a custom seccomp profile are required.
- **AWS Lambda / GCP Cloud Run**: io_uring is unavailable in serverless runtimes.
- **Managed kernel versions**: some cloud providers run kernels older than 5.10 (the minimum for stable `IORING_SETUP_SQPOLL` behaviour), particularly on long-term-support distributions.

**Impact**: If io_uring is unavailable or restricted, the ingress and egress reactors must fall back to `epoll` + `recvmsg`/`sendmsg`, adding one syscall per receive and eliminating zero-copy registered buffers. This adds 1–3 µs per message on the TCP I/O path.

**Mitigation**: Require kernel ≥ 5.10, grant `CAP_SYS_NICE` and `CAP_NET_ADMIN`, and supply a custom seccomp profile that permits all required io_uring opcodes. Do not run this workload in a serverless or restricted container environment.

**Fallback when io_uring is not available**: Two replacements are practical, in order of preference:

1. **`epoll` + `SO_BUSY_POLL` / `SO_PREFER_BUSY_POLL`** — the socket busy-polls the NIC ring buffer before sleeping, recovering most of the latency benefit of SQPOLL without requiring `CAP_SYS_NICE` or a custom seccomp profile. Available on Linux ≥ 3.11; works in any standard container environment. Performance within 1–2 µs of io_uring SQPOLL for single-connection workloads.

2. **`AF_XDP` (XDP sockets)** — true kernel-bypass receive on environments with an XDP-capable vNIC driver (AWS ENA ≥ 5.10, GCP gVNIC, Azure DPDK-backed NIC). Requires `CAP_BPF` and `CAP_NET_ADMIN`. Eliminates the socket stack entirely on the receive path; comparable to DPDK in latency. Higher operational complexity: BPF programs must be loaded and pinned at startup.

See [virtual.md](virtual.md) for implementation details.

---

### 2.12.3 Shared Memory and Huge Pages

**Issue**: Aeron IPC relies on large memory-mapped files under `/dev/shm`. Two constraints apply in containerized environments:

- **`/dev/shm` size limit**: Docker sets `/dev/shm` to 64 MB by default. Aeron's default term buffer size is 16 MB per stream; with six IPC streams and three term buffers each, the media driver requires roughly 6 × 3 × 16 MB = 288 MB of shared memory before accounting for the log meta-data and archive regions. This exceeds the Docker default by 4×.
- **Huge pages**: `vm.nr_hugepages` is a host-level kernel parameter. It is not namespaced; a container cannot configure huge pages independently of the host. On VMs where the host does not pre-allocate huge pages, Aeron falls back to 4 KB pages, increasing TLB pressure and adding ~5–10% latency on the Aeron IPC path.

**Mitigation**: Set `--shm-size=4g` on Docker containers. Pre-allocate huge pages at VM boot time (`vm.nr_hugepages=512` for 1 GB of 2 MB pages). On Kubernetes, request `hugepages-2Mi` in the pod resource spec and use a `hugetlb` volume.

---

### 2.12.4 Network Latency and Overlay Networking

**Issue**: Cloud networking introduces two sources of latency not present in bare-metal co-location:

- **vNIC overhead**: the virtual NIC adds a software path between the guest TCP stack and the physical NIC, typically adding 20–100 µs compared to kernel-bypass on bare metal.
- **Overlay encapsulation**: VPC networks in most cloud providers use VXLAN or Geneve encapsulation for tenant isolation, adding a per-packet encapsulation/decapsulation step in the host kernel. This adds 5–20 µs on the inter-VM path.

**Impact on the Raft cluster**: each Raft log replication round-trip between cluster nodes traverses the overlay network. At 50–100 µs per round-trip (vs. 2–5 µs on bare-metal co-lo), the commit latency stage in the end-to-end budget grows from ~5 µs to ~100 µs — moving it from negligible to the second-largest contributor after the risk RPC.

**Impact on the sell-side connection**: the egress TCP connection to the sell-side gateway, if it crosses a VPC boundary, also incurs this overhead. If the sell-side gateway is co-located with the gateway on bare metal or in the same physical rack, the VPC path is avoided.

**Mitigation**: Place all three cluster nodes in the same cloud availability zone and, where supported, in a placement group or proximity placement group to minimise physical distance. Use SR-IOV or DPDK-backed vNICs (AWS ENA, GCP gVNIC, Azure DPDK) to reduce vNIC overhead. Consider a dedicated interconnect (AWS Direct Connect, GCP Interconnect) if the sell-side gateway is in a separate environment.

---

### 2.12.5 JVM Stability Under Hypervisor Scheduling

**Issue**: The Java Aeron Cluster process relies on ZGC maintaining GC pause budgets under 1 ms. ZGC's concurrent marking and relocation threads compete with the guest OS for CPU time. Under vCPU steal:

- ZGC's concurrent phase may be paused mid-cycle. When the vCPU resumes, the GC phase continues, but the elapsed wall-clock time counts against the pause budget — the application thread may stall longer than ZGC's own measurements report.
- The election timeout safety margin (§ 3.9) assumes GC pauses stay under 1 ms. On a VM with frequent steal, observed GC pauses (from the application's perspective) can reach 5–20 ms, triggering spurious Raft elections.
- `-XX:+AlwaysPreTouch` touches all heap pages at JVM startup to avoid page-fault latency at runtime. On a VM with memory overcommit, the host may page out these memory regions under pressure, reintroducing page faults.

**Mitigation**: Allocate the JVM on a VM with dedicated physical cores and memory that is not overcommitted. Increase the Raft election timeout to 3–5× the observed worst-case GC pause on the target infrastructure. Disable memory overcommit at the hypervisor level for VM memory regions mapped by the JVM heap.

---

### 2.12.6 Clock Sources and Time Synchronisation

**Issue**: Aeron Cluster requires that `cluster.timeMs()` advances monotonically and is reasonably synchronised across nodes so that timer deadlines (heartbeat, test-request) fire at predictable times. In virtualized environments, two clock problems arise:

- **TSC instability**: the x86 Time Stamp Counter (`rdtsc`) is used by `clock_gettime(CLOCK_MONOTONIC)` via the VDSO. On VMs where vCPUs are migrated between physical cores, the TSC may jump or skew. Most modern hypervisors expose an invariant TSC (`constant_tsc`, `nonstop_tsc` in `/proc/cpuinfo`), but this must be verified.
- **NTP jitter**: inter-node clock synchronisation via NTP typically achieves 1–10 ms accuracy on cloud VMs. Aeron Cluster's deterministic timer logic (`cluster.scheduleTimer`) is based on `cluster.timeMs()` which ultimately derives from the system clock. A 10 ms clock skew between nodes means heartbeat timers may fire up to 10 ms early or late relative to the leader's expectation.

**Mitigation**: Use PTP (Precision Time Protocol, IEEE 1588) where the cloud provider supports hardware timestamping (AWS Time Sync Service with PTP, GCP VM clock sync). Verify `constant_tsc` and `nonstop_tsc` are set. Set Raft heartbeat intervals and timer deadlines with sufficient margin above the observed NTP accuracy.

---

### 2.12.7 Capability and Kernel Parameter Requirements

The following table summarises the system requirements that are unavailable or restricted in standard cloud and container environments, with the consequence if each is absent.

| Requirement | Bare metal | Cloud VM | Container | Consequence if absent |
|-------------|:----------:|:--------:|:---------:|----------------------|
| `isolcpus` / `nohz_full` | Available | Unavailable | Unavailable | OS scheduler interferes with busy-spin threads; jitter on every pipeline stage |
| `IORING_SETUP_SQPOLL` | Available | Available (Linux ≥ 5.10) | Restricted (seccomp) | Fallback to `epoll`; +1–3 µs per TCP I/O |
| `CAP_SYS_NICE` | Implicit | Available (privileged) | Requires explicit grant | SQPOLL thread cannot set RT priority |
| Huge pages (2 MB) | Available | Requires host config | Requires host config + pod spec | +5–10% Aeron IPC latency |
| `/dev/shm` ≥ 1 GB | Implicit | Available | Requires `--shm-size` | Aeron media driver fails to allocate term buffers |
| Dedicated physical cores | Inherent | Metal instances only | Never | vCPU steal causes unpredictable latency across all stages |
| PTP clock sync | Available | Provider-dependent | N/A | Timer skew up to 10 ms; spurious Raft elections possible |

---

## 2.13 Mitigating Virtual Environment Issues

Step-by-step configuration instructions for satisfying the deployment requirements in § 2.1.1 on cloud VMs and containerized infrastructure are in [virtual.md](virtual.md). Mitigations are ordered from highest to lowest impact: instance selection and CPU isolation first (addressing vCPU steal), followed by io_uring capabilities, shared memory, network placement, JVM tuning, clock synchronisation, and a pre-deployment validation checklist.
