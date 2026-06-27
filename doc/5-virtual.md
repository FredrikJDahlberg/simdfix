# Chapter 5: Virtual and Cloud Environments

## 5.1 Overview

The gateway is designed for bare-metal Linux with dedicated, isolated CPU cores, hardware-direct I/O, and a kernel that the operator controls entirely. Cloud VMs and containerised deployments relax or remove all three of those assumptions. This document describes the resulting constraints and provides step-by-step configuration to recover as much of the bare-metal operating envelope as the environment permits.

### 5.1.1 Compute: vCPU Preemption and CPU Steal

Cloud VMs run on vCPUs — logical threads multiplexed across physical cores by the hypervisor. The hypervisor may preempt a vCPU at any moment to service another tenant, introducing *CPU steal time*. On commodity instances, steal commonly reaches 5–20 % under contention.

Impact on this system: busy-spin threads stall invisibly, inflating every stage of the latency budget by an unpredictable amount; the risk thread's event loop makes no progress during steal, so in-flight risk RTT grows; Aeron Cluster heartbeats may be delayed, triggering spurious Raft elections. Kernel boot parameters (`isolcpus`, `nohz_full`, `rcu_nocbs`) that eliminate OS scheduler interference are ineffective on vCPUs because the hypervisor scheduler operates below the guest kernel.

### 5.1.2 I/O: io_uring Restrictions

