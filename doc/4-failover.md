# Chapter 4: Failover

## 4.0 Fresh Start

A fresh start is a first-time boot with no prior Raft log, no cluster snapshot, and no replicated FIX session state. All sequence counters begin at zero and no `MemoryStorage` entries exist. This section describes the startup sequence. For failover from a crash, see §4.1 onwards.

### 4.0.1 Startup Order

The gateway processes must start in a specific order to ensure that reference data is in place and outbound routing is ready before inbound client traffic is accepted.

```
1.  Start aeronmd (Aeron Media Driver, Cores 11–14)
    · All IPC ring buffers allocated under /dev/shm/aeron
    · Must be running before any gateway process connects

2.  Start Cluster (Java, Cores 3–6)
    · Three nodes start; ConsensusModule holds election
    · Leader publishes CONNECT → C++ AppWorker (stream 2)
    · Cluster begins accepting messages on stream 1

3.  DB Server loads reference data
    · Connects to database via ODBC
    · Loads instruments, clients, risk parameters, session config
      in a single pass
    · Populates all gateway processes with the reference data
    · All processes must acknowledge receipt before proceeding
    · No external connections are opened until this step completes

4.  Risk system connection
    · How the Risk Thread establishes its connection to the
      external risk system is not specified here; this depends
      on the risk platform's session management protocol.

5.  Connect the outgoing gateway (Egress, Core 9)
    · EgressWorker opens TCP connection to the sell-side gateway
    · FIX Logon sent with MsgSeqNum = 1
    · Session must be ACTIVE before buy-side clients are admitted

6.  Connect the incoming gateway (Ingress, Cores 1–2)
    · TCP acceptor opened; buy-side clients may now connect
    · First inbound Logon triggers FixClusteredService to send
      Logon acknowledgement (outboundSeqNum = 1, inboundSeqNum = 1)
```

Reference data is loaded once before any external connection is opened. No instrument lookups, client validation, or risk limit checks can succeed without it, so all three external connections (risk system, sell-side gateway, buy-side clients) depend on step 3 completing successfully. The sell-side gateway is then connected before buy-side clients so that the order routing path is fully operational before any order can arrive.

### 4.0.2 Initial Sequence State

On a fresh start all FIX sequence counters are initialised to zero in the cluster:

| Counter | Initial value | First outbound value |
|---------|--------------|----------------------|
| `outboundSeqNum` | 0 | 1 (Logon) |
| `inboundSeqNum` | 0 | 1 (client Logon expected) |

No `MemoryStorage` entries exist; a `ResendRequest` for any range before the session begins cannot be serviced.

### 4.0.3 Cluster Quorum on Fresh Start

All three cluster nodes must reach quorum before the leader publishes `CONNECT`. If fewer than two nodes are reachable, the cluster blocks in candidate state and neither C++ process accepts work. The startup procedure requires all three nodes to be started before buy-side clients are admitted.

---

## 4.1 Overview

The cluster runs three nodes. At any moment one node is the **leader** — it holds the open client TCP connections, publishes commands to its local C++ workers, and sends to the sell-side gateway. The other two nodes are **followers** — they apply the same Raft log entries and maintain warm-standby state, but their TCP stacks are idle and their C++ workers discard all non-structural commands.

A failover is triggered when followers stop receiving Raft heartbeats from the leader and elect a replacement. The new leader then re-establishes all external connections using the FIX session state that was already replicated before the crash. No data is recovered from disk beyond what was committed to the Raft log; no manual intervention is required.

---

## 4.2 What Survives a Crash

The Raft log is the durability boundary. Anything committed (acknowledged by a quorum of two or more nodes) is guaranteed to be present on the new leader. Anything not yet committed is lost.

### Survives (committed Raft state)

