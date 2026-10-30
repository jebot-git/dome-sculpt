# Paged Volume allocation qualification

This test-only checkpoint keeps the CPU fixture's persistent allocation-failure
policy and introduces a scoped ONEAPI policy. It changes neither Volume/storage
nor the compute backend, substitutes no CPU computation for GPU work, and does
not claim recovery from a permanently exhausted foreign driver heap.

The CPU path still denies every later C++ allocation after the selected failure
until the operation has unwound. Its original five operation matrices and exact
geometry/mask/layer owners, revision, history and stroke-state checks remain.
Both variants additionally require that the selected fault actually occurred;
an unrelated exception or a never-reached boundary cannot pass.

ONEAPI now fails one request at a time, allowing error translation and foreign
cleanup to allocate. A real, uninstrumented operation on a fresh Volume copy
primes the SDK before measurement and before each injected case. The original
source is unchanged. The complete measured matrix still executes actual Arc
compute. No caller-image allocator filter, backend stub, dropped boundary or
skipped source/history assertion is introduced.

The GPU fixture accepts `std::bad_alloc` or a typed SYCL memory rejection only
after observing its injected request. Standard `sycl::errc::memory_allocation`
is allowed; a generic SYCL runtime error is allowed only with native Unified
Runtime `OUT_OF_HOST_MEMORY`38 or `OUT_OF_DEVICE_MEMORY`39. Unknown errors are
rethrown. A configure-time compile/link check detects the pinned SDK's native
error accessor; if unavailable, the portable memory-allocation category is the
only additional accepted type. There is no error-message substring matching.
The actual SDK7.2 run reported SYCL category/runtime code1 with UR38 in34 cases
and UR39 in one case.

Priming must not conceal the application's own cold staging allocations. Two
fresh host threads therefore separately inject the first result-vector and
BrushDevice-record allocations before any queue constructor can run. Each
requires the exact expected allocation, an actual `bad_alloc`, and byte-exact
preservation of every caller Sample attribute. These are owned host allocation
checks; they do not certify every SDK initialization allocation.

## Diagnosis retained

The unchanged production candidate `dc8a36c` crashes under the old persistent
whole-process fault fixture with system package
`intel-level-zero-26.35.39758.11-3.fc44.x86_64` and SDK7.2. GDB records the last
injected failure through `operator new(nothrow)` in
`NEO::DrmMemoryManager::createBufferObjectInMemoryRegion`. The immediate crash is
`NEO::CommandContainer::fillReusableAllocationLists+1260`: a vector store to
`0x8(r14)` with `r14=0`, reached through command-buffer allocation and memory
copy submission. This is a foreign driver null dereference, not an observed
Volume publication fault or evidence of an asynchronous host-copy use-after-free.
The trace also shows repeated allocation failures during UR queue cleanup while
the fixture keeps the entire heap disabled.

A debugger-only one-shot comparison of the old binary instead stops on a clean
SYCL runtime/UR38 resource exception, which the old `bad_alloc`-only fixture did
not recognize. The qualified fixture tests the complete matrix with typed
resource handling and actual-hit/source-state assertions. The permanent heap
blackout driver failure remains recorded; no driver/runtime patch or general
process-wide OOM-recovery guarantee is implied. An upstream issue was not posted.

## Exact private validation

Fresh regular-user optimized CPU and ONEAPI cores, plus complete own C/C++
ASan/UBSan support including xatlas/Mikk/ufbx, were built coherently from this
private `dc8a36c` branch. No archive from a different source/header ABI is used
for the final qualification.

- CPU:20 native checks,12,920 individually injected persistent allocation
  boundaries,1/1 suite passing in10.29 seconds.
- Actual Intel Arc A770/ONEAPI:22 native checks,12,960 individually injected
  one-shot boundaries plus two cold owned-host stages,1/1 in35.53 seconds.
- ASan/UBSan/leaks: the CPU20 checks and12,920 persistent boundaries pass1/1
  in46.88 seconds, with the entire supporting own C/C++ core instrumented.

The five CPU counts are893 continued-stamp,2 End,38 Undo,63 Redo and11,924
recomposition. GPU counts are933,2,38,63 and11,924 respectively. Every measured
boundary must fail and preserve the same original owners/state. Zero-strength,
zero-pressure, protection, allocation-free Cancel, storage limits and immutable
ancestry checks remain. Logs and hash manifests retain the failed persistent
GPU attempts and final passing scopes separately.

This is a focused fixture qualification. The complete production goal, polygon
native/XR editing and other remaining requirements are not completed by it.
No installation, physical headset, display restart or external push is performed.
