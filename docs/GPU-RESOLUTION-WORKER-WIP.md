# Selected-backend resolution worker checkpoint

Private frozen sourcec0fa331 in artifacts/resample-worker-5ed4057 implements
actual forward interpolation for independent sculpt layers and sparse masks
through the selected backend. CPU builds retain the existing unordered-map
native-double interpolation path. GPU builds pack each immutable field once,
upload it once per field session, and batch at most65,536 query coordinates;
lookup and trilinear arithmetic execute on the device with eight-byte result
readback. Output remains detached until all original reconstruction, topology,
surface/normal quality and source-generation checks permit publication.
Backward reconstruction and mesh quality remain CPU work; this is not complete
GPU resolution or a claim that all eligible algorithms are accelerated.

Fresh coherent Release CPU and OneAPI production-core/support builds finish.
CPU6/6 (3.54s) and actual A770 OneAPI5/5 (5.70s) pass: resolution quality/data,
Document Undo/Redo/Save/restart/stale-source/ordered cancellation, controller
commands, progress and actual backend session. The CPU-only allocation fixture
retains its exact allocation-free no-op and checks91 transactional sampled
boundaries through126,652 measured allocations; these are sampled boundaries,
not exhaustive allocation coverage. No current GPU allocation-fault claim.
The new session compares14,739 results over three calls against an independent
native map formula, with chunking, admission, late cancellation, failed-session
refusal and fresh retry. Final checks count14,748CPU/14,749OneAPI.

An independently CPU-generated native layered/masked input is loaded by BOTH
builds. Actual output matches exactly1,152,784 serialized bytes, SHA256
82c9547e97f0cc57a756d43343c3211ea8b2533434b86b633606ff53cfb1235b.
The test's link wrapper proves actual production dispatch, injects a later
backend-output failure and checks exact retained owner/data without partial
publication. Valid output has19,220 layer samples and1,301 mask exceptions.
Missing OneAPI selector refuses explicitly. All1,142 source/binary hashes match
before registered tests and after the separate Volume proof.

This small whole-resolution case measures77.11msCPU versus213.28msOneAPI,
including first-use queue/kernel overhead and unchanged quality work, while the
user's VRChat/build activity is present. It is not a controlled sustained-speed
result and does not establish whole-job acceleration. The dense interpolation
component's separate improvement must not be substituted for this measurement.

Fresh whole-owned C/C++core/support ASan+UBSan/leaks5/5 passes in35.83s,
including all five corresponding registered worker/controller/data/session
suites. All1,144 captured source/binary hashes remain unchanged. This does not
instrument a frontend or a GPU driver. Initial CUDA backend
compile fails on dynamically indexed host reciprocal tables. Portable child
2d17cb1 fixes that with independent CUDA/HIP compilation and exact host/device
numeric proof; it has not yet been combined with this worker qualification.
This UI/XR-disabled checkpoint does not qualify a fresh FLTK/OpenXR app build,
new physical headset workflow or main runtime admission. Full production,
remaining accelerators and post-basic controller drawers stay required.