| State | Owner | Notes |
|-------|-------|-------|
| `inboundSeqNum` | Java `FixClusteredService` | Last FIX sequence number received from the client and committed to the log. |
| `outboundSeqNum` | Java `FixClusteredService` | Last FIX sequence number sent to the client and committed to the log. |
| `SessionPhase` | Java `FixClusteredService` | Connection state at the last committed message (Active, LogonPending, etc.). |
| `MemoryStorage` | Java `FixClusteredService` | Circular buffer of the last 2500 outbound FIX messages, used for ResendRequest servicing. |
| Heartbeat timer deadlines | Java `FixClusteredService` | Registered via `cluster.scheduleTimer()`; replicated as log entries. |

### Lost (not committed)

| State | Notes |
|-------|-------|
| Inbound FIX frames in the leader's Aeron IPC ring buffer (stream 1) | Published by C++ ingress but not yet offered to the ConsensusModule ingress channel, or offered but not yet replicated to a quorum. |
| FIX frames received by the leader's TCP stack but not yet written to Aeron IPC | Still in the kernel socket buffer on the crashed node. |
| In-flight Risk Thread requests (up to 25) | The correlation table exists only in the leader's Risk Thread. All in-flight orders are treated as rejected on the new leader. |
| C++ AppWorker transient decode state | Stateless by design; resets on CONNECT. |

---

## 4.3 Failure Detection and Election

Aeron Cluster uses a heartbeat-based failure detector. The leader publishes a Raft heartbeat to followers at a configurable interval (default 250 ms). A follower starts an election if it receives no heartbeat for an election timeout period (default 1000 ms, randomised per node to avoid split votes).

```
t = 0 ms    Leader A stops responding (crash, GC pause, network partition).

t = 0–1000  Followers B and C wait out their randomised election timeout.
            B's timeout expires first (e.g. at t = 820 ms).

t = 820     B increments its term, transitions to Candidate, votes for itself,
            sends RequestVote to A and C.

t = 821     C receives RequestVote, grants its vote to B (C's log is at least
            as up-to-date as B's).

t = 822     B has votes from itself and C — a quorum of 2/3.
            B transitions to Leader.
```

The election completes in one round-trip between B and C when no split-vote occurs. With randomised timeouts the probability of a split vote in a 3-node cluster is low, but a second round adds another ~election-timeout window in the worst case.

---

## 4.4 Failover Sequence

The timeline below shows the sequence of events from crash detection to full FIX session resumption. Time values are illustrative; actual timing depends on election timeout configuration and network RTT between cluster nodes.

```
Node A (crashed)         Node B (new leader)              Buy-side client
─────────────────        ─────────────────────────         ────────────────
[CRASH]
                         t=820ms  Election won
                                  FixClusteredService
                                  onRoleChange(LEADER)
                                  │
                                  ├─ Publish CONNECT
                                  │  → C++ AppWorker (stream 2)
                                  │
                                  ├─ Start TCP acceptor
                                  │  (VIP address fails over to B)
                                  │
                                  └─ Egress reconnects
                                     → Sell-side gateway
                                         (FIX Logon)

                         t=825ms  Client TCP connects to B's VIP

                         t=826ms  FixClusteredService sends Logon
                                  (outboundSeqNum + 1,
                                   NextExpectedMsgSeqNum = inboundSeqNum + 1)
                                  │
                                  AppWorker encodes Logon (SBE→FIX)
                                  → stream 6 → Ingress → client TCP

                                                           t=827ms  Client
                                                           receives Logon,
                                                           validates seqs

                                                           ── If seqs match:
                                                           Client sends Logon
                                                           response → session
                                                           ACTIVE

                                                           ── If gap detected:
                                                           Client sends
                                                           ResendRequest
                                                           (see §4.5)

                         t=828ms  FixClusteredService receives
                                  client Logon response (committed)
                                  inboundSeqNum advances

                         t=828ms  Session ACTIVE
                                  Normal order flow resumes
```

---

## 4.5 FIX Session Resumption

### 4.5.1 Sequence Number Continuity

The new leader resumes the FIX session with the sequence numbers that were committed at the time of the crash. The outbound Logon carries:

