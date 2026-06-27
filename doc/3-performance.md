# Chapter 3: Estimated Performance

End-to-end latency budget and throughput analysis for the FIX gateway. All simdfix decode/encode figures are measured values from `SimdFixBenchmark` (release build, ARM NEON, Apple M1 Pro); see [performance.md](performance.md) for the full benchmark table. Throughput figures are derived analytically from Little's Law applied to the risk system, which is the sole throughput bottleneck.

---

## 3.1 End-to-End Latency

The table below covers the **leader hot path** for a `NewOrderSingle` that passes both risk checks. Raft replication latency (2–5 µs) adds to the cluster stage on committed messages.

| Stage | Thread | Typical |
|-------|--------|---------|
| TCP receive → SPSC write | Core 1 (io_uring) | 0.5 µs |
| SPSC read → Aeron offer | Core 2 (FIX Session) | 0.5 µs |
| Aeron IPC → Java dispatch | Core 3 (Cluster) | 0.5 µs |
| Raft commit (3-node, co-lo) | Core 3 | 2–5 µs |
| Java → stream 2 → AppWorker | Core 7 | 0.5 µs |
| SIMD decode NOS (tokenize + getters) | Core 7 | **~130 ns** |
| Fast risk check (atomic loads) | Core 7 | ~30 ns |
| Risk Thread RPC (in-flight slot wait + response) | Core 8 | **100–500 ms** |
| SPSC → EgressWorker | Core 9 | ~150 ns |
| FIX encode NOS (sell-side order) | Core 9 | **~37 ns** |
| io_uring send → NIC | Core 10 | 0.5 µs |

The risk RPC dominates the end-to-end budget by six orders of magnitude. All pipeline stages outside the risk call total under 10 µs; individual order latency is determined almost entirely by the risk system RTT plus any queuing delay waiting for a free in-flight slot.

---

## 3.2 Virtualization Cost

Running in a cloud VM or container changes the latency of several pipeline stages. The risk RPC is unaffected — it is an external network call — but the surrounding pipeline incurs measurable overhead from three sources: I/O path changes, overlay network on the Raft replication path, and vCPU steal inflating all stages unpredictably.

### 3.2.1 Per-Stage Overhead

The table below shows the bare-metal figure, the worst-case overhead without mitigations, and the residual overhead after applying the configuration steps in [virtual.md](virtual.md).

| Stage | Bare-metal | Cloud VM (unmitigated) | Cloud VM (mitigated) |
|-------|-----------|------------------------|----------------------|
| TCP receive → SPSC write | 0.5 µs | +1–2 µs (epoll fallback) | +0 µs (io_uring granted) |
| Raft commit (3-node) | 2–5 µs | +50–100 µs (VPC overlay) | +20–50 µs (placement group + SR-IOV) |
| Aeron IPC (all IPC stages) | baseline | +5–10 % (4 KB pages, TLB pressure) | +0–2 % (huge pages pre-allocated) |
| io_uring send → NIC | 0.5 µs | +1–2 µs (epoll fallback) | +0 µs (io_uring granted) |
| Any stage (vCPU steal) | 0 | +5–20 % wall time (shared host) | ≈ 0 (bare-metal or dedicated host) |

### 3.2.2 I/O Path: io_uring vs. epoll Fallback

When `IORING_SETUP_SQPOLL` is unavailable, the reactor falls back to `epoll` + `SO_BUSY_POLL`. Each TCP receive and send incurs one additional syscall, and registered zero-copy buffers are replaced with kernel-copy `recvmsg`/`sendmsg`. The measured overhead is **+1–2 µs per TCP operation**. With two operations per order (receive on ingress, send on egress), the unmitigated I/O penalty is 2–4 µs total — negligible against the risk RTT but visible in the non-risk pipeline budget.

`AF_XDP` on a GCP gVNIC (kernel ≥ 5.10) eliminates this overhead entirely, matching bare-metal io_uring performance at the cost of BPF program complexity.

### 3.2.3 Raft Replication: Overlay Network

The Raft commit stage is the most sensitive to cloud networking. Bare-metal co-location gives 2–5 µs round-trip between cluster nodes; a VPC overlay (VXLAN/Geneve) raises this to 50–150 µs on unoptimised placement, and the commit must complete before the AppWorker can process the message.

| Placement | Raft RTT | Commit overhead vs. bare-metal |
|-----------|----------:|-------------------------------:|
| Bare-metal, same rack | 2–5 µs | — |
| Cloud VM, same AZ, placement group, SR-IOV | 20–50 µs | +15–45 µs |
| Cloud VM, same AZ, default VPC | 50–150 µs | +45–145 µs |
| Cloud VM, cross-AZ | 200–500 µs | +195–495 µs |

Cross-AZ deployment should be avoided for the Raft cluster; it moves the commit latency into the same order of magnitude as the risk RPC and makes it the second-largest latency contributor.