`io_uring` with `IORING_SETUP_SQPOLL` requires `CAP_SYS_NICE` and kernel ≥ 5.10. Default seccomp profiles in Docker and Kubernetes block several required opcodes. Serverless runtimes disable io_uring entirely. When io_uring is unavailable, `epoll` + `SO_BUSY_POLL` is the primary fallback; `AF_XDP` is the secondary fallback on XDP-capable vNICs. See the [io_uring section](#io_uring-capabilities-and-seccomp) for details and fallback implementations.

### 5.1.3 Memory: Shared Memory and Huge Pages

Aeron IPC requires large memory-mapped regions under `/dev/shm`. Docker defaults this to 64 MB; the media driver needs at minimum 288 MB (six streams × three 16 MB term buffers) plus archive headroom — provision at least 1 GB. Huge pages (`vm.nr_hugepages`) are a host-level kernel parameter that cannot be set from inside a container; the host must pre-allocate them before the gateway starts.

### 5.1.4 Network: Overlay Latency

VPC networks in most cloud providers use VXLAN or Geneve encapsulation for tenant isolation, adding 5–20 µs per inter-VM packet. This is negligible for client TCP traffic but becomes the second-largest contributor to end-to-end latency on the Raft replication path between cluster nodes (100 µs vs. 2–5 µs on bare-metal co-lo). Place all cluster nodes in the same availability zone within a placement group and use SR-IOV or accelerated vNIC drivers to minimise this.

### 5.1.5 Summary

| Challenge | Primary mitigation | Bare-metal equivalent |
|-----------|-------------------|-----------------------|
| vCPU steal | GCP sole-tenant node or Bare Metal Solution | `isolcpus` + `nohz_full` |
| io_uring SQPOLL unavailable | Grant `CAP_SYS_NICE`; fallback to `SO_BUSY_POLL` | Available by default |
| `/dev/shm` too small | `--shm-size=2g` / `emptyDir` | Host default sufficient |
| No huge pages in container | Pre-allocate on host; K8s `hugepages-2Mi` resource | `vm.nr_hugepages` in sysctl |
| Overlay network latency | Compact placement policy + gVNIC / `hostNetwork: true` | Physical co-location |
| Clock drift | GCP VM PTP (`/dev/ptp0`); tune `chrony` against `metadata.google.internal` | Hardware TSC + PTP |

The configuration steps below address each row in impact order. All shell commands and YAML examples are tested against Linux 6.1+ with Kubernetes 1.28+ and Docker 24+.

---

## 5.2 Instance and Host Selection

The most effective mitigation is eliminating the hypervisor from the critical path entirely.

**Bare-metal and sole-tenant options** give the full set of kernel parameters, no vCPU steal, and hardware-level CPU isolation. Recommended GCP options in order of preference:

| Option | Notes |
|--------|-------|
| Bare Metal Solution (`o3-*` project) | No hypervisor; full kernel control including `isolcpus`; requires GCP Bare Metal Solution project enablement |
| `c3-standard-*` on sole-tenant nodes | NUMA-aware placement; sole-tenant eliminates co-tenancy; physical core pinning via vCPU topology |
| `c3d-standard-*` on sole-tenant nodes | AMD EPYC Genoa; higher memory bandwidth; latency comparable to `c3` |

**Sole-tenant nodes** provide physical host exclusivity without Bare Metal Solution overhead. vCPU steal drops to near zero because no other tenants share the physical host, though the hypervisor layer itself still adds a thin scheduling overhead.

Disable SMT (hyperthreading) at the hypervisor or BIOS level for physical cores allocated to busy-spin threads. Sibling threads competing for the same execution units degrade pipeline throughput even when vCPU steal is zero.

---

## 5.3 CPU Isolation Without `isolcpus`

On cloud VMs where kernel boot parameters cannot be changed, CPU isolation must be approximated through OS-level mechanisms.

**cgroup `cpuset`**: partition CPUs into two sets — one for OS and system services, one for gateway threads. Assign the gateway process to the isolated set via cgroups v2:

```bash
# Create an isolated cpuset for gateway threads (cores 1-10)
mkdir /sys/fs/cgroup/gateway
echo "1-10" > /sys/fs/cgroup/gateway/cpuset.cpus
echo "0"    > /sys/fs/cgroup/gateway/cpuset.mems
echo $GATEWAY_PID > /sys/fs/cgroup/gateway/cgroup.procs

# Restrict the system slice to core 0 only
echo "0" > /sys/fs/cgroup/system.slice/cpuset.cpus
```

**Real-time scheduling**: elevate busy-spin threads to `SCHED_FIFO` to prevent the OS scheduler from preempting them in favour of normal-priority processes. This does not prevent hypervisor preemption but eliminates intra-guest interference:

```cpp
sched_param param{.sched_priority = 80};
pthread_setschedparam(pthread_self(), SCHED_FIFO, &param);
```

Requires `CAP_SYS_NICE` or `RLIMIT_RTPRIO` set to a non-zero value. On Kubernetes, add `CAP_SYS_NICE` to the container `securityContext`.

**IRQ affinity**: move hardware interrupt handling away from gateway cores. Bind NIC IRQs to the OS-reserved core (core 0) using `/proc/irq/*/smp_affinity_list`.

---

## 5.4 io_uring: Capabilities and Seccomp

Grant the required Linux capabilities and supply a permissive seccomp profile. The minimum capability set for full io_uring operation is:

```
CAP_SYS_NICE    — SQPOLL kernel thread priority
CAP_IPC_LOCK    — mlock() for registered buffer pages
CAP_NET_ADMIN   — socket option tuning (SO_BUSY_POLL)
```

**Docker**:

```bash
docker run \
  --cap-add SYS_NICE \
  --cap-add IPC_LOCK \
  --cap-add NET_ADMIN \
  --security-opt seccomp=gateway-seccomp.json \
  --ulimit memlock=-1 \
  gateway:latest
```

**Kubernetes** (`securityContext`):

```yaml
securityContext:
  capabilities:
    add: ["SYS_NICE", "IPC_LOCK", "NET_ADMIN"]
  seccompProfile:
    type: Localhost
    localhostProfile: gateway-seccomp.json
```

The custom seccomp profile must allow: `io_uring_setup`, `io_uring_enter`, `io_uring_register`, `mmap`, `mlock`, `setsockopt`, and all standard socket syscalls. If a custom profile cannot be deployed, `seccomp=unconfined` is the fallback; this is acceptable in an isolated single-tenant environment.

Verify io_uring availability at startup and emit a clear error rather than silently falling back to `epoll`:

```cpp
int fd = io_uring_queue_init(QUEUE_DEPTH, &ring,
                              IORING_SETUP_SQPOLL | IORING_SETUP_SQ_AFF);
if (fd < 0)
    throw std::runtime_error("io_uring unavailable: " + std::string(strerror(-fd))
                             + " — run with CAP_SYS_NICE or disable SQPOLL");
```

### 5.4.1 Fallback: `epoll` + `SO_BUSY_POLL`

Use this when io_uring SQPOLL cannot be enabled (seccomp restriction, missing capability, kernel < 5.10, or serverless runtime).

```cpp
// Set on the accepted client socket before adding to epoll
int busy_poll_us = 50;   // poll for up to 50 µs before sleeping
setsockopt(fd, SOL_SOCKET, SO_BUSY_POLL, &busy_poll_us, sizeof(busy_poll_us));

// Tell epoll to prefer busy-poll for this socket (Linux ≥ 5.11)
int prefer = 1;
setsockopt(fd, SOL_SOCKET, SO_PREFER_BUSY_POLL, &prefer, sizeof(prefer));

// Create epoll with busy-poll budget (Linux ≥ 5.11)
int epfd = epoll_create1(EPOLL_CLOEXEC);
uint32_t budget = 8;   // messages to busy-poll per epoll_wait call
ioctl(epfd, EPIOCSPARAMS, &budget);   // EPIOCGPARAMS / EPIOCSPARAMS
```

No additional Linux capabilities are required. The reactor loop is otherwise identical to the io_uring path: edge-triggered `EPOLLIN`/`EPOLLOUT`, non-blocking `recvmsg`/`sendmsg`, read/write until `EAGAIN`. Zero-copy registered buffers are not available; the kernel copies each message through a kernel buffer (typically one extra copy per syscall). Expected overhead vs. io_uring SQPOLL: **+1–2 µs per message** on a local socket path.

### 5.4.2 Fallback: `AF_XDP`

Use this on GCP VMs with a gVNIC driver (kernel ≥ 5.10) when both io_uring SQPOLL and `SO_BUSY_POLL` are insufficient. `AF_XDP` bypasses the kernel socket stack entirely on the receive path, delivering packets directly to a user-space ring buffer.

Required capabilities: `CAP_BPF` and `CAP_NET_ADMIN`.

```bash
# Docker
docker run \
  --cap-add BPF \
  --cap-add NET_ADMIN \
  --cap-add SYS_ADMIN \   # needed to pin BPF maps to /sys/fs/bpf
  gateway:latest
```

The gateway must load an XDP program on the NIC at startup to redirect matching packets to the `AF_XDP` socket. A minimal redirect program (using `libxdp` or `libbpf`):

```c
// xdp_redirect.c — redirect all TCP traffic on port 9200 to AF_XDP socket
SEC("xdp")
int xdp_redirect_fn(struct xdp_md *ctx)
{
    // parse Ethernet/IP/TCP headers and match destination port
    // return bpf_redirect_map(&xsks_map, rx_queue_index, XDP_PASS)
    // on match; XDP_PASS for everything else
}
```

At runtime, replace the io_uring reactor with a single thread that polls the `AF_XDP` receive ring (UMEM fill/completion rings) in a tight loop. Outbound sends use the transmit ring in the same UMEM region, giving zero-copy in both directions. Expected overhead vs. io_uring SQPOLL: **comparable or lower** on XDP-capable hardware; the main cost is BPF program complexity and operational overhead.

---

## 5.5 Shared Memory and Huge Pages

**Sizing `/dev/shm`**: compute the required size as `streams × term_buffers × term_size + archive`. With default Aeron settings (16 MB term buffers, 3 terms per stream, 6 streams) plus the Raft log archive:

```
shm_required = 6 × 3 × 16 MB  +  256 MB (archive)  ≈  544 MB
```

Add 50% headroom: provision at least **1 GB**.

```bash
# Docker
--shm-size=2g

# Kubernetes — emptyDir backed by tmpfs
volumes:
  - name: aeron-shm
    emptyDir:
      medium: Memory
      sizeLimit: 2Gi
volumeMounts:
  - name: aeron-shm
    mountPath: /dev/shm
```

**Huge pages**: pre-allocate at VM boot time. This requires host-level configuration; it cannot be done from within the container:

```bash
# /etc/sysctl.d/99-hugepages.conf on the host
vm.nr_hugepages = 512       # 512 × 2 MB = 1 GB
vm.hugetlb_shm_group = 1001 # GID of the aeron process
```

On Kubernetes, declare the resource in the pod spec and mount the hugetlbfs:

```yaml
resources:
  requests:
    hugepages-2Mi: 1Gi
    memory: 8Gi
  limits:
    hugepages-2Mi: 1Gi
    memory: 8Gi
volumes:
  - name: hugepages
    emptyDir:
      medium: HugePages
```

If huge pages cannot be pre-allocated, advise the kernel to back Aeron's mmap regions with transparent huge pages:

```bash
echo always > /sys/kernel/mm/transparent_hugepage/enabled
```

This is a best-effort fallback; the kernel may not honour it under memory pressure.

---

## 5.6 Network Configuration

**Placement**: co-locate all three cluster nodes in the same availability zone and, where supported, within a placement group or proximity placement group. This minimises physical distance and reduces VPC round-trip latency for Raft replication.

| Placement | Expected intra-cluster RTT |
|-----------|---------------------------:|
| Compact placement policy (same zone) | < 50 µs |
| Same zone, no placement policy | 50–150 µs |
| Cross-zone (same region) | 200–500 µs |

**gVNIC (virtio-net replacement)**: enable GCP's gVNIC driver at VM creation to bypass the default virtio-net software path:

```bash
# Enable gVNIC at VM creation
gcloud compute instances create INSTANCE_NAME \
  --network-interface=nic-type=GVNIC

# Verify gVNIC driver is active on the running instance
ethtool -i eth0 | grep driver   # should show gvnic
```

**Kubernetes host networking**: to bypass the pod overlay (CNI), run gateway pods with `hostNetwork: true`. This places the pod directly on the node's network namespace, eliminating the VXLAN/Geneve encapsulation penalty:

```yaml
spec:
  hostNetwork: true
  dnsPolicy: ClusterFirstWithHostNet
```

Use pod anti-affinity to guarantee that each cluster node pod lands on a different physical host, preventing a single-host failure from eliminating a quorum.

---

## 5.7 Aeron and JVM Tuning for Cloud

**Raft election timeout**: increase to account for vCPU steal and GC pause variability. Measure the worst-case observed GC pause on the target instance type under load and set the timeout to at least 5×:

```bash
# Aeron Cluster system properties (Java process)
-Daeron.cluster.election.timeout.ns=5000000000   # 5 s (vs default 1 s)
-Daeron.cluster.heartbeat.interval.ns=500000000   # 500 ms
```

**ZGC heap configuration**: keep the heap bounded and pre-faulted. On VMs with memory that is not overcommitted:

```bash
-XX:+UseZGC -XX:+ZGenerational
-Xms6g -Xmx6g
-XX:+AlwaysPreTouch
-XX:ParallelGCThreads=2        # limit GC threads competing for vCPUs
-XX:ConcGCThreads=2
-XX:+DisableExplicitGC
```

**Aeron media driver threading**: pin the conductor thread and set busy-spin idle strategies:

```bash
-Daeron.threading.mode=DEDICATED
-Daeron.conductor.idle.strategy=noop
-Daeron.sender.idle.strategy=noop
-Daeron.receiver.idle.strategy=noop
-Daeron.dir=/dev/shm/aeron
```

**Socket buffer sizes**: cloud vNICs benefit from larger socket buffers to absorb burst jitter:

```bash
-Daeron.socket.so_rcvbuf=4194304    # 4 MB
-Daeron.socket.so_sndbuf=4194304
```

---

## 5.8 Clock Synchronisation

Verify the clock source is the invariant TSC and that PTP is active before starting the cluster:

```bash
# Verify TSC is the active clock source
cat /sys/devices/system/clocksource/clocksource0/current_clocksource
# Expected: tsc

# Verify invariant TSC flags
grep -m1 "constant_tsc\|nonstop_tsc" /proc/cpuinfo

# GCP: PTP hardware clock available on C3 and N2 instance families
cat /sys/class/ptp/ptp0/clock_name   # expected: Google Compute Engine Virtual PTP

# Check synchronisation offset (should be < 100 µs for PTP)
chronyc tracking | grep "RMS offset"
```

Configure `chrony` for tight synchronisation:

```ini
# /etc/chrony.conf
server metadata.google.internal prefer iburst minpoll 0 maxpoll 0  # GCP Time Sync
makestep 0.1 3
maxdistance 1.0
```

If PTP is unavailable and NTP accuracy exceeds 1 ms, widen the heartbeat interval proportionally:

```
heartbeat_interval ≥ 2 × NTP_accuracy + safety_margin
```

For 5 ms NTP accuracy: heartbeat interval ≥ 15 ms, election timeout ≥ 5 s.

---

## 5.9 Validation Checklist

Run the following checks on the target environment before deploying to production.

```bash
# 1. Verify no vCPU steal on isolated cores during a 60-second busy-spin
taskset -c 1-10 stress-ng --cpu 10 --timeout 60 &
vmstat 1 60 | awk '{print $17}'   # st column; should be 0 throughout

# 2. Verify io_uring SQPOLL is available
cat > /tmp/uring_check.c << 'EOF'
#include <liburing.h>
#include <stdio.h>
int main() {
    struct io_uring ring;
    int r = io_uring_queue_init(32, &ring, IORING_SETUP_SQPOLL);
    printf(r < 0 ? "SQPOLL unavailable: %s\n" : "SQPOLL OK\n", strerror(-r));
    if (r == 0) io_uring_queue_exit(&ring);
}
EOF
gcc -o /tmp/uring_check /tmp/uring_check.c -luring && /tmp/uring_check

# 3. Verify /dev/shm capacity
df -h /dev/shm   # Available should exceed 1 GB

# 4. Verify huge pages are allocated
grep HugePages_Total /proc/meminfo   # should be >= 512

# 5. Verify TSC clock source
cat /sys/devices/system/clocksource/clocksource0/current_clocksource

# 6. Measure Aeron IPC round-trip latency (warm path)
# Run aeron-samples PingPong benchmark; p99 should be < 5 µs on same host

# 7. Verify Raft cluster stability under load — no elections during 10-minute run
# Monitor: grep "ELECTION" aeron-cluster.log | wc -l   # expected: 0
```