# Compute backend selection and current evidence

CPU, ONEAPI, ROCM and CUDA are explicit CMake choices. ONEAPI configuration
compiles and links a real SYCL queue/kernel, rather than merely testing whether
the compiler accepts `-fsycl`. A requested GPU backend never silently selects
the CPU implementation when the SDK or device is unavailable.

## Intel Arc execution

Actual Intel Arc A770 execution was verified using the official
[DPC++7.2.0 release](https://github.com/intel/llvm/releases/tag/v7.2.0), compiler
commit5680988bb38bd45f13081394fba43588c5f5e395 and the installed LevelZero driver.
The untouched Linux archive SHA256 is
385586a25e75ac727883f0be5e442dec0ee81b3c20f9a29037a79921ad2f1728.
The compiler is user-owned under artifacts/oneapi/7.2.0, not a system install
or a vendored product dependency.

On this host the release's bundled libumf lacks a direct libm dependency and
device enumeration initially crashed with an IFUNC modff relink diagnostic.
The original archive/libraries remain unchanged. A separate local compatibility
copy in artifacts/oneapi/compat has `patchelf --add-needed libm.so.6` applied.
Explicit LD_LIBRARY_PATH selects that copy for these compiler/runtime tests.
Both original and modified library hashes and download provenance are recorded
in artifacts/oneapi/provenance.json. Do not silently patch another user's SDK
or infer this host workaround is required by every installation.

The isolated candidate starts at6947fb2 in artifacts/verify-oneapi-6947 and adds
only the SYCL backend/test and mesh numerical fix. CMake builds the production
core with ONEAPI and only the kernel translation unit needs `-fsycl`; executables
retain the required SYCL link flag. Five Arc-backed native suites pass8.66s:
shared-document, sculpt-geometry, compute-backend, compute-missing-gpu and
mesh-scale (tests-oneapi-mesh-fix.log). This is actual selected-backend execution,
not an EGL renderer test or a compile-only accelerator claim.

The portable compute fixture covers four independent analytic brush endpoints,
128 batches/254,336 samples compared with a separately compiled CPU reference,
all four tools, pressure, capsule bounds and workgroup-edge sizes. Maximum
observed distance error is2.98023e-08. It also verifies a2,000,007-sample operation,
four concurrent authoring threads and unchanged non-distance input attributes.
Forcing a CPU-only device filter makes the GPU selector throw while preserving
the original host samples; empty input succeeds without creating a GPU queue.

SYCL now reuses one in-order queue and bounded USM storage per authoring thread.
Device capacity is at most2,000,000 samples; larger API calls use ordered chunks.
Only output distances are copied back, reducing payload from40 to24 bytes per
sample. Host samples publish after every chunk succeeds, and failed queues are
discarded instead of being reused or replaced with CPU work. UR tracing of the
fixture records166 actual kernel launches,6 queues (one independent device query
plus five authoring threads),20 capacity-growth allocations and332 transfers.
Logs/counts: tests-oneapi-resource-trace.log and oneapi-resource-counts.json.
Single-run timings are diagnostic, not production performance certification.

## Integrated native frontend and XR execution

The complete Release FLTK/OpenXR application, including Unicode palette and
native asset controls, also builds with ONEAPI in artifacts/publish-final/build-oneapi.
Its production compute path runs on the Arc A770 using ONEAPI_DEVICE_SELECTOR=level_zero:0;
there is no CPU substitution. Five native compute/geometry/controller suites
pass7.10s (tests-production-oneapi-initial.log), and shared-document separately
passes6.38s (tests-production-oneapi-document.log).

The complete real-loader synthetic OpenXR suite passes96/96 with that native
ONEAPI executable, including accepted/focus-cancelled skin binding, baking, UV
work, controller/stylus inputs, ordered worker files and graceful SIGINT/SIGTERM
save/cleanup. Log: tests-production-oneapi-final-worker-safe.log (source `6df3cf2`); report complete=true,
failed=0. Its initial partial run stopped because two fixture helpers had not
been built; that is retained as a failed harness attempt, not a passing suite.
The corrected run built both helpers with the same ONEAPI core and headers.

These checks use software EGL graphics to isolate loader/input behavior while
actual brush compute uses the Arc. Independent material/picking25/25 and Unicode
palette GPU/layout tests also pass on actual Arc OpenGL. They do not establish
sustained physical-device performance or every eligible accelerated workload.
The final96check run includes the bounded atlas repair and worker-shutdown
handshake from source `6df3cf2`. Earlier native numeric/resource counts retain
their stated source scopes; future Sample-layout changes require new evidence.

## Scale-robust extraction

The first actual ONEAPI core run exposed132 open edges around almost-zero sphere
samples. The mesher discarded geometrically valid small closing faces using a
fixed square-metre area cutoff. The shared mesh validator/extractor now computes
the cross product in double precision and rejects collinear/coincident faces
without an absolute world-size floor. It neither rounds stored field distances
nor relaxes the existing watertightness assertions. The unchanged GPU geometry
suite now passes. Eighteen independent regressions cover triangle scale,
collinearity, perturbed positive/negative/zero lattice samples, outward winding,
unchanged fields and cancellation down to10-micrometre voxels. The existing
38-check CPU core suite and the new scale fixture also pass with the changed
mesher linked to the frozen CPU core (tests-mesh-scale-core.log /
tests-mesh-scale-cpu.log).

## Scope still requiring work

This proves the production brush path and affected native model workflows on
the Arc. Sparse sample gathering/layer merging and most geometry, paint, bake
and rig algorithms still run on the host; this is not a claim that every
eligible workload has been moved to the GPU. Sustained physical VR performance,
device-loss recovery and broader accelerator workloads remain requirements.

HIP7.1 compiles the existing kernel for explicit gfx1030, but the current host
has no AMD GPU/ROCK device. CUDA hardware/toolchain execution is unverified.
Compile-only HIP evidence and unavailable CUDA hardware do not complete those
backend requirements. See artifacts/accelerator-inventory.log and
hip-compile-inventory.log. CPU configuration remains the default.
