# Calibrated filtered-normal forecast — 2026-11-18

This is a CPU forecast milestone for production `e006869d` and graphics fixture
`c56a55ae`. Software OpenGL, Arc, public application and physical headset
qualification remain pending. It does not admit this component into the combined
application or complete the production objective.

## Actual result

All 17 families and 68 source/target PBR/MToon views pass at explicit target
resolution 2048, with source resolution 64 unchanged. All 37,968 independently
decoded dense points satisfy the unchanged normal bound `.035`; the maximum is
`.0119707896`. Every original probe remains, giving 272 original points. All 16
immutable prior PBR control triplets calibrate within 1 RGB unit. All 104 prior
actual source controls, including 56 MToon controls, calibrate within 2 under the
original bound 3. Presentation forecasts do not replace actual GL comparisons.

The previously interrupted forecast's entire 19,034,211-byte output is an exact
prefix of this completed run. Its SHA is recorded in the receipt. Normal
functions, material data, UV samples, family/point order, output fields, quality
bounds and previous negative controls remain unchanged.

The witness selector retains all original probes and adds at most three
independent contrast witnesses per view. The immutable manifest contains 432
points and expects 1,796 PNGs in each future graphical matrix. All three
wrong-domain/omitted/swapped-role contrasts retain gap 7 and disjoint intervals.
The selector's manifest kind now identifies the actual 2048 consumer; a stale
64-resolution kind was corrected before any graphical launch.

## Build and runtime provenance

The old dense helper copied all 2048 material planes through `prepared_point`
for each UV probe. Its material packet is independent of that probe's UV,
normal and geometry; only the subsequent explicit sampling uses the current
UV. The fixture now constructs that immutable packet once per view, then
samples every original UV independently. Production code is unchanged.

One complete graphics fixture TU compiles freshly in 16.644s and a fresh link
completes in .318s. The actual compiler MD binds 475 dependencies; all 71 actual
linker inputs are audited. Four qualified renderer owners and the authenticated
193-member core retain their original hashes and histories. No reused object
is called newly compiled. Actual ELF class/machine/DT_NEEDED/RUNPATH match the
previous product. Exact source/product hashes are in the receipt.

An initial read-only link audit included linker-generated phony make targets.
Its failure is retained. Reading only the primary rule corrects the observer;
no compiler command, object, product or numerical predicate changed.

The product completes in 237.257s under unchanged 900s/AS2GiB/RSS3GiB bounds.
The regular-user unit retains MemoryMax4GiB/RuntimeMax1200s. The existing task-local
two-arena/128KiB mmap allocator profile stays explicit. Unit
`ds-advanced-normal-forecast-cache-forecast-v1-20261118.service`, Invocation
`27a631e38d734b98aae420e6920eb364`, has its terminal/journal captured before stop.
Drift and survivors are empty. The common lease is independently NB-free at
`1794981453.474329`, device40/inode99618, then handed to the queued creator.
No installation, session restart or idle lease reservation occurred.

The receipt and scoped archive in `validation/advanced-normal-forecast-20261118/`
include the complete forecast JSONL, exact plans/drivers, actual dependency
files, logs, terminal captures, witness manifest and preserved interruption
records. No ELF/object payloads are included. The original interruption remains
an interruption, not a numerical failure or acceptance result.