- `MsgSeqNum` (tag 34) = `outboundSeqNum + 1` — the next outbound sequence the new leader will use.
- `NextExpectedMsgSeqNum` (tag 789, FIX 4.4) = `inboundSeqNum + 1` — the next inbound sequence the new leader expects from the client.

### 4.5.2 Client-Side Gap Detection

Messages sent by the old leader that were in its TCP send buffer at the time of the crash may or may not have been received by the client, depending on where the TCP connection was cut.

**Case A — client received all committed messages**: the client's expected inbound sequence matches `outboundSeqNum + 1`. The session resumes without a gap.

**Case B — client received messages A sent after the last committed outbound sequence**: this occurs if A sent FIX bytes from its TCP buffer for messages it had already published downstream but that were not yet committed to Raft (i.e. the cluster had sequenced them but the commit was in-flight at crash time). The client's expected sequence is higher than the new leader's `outboundSeqNum + 1`. From the client's perspective, the new leader has reset the sequence. The new leader responds by sending a SequenceReset-GapFill (tag 123 = Y, `NewSeqNo` = client's expected inbound seq) to bridge the gap, then continues normally. The skipped range corresponds to messages the cluster never committed and therefore cannot retransmit.

**Case C — client missed messages the old leader committed and sent**: the client sends a `ResendRequest` for the missing range. The new leader services it from `MemoryStorage` (last 2500 outbound messages). For session-level messages in the resend range (heartbeats, test requests), the leader emits a SequenceReset-GapFill instead of retransmitting them. Application messages (ExecutionReport, etc.) are retransmitted with `PossDupFlag=Y` (tag 43 = Y).

### 4.5.3 Inbound Gap at Client

If the client had sent messages that reached the old leader's TCP stack but never made it into the Raft log, those messages are permanently lost from the cluster's perspective. The new leader expects `inboundSeqNum + 1` from the client; if the client's next message carries a higher sequence number, the new leader sends a `ResendRequest` to the client for the missing range. The client retransmits with `PossDupFlag=Y`.

---

## 4.6 C++ Process Restart on the New Leader

The C++ processes on the new leader (Ingress, Application, Egress) have been running throughout — they were just in follower mode with no TCP connections and with the AppWorker discarding SEND commands.

### 4.6.1 AppWorker (Core 7)

On receiving the `CONNECT` command the AppWorker:

1. Resets its internal decode state (`PayloadDecoder::reset()`).
2. Clears any pending in-process risk state (the fast atomic counters that feed the pre-risk check).
3. Begins actively processing `FORWARD_APP` and `SEND`/`RESEND` commands instead of discarding them.

The AppWorker holds no durable state; there is nothing to recover.

### 4.6.2 Risk Thread (Core 8)

The Risk Thread on the new leader has zero in-flight requests at the point of failover — it was idle during follower operation. It begins accepting orders from the SPSC immediately after the AppWorker activates.

Orders that were in-flight in the old leader's Risk Thread at the time of the crash are lost. From the cluster's perspective those orders were in the `FORWARD_APP` dispatch path and were never committed as outbound messages. The client will either retransmit them (if it does not receive an ExecutionReport within its timeout) or the session-level ResendRequest exchange will cause the cluster to determine that no outbound ExecutionReport was ever sent for those orders, triggering client-side retransmission.

### 4.6.3 Ingress (Cores 1–2)

The new leader's Ingress process accepts the client TCP reconnection via the cluster's shared VIP address (or DNS failover). It begins forwarding frames to the cluster via stream 1 as soon as the TCP connection is accepted.

### 4.6.4 EgressWorker (Core 9)

The EgressWorker on the new leader reconnects to the sell-side gateway using a standard FIX Logon. The sell-side gateway sees a disconnection from the old leader's TCP connection and then a new inbound FIX session from the new leader. Any orders sent by the old leader's Egress to the sell-side gateway but not yet acknowledged (via ExecutionReport) are handled by the sell-side gateway's own cancel-on-disconnect policy or by the gateway's FIX ResendRequest to the sell-side.

---

