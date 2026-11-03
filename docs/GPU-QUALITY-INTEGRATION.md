# Reverse reconstruction GPU integration

Runtime merge `772213d` combines qualified component `1be8ab6` (runtime
`ac920ed`, aggregate API `b045ee8`) with main `ede6686`. Existing retopology
bridge/extrusion, current ROCm PIC defaults and newer atomic CUDA/HIP brush
transfers are preserved. Only CMake had a merge conflict; both sets of target
definitions are retained. Component quality code is unchanged; the CUDA/HIP
backend also retains main's existing scalar brush kernel and BrushApi helper.
No public layout, Scene or Volume format changes are introduced.

Read GPU-RESOLUTION-QUALITY.md for the arithmetic, bounded transfers, complete-cost
measurements and deliberate CPU tiers. Eligible large coarsening reduces raw
layer/mask reconstruction error on the selected GPU and returns one 8-byte
maximum per chunk. Small/refinement jobs retain the native paged CPU quality
path. GPU errors remain explicit failures. Surface, topology and normal
certification still run on CPU. Component timings do not establish whole-job or
sustained VR throughput.

Fresh optimized CPU and OneAPI complete native FLTK/OpenXR applications compile.
Selected CPU 12/12 (5.84 s), actual Arc OneAPI 9/9 (6.02 s), and fresh owned C/C++
core/support ASan/UBSan/leak suites 12/12 (67.79 s) pass. The sanitizer build has
UI/XR disabled; its aggregate host seam exercises real packing and transactions
with the native sampler, separately from actual Arc numerical execution.
Native FLTK universal-menu 15 plus compact BDF 4 checks pass with private Xvfb
and software GL. Actual merged CUDA/HIP backend quality and brush modules also
compile/link; registered brush missing-device 1/1 and direct quality missing
probes pass for each. These are not new complete vendor frontend builds or
positive AMD/NVIDIA hardware tests. The component's separate model-build and
hardware receipts retain their own exact scope.

The committed `tests/resolution-quality-xr.py` additionally drives actual
application controls through the synthetic OpenXR loader while rendering both
real GL eyes on the Arc A770. Its native CPU-only oracle constructs a closed
37,193-sample layer, independently coarsens 10 mm to 20 mm (4,697 samples), and
saves the expected scene. Accepted application output matches every serialized
object field exactly; Cancel preserves the entire original scene. Actual
Unified Runtime trace captures both aggregate kernel creations and scalar
8-byte readback inside that rendering application. All ten checks pass. This
is synthetic input with real graphics/compute, not a physical-headset test.

Build `dome-sculpt-resolution-quality-fixture` in the CPU build and the native
application/runtime plus resolution-document inspector in the OneAPI build.
With the recorded OneAPI runtime library path, `ONEAPI_DEVICE_SELECTOR=level_zero:0`
and `DRI_PRIME=pci-0000_03_00_0`, the reproducible command is:

```sh
python3 tests/resolution-quality-xr.py \
  build-oneapi/dome-sculpt build-oneapi/libdome-sculpt-test-runtime.so \
  build/dome-sculpt-resolution-quality-fixture \
  build/dome-sculpt-resolution-document-test artifacts/quality-xr-repro
```

All 1,166 tracked inputs remained unchanged through the initial merged gates;
24 CPU, 21 OneAPI, 19 sanitizer and four vendor binaries retain their exact
captured hashes. Subsequent metadata and standalone-oracle target/source
additions leave every existing application/library/test binary unchanged. The
committed oracle/script and its binary have their own before/after manifest.
The initial standalone helper compile error and unconfigured new-target failure
are retained; neither required a runtime or tolerance change. Exact logs,
commands/scopes and hashes are in validation/GPU-QUALITY-INTEGRATION.json.

No system installation, active-session restart or physical test occurred. The
complete production requirements, remaining eligible GPU workloads, unavailable
vendor hardware proof and post-basic spatial drawers remain required.