### 3.2.4 vCPU Steal: Unpredictable Inflation

On a shared host, the hypervisor may preempt any vCPU for an unbounded duration. Steal inflates every stage in the pipeline by an unpredictable multiplier; it cannot be bounded analytically. Observed effects on comparable workloads:

- Busy-spin stages (io_uring reactor, Aeron IPC poll) stall silently. A 5 ms steal event on Core 1 adds 5 ms to apparent TCP receive latency.
- The Aeron Cluster heartbeat timer may fire late, triggering a Raft election if the delay exceeds the election timeout. With the default 1 s timeout, steal spikes above 1 s (rare but observed under noisy-neighbour conditions) cause a leader election, temporarily halting commit throughput.
- The risk thread event loop makes no forward progress during steal; in-flight slots accumulate wait time, temporarily reducing effective throughput below the Little's Law ceiling.

The only reliable mitigation is eliminating steal: bare-metal cloud instances or dedicated-host VMs. Dedicated hosts reduce steal to near zero (hypervisor overhead only, typically < 0.1 %); shared commodity VMs should not be used for the busy-spin cores.

### 3.2.5 Net Impact on Non-Risk Pipeline

Combining the overhead sources:

| Environment | Non-risk pipeline total | Notes |
|-------------|------------------------:|-------|
| Bare-metal, co-located | < 10 µs | Reference |
| Cloud VM, mitigated (bare-metal instance, placement group, io_uring) | 30–60 µs | Raft commit dominates |
| Cloud VM, partially mitigated (dedicated host, epoll fallback) | 70–160 µs | Raft commit + I/O fallback |
| Cloud VM, unmitigated (shared host, epoll, cross-AZ) | 250–600 µs | Steal + overlay both visible |

In all cases the risk RPC (100–500 ms) remains the dominant term; the non-risk pipeline overhead is a second-order effect on individual order latency. The virtualization cost becomes significant only when comparing sub-millisecond pipeline latency targets or when the risk system is replaced by an in-process check.

---

## 3.3 Throughput Analysis

The system throughput ceiling is set by the risk system, which is the sole bottleneck. All other pipeline stages have headroom orders of magnitude above the risk-constrained limit.

### 3.3.1 Risk System: Little's Law

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

### 3.3.2 Queuing: Slot Wait Time

When the risk system RTT is variable, orders arriving faster than the risk system can clear them accumulate in the inbound SPSC. The time an order waits for a free slot is:

```
slot_wait ≈ (in_flight / N) × avg_RTT
```

At steady-state throughput equal to the ceiling (all N slots continuously occupied), the average slot wait approaches zero — each slot is freed just as a new order needs it. When the order arrival rate exceeds the ceiling, the SPSC fills and back-pressure propagates to the client TCP socket (§ 2.9.3 of [architecture.md](architecture.md)).

### 3.3.3 Other Pipeline Stages: Headroom

The following figures show headroom at two reference points: the current production configuration (25 outstanding, 100 ms RTT = 250 orders/sec) and the theoretical optimum row (100 outstanding ★, 1 ms RTT = 100 000 orders/sec). The Raft commit ceiling is shown for bare-metal and both cloud extremes because it is the only stage whose capacity changes materially across deployment environments.

| Stage | Capacity | Headroom @ 250/sec | Headroom @ 100 000/sec |
|-------|----------:|-------------------:|-----------------------:|
| io_uring TCP receive | > 500 000 frames/sec | > 2 000× | ~5× |
| Aeron IPC (aeron:ipc) | > 5 000 000 msgs/sec | > 20 000× | ~50× |
| Raft commit — bare-metal co-lo | ~50 000 ops/sec | ~200× | **< 1× ⚠** |
| Raft commit — cloud, min cost (placement group + SR-IOV) | ~10 000–25 000 ops/sec | ~40–100× | **< 1× ⚠** |
| Raft commit — cloud, max cost (default VPC or cross-AZ) | ~1 000–3 000 ops/sec | ~4–12× | **< 1× ⚠** |
| simdfix SIMD decode | > 1 000 000 msgs/sec | > 4 000× | ~10× |
| io_uring TCP send (egress) | > 500 000 frames/sec | > 2 000× | ~5× |

The Raft commit ceiling drops with overlay network RTT: bare-metal co-lo (2–5 µs) supports ~50 000 ops/sec; a mitigated cloud placement group (20–50 µs) supports ~10 000–25 000 ops/sec; a default VPC or cross-AZ path (200–500 µs) degrades to ~1 000–3 000 ops/sec. At the current production configuration (250 orders/sec) all cloud environments retain comfortable Raft headroom. The headroom collapse is only relevant at the theoretical optimum (100 outstanding, 1 ms risk RTT = 100 000 orders/sec), where Raft is the binding constraint in every deployment.

### 3.3.4 Summary

