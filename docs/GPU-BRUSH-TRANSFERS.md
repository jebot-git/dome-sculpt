# Atomic CUDA/HIP brush transfers

This private component changes the actual CUDA/HIP brush host transaction and
its GPU output layout. The shared `brush_sample` arithmetic and public
`Sample`/`Brush` layout remain unchanged. No CPU fallback is introduced.

Previously `apply_brush` uploaded all Sample records, updated those device
records in place, then copied the full records directly into caller memory.
A partial failed readback could therefore publish part of a gesture before
throwing. Unchanged positions, averages and protection were also copied back.

The production CUDA/HIP adapter now invokes `gpu_brush::apply`, the common
private host-control-flow helper. Device capacity is at most 2,000,000 samples:
48,000,000 input bytes and 8,000,000 scalar-output bytes. The kernel reads const
Sample records and writes only distances. Each chunk uploads 24 bytes per
sample and reads back 4, matching the existing SYCL traffic contract. All host
results are detached until every chunk succeeds. The final scalar publication
cannot allocate or throw and never rewrites metadata. On a transfer/launch/wait
error, best-effort vendor synchronization precedes buffer and result unwinding.
Partially returned allocation pointers are released even if allocation reports
an error. No queue/device cache is added.

The always-available `gpu-brush-transfer-shims` test executes that exact
production host helper through explicitly labelled CUDA/HIP API shims. Its
synthetic kernel writes a sentinel, not GPU brush arithmetic. It tests actual
2M boundaries and three-chunk transactions, 24/4 byte counts, immutable NaN and
negative-zero metadata, brush argument forwarding, failures in both allocation
steps, upload/launch/wait, partial readback in chunks 1/2/3, deferred work before
cleanup, exact original exception type, same-input retry and host-staging
allocation refusal before any vendor call. These are host-control-flow proofs,
not NVIDIA/AMD numerical or device-loss proofs.

`gpu-brush-missing-device` builds the entire current backend GPU translation
unit and a minimal real-runtime probe, with no CPU backend linked. It requires
an actual missing/hidden vendor device, checks its SDK status and explicit
runtime refusal for 1, 257 and 2,000,007 samples, with every host byte unchanged.
The empty brush remains a no-op. CUDA/HIP positive hardware is absent here and
remains required independently.

Final optimized shim qualification passes130 checks in18.72s; the entire
production host helper plus shim fixture passes130 atO1 with ASan/UBSan and
leak detection in20.18s, with no report. These are not whole-core instrumented
or GPU numerical results. The final test also requires the exact failing chunk,
so a first-chunk error cannot substitute for the later-chunk proof. Earlier
R9.01s/SAN13.79s results remain separately preserved.

Both actual vendor configurations compile and link the entire current backend
module and minimal real-runtime probe. CUDA reports status35/insufficient driver;
HIP status100/no device. Each registered suite passes for empty plus1/257/
2,000,007 samples with the full source bytes unchanged. The probes link no CPU
backend and accept only explicit vendor availability errors.

Fresh optimized full native FLTK/OpenXR/core/support applications also compile
and link for CUDA13.4.59 compute86/sm86 and HIP/gfx1030, both with explicit
`CMAKE_POSITION_INDEPENDENT_CODE=ON`. Root owns the separate ROCm native PIC
**default** fix and honoring explicit user overrides; this component does not
claim that repair. All sources/tests stayed frozen during the SDK builds. Only
the test's exact chunk assertion changed after both native builds were terminal;
its final R/SAN gates were then rerun. Shared brush arithmetic/public headers
remain byte-identical tofb75ea5. Native dependency logs use the staged canonical
toolkit/bitmap runtime. The inherited unused SoftFloat label warning remains
visible and unsuppressed.

The sealing-time source/binary/log hashes and exact scopes are in
`validation/GPU-BRUSH-TRANSFERS.json`. No before/after binary manifest was captured
for these SDK runs. Positive NVIDIA/AMD brush numerics, real vendor device-loss/
partial-readback behaviour, recovery and sustained performance remain required.
Reduced byte traffic is not a measured throughput claim; the extra scalar
allocation may cost more for tiny jobs. No hardware session, installation,
physical HMD or desktop restart occurred. The full goal remains active.
