# Sustained sculpt pipeline measurement plan

This is an unexecuted plan. Existing native/GL checks and upload counters are
correctness evidence, not production performance or a frame-time benchmark.
The local-coordinate grouped regression must pass before this gate starts.
Request a bounded artifact budget and Arc device lease from the coordinator
before compilation or execution; do not touch qualified cohorts or live sessions.

Use the actual native Document worker and SurfaceRenderer, with the same
optimized compiler, current canonical DomeD SDK, source files, resource limits,
resolution, materials, viewport and stroke samples for each compared run. Record
both backend selection and actual render device. CPU brush processing with Arc
OpenGL is a distinct case from OneAPI brush sampling; neither proves GPU stroke
mutation or fully accelerated meshing. Do not silently substitute a CPU device.

## Fixtures and correctness

Produce deterministic, genuine narrow-band sculpt objects, starting from sphere
radius0.6 metres and voxel sizes0.025,0.0125,0.00625 metres. Record actual stored
samples, vertices, triangles, chunks, scalar bytes and ownership envelopes before
naming a tier. These are candidate sizes, not assumed counts. Require a separately
identified production tier with at least250000 visible triangles, and keep its
actual admitted size in the report. A resource refusal is retained as a refusal;
never silently lower density and label the resulting run production-size.

Include a fixed active object with1,4 and16 total objects, choosing the extra
objects' real declared density so the aggregate respects the existing quotas.
Include original grouped placements, one nonuniform/sheared/reflected case,
visible/hidden objects, and opaque/Blend/18%-ghost views. Keep the original grouped
sphere regression unchanged. Imported/painted source39/40 and rare authenticated
exact-repair routes are separate domains, requiring their own admitted fixtures.

Before timing, and after each sequence, compare actual ordered geometry/normals
and source identity against an independent whole-mesher reference. At checkpoints
verify scalar values against the real Volume sampler and compare actual frames.
Keep Cancel, stale-generation refusal, Undo/Redo and ordered Save/reopen checks
as mandatory correctness gates. A fast run with wrong retained fields is failed.
Do not materialize chunks on every frame merely to operate an observer.

## Workloads

- Run at least300 accepted local brush capsules, alternating actual Clay/Carve
  samples at a known surface. Separate single-stroke latency from batches of
  genuine coalesced input; retain the complete fixed input sequence.
- Run120 mask strokes, then explicit global Fill/Clear and visibility toggles.
  Distinguish scalar packet work from geometry/BVH and GPU geometry uploads.
- Render at least2000 stereo view pairs with unchanged geometry, controller-held
  motion and canvas motion. Follow with authored group/affine placement edits.
  Verify actual immutable local geometry and BVH reuse rather than treating a
  changed placed wrapper as necessarily changed geometry.
- Run one long sequence combining strokes, masks, view changes and accepted
  history boundaries, followed by exact Cancel/Undo/Redo/Save/reopen verification.

Report initial cold preparation and warm sustained sequences separately. Use at
least three measured repetitions after a declared warm-up. Record concurrent
CPU/GPU load and any process affinity. Pin only benchmark processes to appropriate
CPUs if needed; do not change the running desktop or other applications' affinity.

## Measurements and comparison

Record end-to-end input-to-accepted-snapshot latency, CPU thread time, actual
worker wall time, extraction/assembly/query/scalar preparation where separately
measurable, rendered-frame CPU time, and GPU elapsed time through asynchronous
GL timer queries. Do not force a per-frame glFinish or readback to obtain timings.
Collect diagnostic pixels only outside the measured interval or at explicitly
excluded checkpoints. Record observer overhead and disjoint/invalid timer data.

Report median,p90,p95,p99,maximum and throughput alongside actual workload sizes.
Budget misses for72/90Hz may be calculated, but uncapped offscreen throughput is
not a physical headset frame-rate result. Record geometry/scalar upload bytes,
actual changed/reused owners and CPU directory/vertex work to explain latency;
these counters alone cannot establish speed.

Use a separately built, source-pinned pre-chunk application or a declared whole
producer baseline for the same input files. Never link old MeshSnapshot/Document
objects into the new layout. If an old baseline fails a real fixture, keep that
failure; do not alter the fixture merely to manufacture a timing comparison.
Record independent whole extraction/reference costs separately from application
latency, and account for texture/material/normal work rather than timing only
one kernel. A partial baseline permits only its stated comparison.

For hardware, require actual GL renderer/version plus live DRM render-node FDs,
sysfs PCI/vendor/device/driver association, and sampled runtime mappings. Arc
A770/renderD129 is distinct from UHD/renderD128; environment variables alone are
not device evidence. CPU-reference and real Arc GL cases precede any claimed
OneAPI comparison. HIP/CUDA positive hardware qualification remains separate.

## Required result

Deliver raw fixed inputs, source/compiler/SDK/binary hashes, resource/device
receipts, actual timing distributions, work/transfer statistics, retained failures
and exact final correctness checks. State remaining whole-directory metadata,
scalar resampling, fallback and source-domain costs plainly. No speedup, bounded
work, production latency or full GPU completion is claimed until these actual
measurements and their matching correctness scope pass.
