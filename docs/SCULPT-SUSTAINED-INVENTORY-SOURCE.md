# Inventory, device and sample recording source checkpoint

Product source255654a7 is a new child of05178834. The previous passing small
logic source/ELF and failed allocator source/ELF stay unchanged. This checkpoint
has a warning-free whole consumer syntax check only, against this child's own
8a05-derived headers. It has no new object, link, EGL, current-combined consumer,
inventory, production or Arc qualification. Rootd1f40159 providers stay untouched.

The source implements explicit inventory, tier, object count, voxel, stroke/mask/
view/held/repetition counts, resolution and software/Arc device arguments. Integer
parsing consumes the full value and bounds the manual workload; fixed smoke
refuses conflicting overrides. Explicit voxel changes never happen automatically.
Fresh evidence directories are acquired atomically; existing evidence is refused.

`--inventory-only` performs one real cold Document publication and first draw,
independent whole geometry/scalar/affine-query/three-mode frame checks, a real
unmeasured warm-up stroke and exact Undo, then a second independent checkpoint.
It stops before sustained loops. Production admission requires at least250000
actual published visible triangles; candidates1×.0125,4×.025 and16×.05 remain
unadmitted. Existing sample/cache/query/working limits remain unchanged. A worker
error is observed promptly by bounded polling, rather than waiting the entire
120s timeout after a genuine quota/validation refusal.

GL identity is recorded before authentication. Software requires actual Mesa
llvmpipe/softpipe. Arc requires actual current GL/EGL device identity, the canonical
PCI03 render node, matching live process FD and sysfs8086:56a0/i915 evidence; UHD
or software cannot satisfy this branch. Initial/final context maps and device
witnesses are recorded. The coordinated Arc lock is
`/tmp/dome-sculpt-arc-device.lock`; source work acquires no lease or GPU context.

Accepted-publication latency and first-draw submission time are now separate from
the combined operation window. Wall/thread CPU intervals end before constructing
JSON counters. Source-owner traversal, checkpoint readbacks and raw-row writes
stay outside measured intervals with their own costs. Each completed sample is
flushed to NDJSON before another sample starts, preserving partial evidence after
a later interruption. Final reports add each row's flush cost; the streamed row
cannot contain the duration of its own flush. Requested fixed view/held inputs
are also retained.

The declared overview now contains all fixture objects, including the reflected
instance and both original grouped spheres. No per-frame mesh baking is added.
GPU elapsed queries cover the two eye draws, while CPU pair intervals also include
actual controller queries. A64-slot asynchronous pool records missing indices,
bounded drain, reset/disjoint support and observations. GPU distributions require
at least95% valid coverage without reset/disjoint/pending results. The source's
natural process disables C++allocation counting but retains explicit factory and
transfer instrumentation; it does not claim a wholly uninstrumented application.

Actual argument/runtime/device behavior still requires the bounded whole current
consumer gate. Production and Arc work remain prohibited until coordinator go
and actual size/oracle admission. No speedup, production latency or physical
headset pacing follows from this source checkpoint or earlier zero-upload checks.