## 4.7 In-Flight Orders at Crash Time

The fate of an order depends on how far it had progressed through the pipeline at the moment of the crash.

| Stage at crash | Fate |
|----------------|------|
| In client TCP send buffer (not yet received by gateway) | Not lost. Client retransmits after reconnecting. |
| Received by old leader's TCP stack, not yet in Aeron IPC (stream 1) | Lost. Not committed. Client detects gap via seq numbers; resends. |
| In Aeron IPC (stream 1), not yet committed by Raft | Lost. Not committed. Client resends. |
| Committed by Raft, `FORWARD_APP` not yet dispatched to C++ | Replayed on new leader. New leader dispatches `FORWARD_APP`. |
| Dispatched to C++ AppWorker, not yet in Risk SPSC | Replayed on new leader (same `FORWARD_APP` entry). |
| In Risk Thread SPSC or in-flight to risk system | Lost (risk thread state not replicated). New leader re-dispatches the same `FORWARD_APP` from Raft log. Order reaches risk system again as a new request. |
| Risk-approved, `OrderCommand` published (stream 4) | Not yet committed as outbound. New leader re-dispatches `FORWARD_APP`; order re-enters risk. |
| `OrderCommand` sent to sell-side, no ExecutionReport yet | Sell-side cancel-on-disconnect or order-status query determines outcome. Client may resend order if no ExecutionReport arrives within its timeout. |
| ExecutionReport received from sell-side, outbound FIX committed | Survives. New leader services ResendRequest from MemoryStorage if client requests retransmission. |

---

## 4.8 Follower Divergence Prevention During Failover

During the election window there is no leader. The cluster stops committing new log entries. The Aeron ingress channel accepts no new messages from the C++ Ingress processes (the `offer()` call returns `NOT_CONNECTED` or `BACK_PRESSURED`). The C++ Ingress thread sees back-pressure, which propagates to the client TCP socket as described in § 2.9.3.

Once a new leader is elected and commits its first no-op log entry (a standard Raft safety measure to flush uncommitted entries from the previous term), normal commit processing resumes. The brief window during which the cluster rejects new ingress messages is bounded by the election timeout.

---

## 4.9 Failover Timing Budget

| Event | Duration |
|-------|----------|
| Leader crash to election timeout expiry | 500–1000 ms (configurable) |
| Election (RequestVote round-trip, co-lo) | 1–5 ms |
| `onRoleChange(LEADER)` → CONNECT on stream 2 | < 1 ms |
| TCP VIP failover (anycast / BGP) | 10–100 ms |
| Client TCP reconnect + Logon round-trip | 1–5 ms |
| ResendRequest exchange (if gap) | 1–10 ms |
| Sell-side gateway FIX Logon | 1–5 ms |
| **Total (election timeout dominates)** | **~500–1100 ms** |

The election timeout is the dominant term. It can be reduced at the cost of increased sensitivity to transient slowdowns — a GC pause longer than the election timeout on the leader triggers a spurious election. With ZGC and a 6 GB heap, GC pauses are consistently under 1 ms, so an election timeout of 500 ms provides a comfortable margin without materially increasing failover time.

---

## 4.10 External Connection Failures

External connection failures are distinct from cluster failover: the Raft cluster and all three nodes remain healthy. Only an external TCP connection is lost. The two external TCP connections — the buy-side client and the sell-side exchange — have opposite recovery behaviours.

### 4.10.1 Buy-side Client Disconnect

The gateway is the FIX acceptor for buy-side clients. When a client's TCP connection drops the gateway takes no outbound action:

```
Client disconnects (TCP RST / timeout)
    │
    ├─ Reactor (Core 1): CQE signals ECONNRESET / EOF on client fd.
    │   Closes fd. Removes fd from registered buffer set.
    │
    ├─ FIX Session (Core 2): detects fd closure.
    │   Publishes disconnect notification to cluster (stream 1).
    │
    └─ FixClusteredService: commits disconnect.
        SessionPhase → DISCONNECTED.
        Heartbeat and test-request timers cancelled.
        inboundSeqNum / outboundSeqNum / MemoryStorage: preserved unchanged.
```