Throughput scales linearly with both the outstanding-request limit and the inverse of the risk RTT. At the current configuration of 25 outstanding requests, the ceiling ranges from 50 orders/sec (500 ms RTT) to 500 orders/sec (50 ms RTT) and is unaffected by cloud deployment — the risk system remains the binding constraint in all cloud environments at this operating point. The theoretical optimum of 100 outstanding requests at 1 ms RTT yields 100 000 orders/sec on bare-metal; in cloud, the Raft commit ceiling (1 000–25 000 ops/sec depending on placement) becomes the binding constraint instead. Individual order latency is not improved by raising the concurrency limit — each order still waits one full RTT for a risk response — but the pipeline never idles: all N slots progress through the risk system in parallel at all times.

---

## 3.4 Per-Client Throughput

Because orders from the same client are checked in sequence, each client is limited to one outstanding risk request at a time regardless of the global slot count. Per-client throughput is therefore:

```
per-client throughput = 1 / effective_RTT
```

In cloud environments, vCPU steal adds one steal-burst duration to each response observation. At 10% steal with ~5 ms average burst, effective RTT grows by roughly 0.5 ms — negligible against risk RTTs of 50 ms or more. The cloud impact becomes meaningful only at very low risk RTT (< 10 ms), where a cross-AZ Raft ceiling (~1 000–3 000 ops/sec shared across all clients) can limit per-client rate below the risk-only figure.

| RTT | Bare-metal | Cloud (min cost) | Cloud (max cost) |
|-----|----------:|-----------------:|-----------------:|
| 1 ms | 1 000 orders/sec | 1 000 orders/sec† | ≤ 120 orders/sec‡ |
| 50 ms | 20 orders/sec | 20 orders/sec | 20 orders/sec |
| 100 ms | 10 orders/sec | 10 orders/sec | 10 orders/sec |
| 500 ms | 2 orders/sec | 2 orders/sec | 2 orders/sec |

† Raft ceiling (10 000–25 000 ops/sec) exceeds per-client rate at this RTT for any realistic client count.  
‡ Raft ceiling (~1 000–3 000 ops/sec) shared across N concurrent clients; with 25 clients ≈ 40–120 orders/sec per client.

### 3.4.1 Aggregate Throughput as a Function of Client Count

Let C be the number of concurrently active clients (each submitting orders at the maximum per-client rate) and N the global outstanding-slot limit. The aggregate throughput is:

```
aggregate throughput = min(C, N) / RTT
```

When C < N each client occupies its own slot and the global limit is not reached; the system is **client-count limited**. When C ≥ N the global limit fills and additional clients yield no further aggregate gain; the system is **slot-limited**.

| Active clients (C) | Outstanding limit (N) | RTT = 100 ms | Limiting factor (bare-metal) |
|--------------------|----------------------:|-------------:|------------------------------|
| 5 | 25 | 50 orders/sec | Client count |
| 10 | 25 | 100 orders/sec | Client count |
| 25 | 25 | 250 orders/sec | Slot limit |
| 100 | 25 | 250 orders/sec | Slot limit |
| 100 | 100 ★ | 1 000 orders/sec | Slot limit |

The figures above assume C ≥ N — enough concurrently active clients to keep all slots occupied. When the client population is small (C < N), the aggregate throughput is C × (1/RTT) and the outstanding-slot limit has no effect.

### 3.4.2 Cloud Impact on Aggregate Throughput

The table below shows the risk-system ceiling alongside the Raft commit ceiling for min-cost and max-cost cloud deployments across representative configurations. The binding constraint is whichever ceiling is lower.

| Configuration | Risk ceiling | Raft — bare-metal | Raft — cloud min | Raft — cloud max | Binding in cloud |
|--------------|------------:|------------------:|-----------------:|-----------------:|------------------|
| 25 out, 500 ms RTT | 50 /sec | ~50 000 /sec | ~10 000–25 000 /sec | ~1 000–3 000 /sec | Risk |
| 25 out, 100 ms RTT | 250 /sec | ~50 000 /sec | ~10 000–25 000 /sec | ~1 000–3 000 /sec | Risk |
| 50 out, 50 ms RTT | 1 000 /sec | ~50 000 /sec | ~10 000–25 000 /sec | ~1 000–3 000 /sec | Risk (min) / Raft (max) |
| 100 out ★, 1 ms RTT | 100 000 /sec | ~50 000 /sec | ~10 000–25 000 /sec | ~1 000–3 000 /sec | Raft (all cloud) |

At production configuration (25 outstanding, 100 ms RTT) the risk system is the binding constraint in every cloud environment — including worst-case cross-AZ. Cloud deployment only shifts the binding constraint from risk to Raft when operating near the theoretical optimum (high outstanding count, low risk RTT). At 50 outstanding / 50 ms RTT the worst-case cloud Raft ceiling (1 000–3 000 ops/sec) can fall below the risk ceiling (1 000 ops/sec), making deployment topology a first-order throughput decision at that operating point.