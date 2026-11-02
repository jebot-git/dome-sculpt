# Sparse GPU interpolation prototype

Frozen source `5ed4057` in `artifacts/sparse-resample-392c8dc` advances the
integer-binary64 precision prototype into actual sparse hash lookup and trilinear
interpolation. This remains a component: it is not wired into the published
resolution worker and does not complete the accelerator requirement.

The immutable source hash admits at most four million finite entries, bounded
128-probe lookup, exact native voxel spacings and bounded scaled coordinates.
Device operations use the pinned SoftFloat integer implementation; the actual
A770 lacks native FP64. Query chunks contain coordinates only, with eight-byte
result readback. The measured prototype uploads the entire field in EVERY
iteration; it does not claim a persistent device cache. No CPU per-query
interpolation precedes the device kernel.

Release host1/1 and whole-header/test ASan+UBSan1/1 pass4,380 exact interpolation
words plus malformed-input guards. OneAPI2/2 passes host and actual A770 tests.
Every result from every one of four GPU iterations is compared with an
independent native-double unordered-map formula:889,116 results periteration
across small, empty, negative/distant-coordinate, background and dense cases.
The separate precision prototype's broader arithmetic evidence remains distinct.

For the dense262,144-entry/884,736-query case, native unordered-map CPU time is
92.87ms and an optimized native packed-hash CPU comparison is77.22ms. Source
packing costs5.97ms and query validation1.11ms. GPU field/query upload, kernel
and readback take6.94–8.49ms; packing/validation-inclusive time is14.02–15.57ms.
Queue+allocation for this already-warmed dense case adds0.38ms. The first small
case instead records48.18ms queue+allocation and3.26ms first transaction: these
initialization costs must not be hidden in a whole-job speed claim. Tiny cases
are slower on the GPU than the CPU.

This is a measured dense interpolation benefit, not a complete resolution-job
speedup. Support enumeration, output insertion, reverse reconstruction, topology,
surface quality, cancellation/history and Save are outside this benchmark.
The next child must prove these actual worker invariants and end-to-end cost,
then CUDA/HIP device compilation and real hardware where available.

The final source was frozen during these builds/tests. The receipt hashes the
currently preserved source/logs/binaries at sealing time; this run did NOT capture
separate before/after manifests, and no such claim is made. No application build,
physical headset launch, installation or active-session restart is implied.