The TCP acceptor remains open throughout. All committed session state is preserved in the cluster. The gateway waits indefinitely for the client to reconnect. No reconnect timer fires; no Logout is sent; no alert is emitted beyond normal operational logging.

When the client reconnects and sends a Logon, the sequence picks up from §4.5 (FIX Session Resumption). The cluster resumes from the committed `inboundSeqNum` and `outboundSeqNum` exactly as in a cluster failover resumption.

**In-flight orders at disconnect time.** Orders that had been committed to the Raft log as `FORWARD_APP` but not yet returned from the risk system, and orders for which the cluster had emitted `SEND` but the TCP bytes had not been flushed before the connection dropped, are handled identically to the crash case described in §4.7. The client's own timeout-and-retransmit behaviour drives recovery.

### 4.10.2 Sell-side Exchange Disconnect

The gateway is the FIX initiator toward the sell-side exchange. When the exchange connection drops the EgressWorker immediately attempts to reconnect, cycling through the configured endpoint list in priority order:

```
Exchange connection drops (TCP RST / FIX Logout from exchange / timeout)
    │
    ├─ io_uring Egress Reactor (Core 10): CQE signals disconnect on exchange fd.
    │   Closes fd. Notifies EgressWorker via event flag.
    │
    ├─ EgressWorker (Core 9): receives disconnect notification.
    │   Suspends processing of stream 4 (no new orders sent during reconnect).
    │   Selects next endpoint from priority list (primary → secondary → …).
    │
    ├─ TCP connect attempt to selected endpoint.
    │   ├─ Success: sends FIX Logon (initiator). Waits for Logon-Ack.
    │   │   On Logon-Ack: session ACTIVE on alternate endpoint.
    │   │   Resumes processing stream 4.
    │   └─ Failure (refused / timeout): selects next endpoint. Repeats.
    │       If all endpoints exhausted: waits a configured back-off interval,
    │       then restarts from the top of the priority list.
    │
    └─ Cluster: not notified of the disconnect. Cluster state is unaffected.
        Buy-side sessions remain ACTIVE throughout.
        RISK_APPROVED commands continue to accumulate on stream 4.
        Back-pressure on stream 4 propagates to the AppWorker SPSC once the
        EgressWorker's send queue is full (see §2.9.3).
```

The reconnect sequence is entirely local to the EgressWorker; it does not involve the cluster or modify any cluster-committed state. The sell-side FIX session sequence numbers are maintained by the EgressWorker locally and are negotiated with the exchange during the Logon handshake. If the exchange detects a sequence gap it sends a `ResendRequest`; the EgressWorker services it from its local outbound message buffer.

**In-flight orders at disconnect time.** Orders sent to the exchange but not yet acknowledged by an `ExecutionReport` are subject to the exchange's cancel-on-disconnect policy. The EgressWorker does not automatically retransmit them after reconnect — retransmission is driven by the exchange's `ResendRequest` (for messages the exchange received) or by the client's timeout-and-retransmit (for orders the exchange never saw). The EgressWorker maintains its own outbound buffer (separate from `MemoryStorage` in the cluster) for this purpose.

### 4.10.3 Comparison

| Property | Buy-side disconnect | Sell-side disconnect |
|----------|--------------------|--------------------|
| Gateway role | Acceptor (passive) | Initiator (active) |
| Recovery action | Wait for client to reconnect | Connect to next endpoint immediately |
| Cluster state affected | `SessionPhase → DISCONNECTED` | No change |
| Sequence numbers | Preserved in cluster, resumed on reconnect | Maintained locally in EgressWorker; negotiated at Logon |
| Buy-side sessions during outage | Remain ACTIVE; new orders queue at stream 4 back-pressure | Remain ACTIVE; new orders queue at stream 4 back-pressure |
| Order fate | See §4.7 | Exchange cancel-on-disconnect policy + ResendRequest |