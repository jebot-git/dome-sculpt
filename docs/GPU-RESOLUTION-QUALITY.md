# Bounded reverse reconstruction quality

Private runtime `ac920ed`, aggregate API `b045ee8`, reproducible host test
`c871c0f`; base `4fbc72d`. The complete Dome Sculpt goal remains active.
There are no public header, Volume layout, archive, or frontend control changes.

Resolution now offloads independent raw-layer and protection reconstruction
error on sizeable coarsening: selected backend is not CPU, new spacing is at
least 1.25 times the old spacing, at least 32,768 original samples exist, and the
reconstructed sparse map has no more entries than the original. Small fields,
refinement, and other ineligible fields deliberately retain native paged-map
quality evaluation. A backend error never changes the chosen path or retries
on CPU. Surface/topology/distance/normal certification remains unchanged on CPU.

Each 24-byte query carries an integer source lattice point and its **raw expected
binary64 value**. No CPU interpolation results are staged for GPU queries. The
existing integer binary64 interpolation computes reconstruction and subtraction;
nonnegative finite error words have the same ordering as unsigned64. Two
128-lane reduction stages return one 8-byte maximum per bounded 65,536-query
chunk, without native FP64 or atomic64. CUDA, HIP and SYCL use the same math.
Expected values, opaque parameters, coordinates and returned scalar words are
validated. Admission rejection leaves a session reusable; dispatch-phase failure
or cancellation drains and poisons the session.

Host packing and hash probes are charged to the existing work budget, as are
eight interpolation visits per original sample. Entry/query loops, validation,
chunk boundaries and publication retain cancellation checks. Thresholds are
compared at native double precision before the existing float report is used.
Mask float quantization/default, all independent layers and their properties,
physical distance band and detached publication semantics are retained. The
same-spacing path remains allocation-free.

## Complete-cost measurements

Twelve cases cover coarsening/refinement, layer double/mask float, and small,
medium and larger fields. Every GPU maximum exactly matches independently
computed native paged-map and packed-native-CPU results; repeated warm queries
also check the exact word. These are single-machine process measurements under
uncontrolled concurrent load, not end-to-end application throughput claims.
The complete logs retain packing, hash construction, session/upload,
query/reduction/readback, and repeated timings separately.

| Field / original samples | Mode | GCC native map | Arc total including packing/upload |
| --- | --- | ---: | ---: |
| 64 / 216 | Coarsen, layer | 0.045 ms | 4.254 ms, first cold reduction |
| 64 / 216 | Coarsen, mask | 0.051 ms | 0.251 ms |
| 13,824 / 46,656 | Coarsen, layer/mask | 5.47 / 5.41 ms | 2.83 / 2.57 ms |
| 110,592 / 373,248 | Coarsen, layer/mask | 45.42 / 45.62 ms | 17.67 / 11.18 ms |
| 373,248 / 110,592 | Refine, layer/mask | 15.20 / 14.76 ms | 30.36 / 38.34 ms |

The OneAPI binary also measures its own native map path, which was slower than
GCC in these runs. Both raw logs are retained; the table uses the faster GCC
baseline. Packed CPU setup for the large refinement cases costs roughly
47–51 ms including interpolation, so it is not substituted for the native CPU
tier. The policy does not claim a speed benefit for every data distribution or
unmeasured NVIDIA/AMD device.

## Finished evidence

Artifacts are under `artifacts/gpu-resolution-quality-4fbc72d/artifacts/`.
The machine-readable ledger is `validation/GPU-RESOLUTION-QUALITY.json`.

- Actual PCI03 Intel Arc A770, native FP64 unavailable: 94 aggregate checks and
  12 complete-cost cases. Independent paged-map references, one-ULP/subnormal
  errors, distinct workgroups/chunk tails, empty/invalid inputs, post-chunk
  cancellation, failed-session refusal and fresh retry pass.
- Actual Unified Runtime trace: 257 queries with capacity127 upload
  3048/3048/72 bytes; scalar readbacks are 8/8/8 bytes; six kernels. The sparse
  field upload is48 bytes. No per-query result readback occurs.
- Fresh CPU registered seven-suite scope: 7/7 in4.27s. Whole owned C/C++ core and
  support libraries, O1 ASan/UBSan/leaks: 7/7 in38.39s, no reports. The final
  strengthened fixture separately passes R1/1 .67s and SAN1/1 7.14s, including
  an eligible visible closed Volume with topology/distance/normal certification.
- Actual Arc native integration: 23 checks in1.27s. Exact independent layer/mask
  maxima and bounds, work exhaustion, deliberate CPU tiers, unchanged source,
  stable nested paint/image/other-geometry owners, one Undo/Redo, accepted
  Save/restart, cancellation/stale generation, failed dependent Save/prior-file
  preservation, retry and final shutdown drain pass. Five unchanged existing
  resolution/aggregate suites passed in the preceding eight-second Arc run.
- Reproducible **test-only host-control-flow seam** forces aggregate staging with
  the native CPU sampler. All22 Volume/worker checks pass R1/1 1.01s and wholly
  instrumented core/support SAN1/1 9.19s. This covers aggregate packing and
  transaction branches under sanitizers; it is not GPU/SDK/numerical evidence.
- Actual CUDA13.4.59/sm86 and HIP7.1/gfx1030 complete backend component
  compile/link pass. Fresh entire core/support/Document model integration
  compile/link also passes both. Actual missing-session and complete Volume
  probes fail explicitly and preserve the original source; no CPU substitution.
  The frontend is disabled for these scoped builds. NVIDIA/AMD positive
  numerical, performance, device-loss and hardware tests remain unavailable.

The original standalone link omission, incorrect invalid-coordinate fixture,
new-target missing include directory, and Arc wrapper-identity assertion are
retained as failed logs. The last failure was in the fixture: `copy_scene()`
intentionally mints snapshot wrappers. Its corrected check authenticates stable
nested geometry/image owners and exact paint data. There was no runtime fix,
tolerance reduction, or dropped source assertion. Both Arc runs have819 input
source/binary hashes unchanged before/after. The earlier prototype has its own
separate exact hash and transfer audit.

The branch predates the separately published atomic CUDA/HIP brush-transfer
change. Its diff does not alter brush code; integration must preserve that newer
main implementation. This checkpoint does not complete every accelerator or
resolution workload, change the normal/surface quality policy, or establish
whole-project latest-main qualification.
