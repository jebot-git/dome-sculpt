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

Initial optimized shim qualification passes 130 checks in 9.01 s. Current
real SDK compile/link/missing-device and instrumented shim gates are pending;
record their exact scopes and command/artifact hashes before offering this
component for integration. Root owns the separate ROCm native PIC default fix;
this component does not claim that build-system repair or an arbitrary full
application/backend qualification.
