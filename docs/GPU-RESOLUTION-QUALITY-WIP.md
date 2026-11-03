# Historical reverse reconstruction quality prototype

The initial prototype below is retained as history. The implemented bounded
production tier and completed evidence are in [GPU-RESOLUTION-QUALITY.md](GPU-RESOLUTION-QUALITY.md); its ledger records all remaining hardware scope.


This private child of4fbc72d adds an actual aggregate sparse-field API, not yet
production Volume eligibility. Existing forward interpolation and surface/
topology/normal certification remain unchanged. Full goal remains active.

`ErrorQuery` is24bytes: signed lattice point, reserved0, expected binary64 word.
The shared validated integer-binary64 sampler computes the reconstruction and
`abs(sample-expected)` at native-double precision. Finite nonnegative binary64
magnitude words are ordered as unsigned64. GPU workgroups128 reduce local
maxima; a second workgroup reduces partials, with only8bytes read back per
bounded65536-query chunk. No nativeFP64 or atomic64 is required. Callers stage
raw coordinates/expected values, never CPU interpolated per-query results.
Cancellation/failure drains outstanding work before scalar readback storage
unwinds and permanently refuses a failed GPU session; no CPU fallback.

The CPU packed reference retains native-double arithmetic with contraction
disabled. Independent actual paged-map interpolation verifies it. Prototype
fixtures cover nonfirst workgroups/chunk tails, exact oneULP and subnormal
errors, invalid/admission/cancel/retry paths, and detached source immutability.
CPU exact tests pass; OneAPI compiles. First standalone link missed the actual
paged storage support TU and one invalid-coordinate fixture accidentally used
a legal coordinate at2:1; their failed logs remain. Neither changed a tolerance.
Actual Arc/SAN/vendor compile and integrated Volume evidence are pending.

Initial CPU benchmarks already demonstrate why eligibility must be measured:
refinement373248 target-grid entries/110592 original points takes15.7ms via
native paged lookup, versus49.7ms including packed-CPU setup. Coarsening and
small cases also remain measured separately. Current prototype performs no
production path selection. Packing, session upload, query/reduction/readback
and warm native/packed-CPU timings must be compared before choosing a deliberate
CPU quality tier or GPU eligibility; there will be no runtime-error fallback.
