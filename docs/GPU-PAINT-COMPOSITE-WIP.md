# Plain writable-paint compositing checkpoint

This private component implements a selected-backend compositor in actual native
paint/mesh/Bake workers. It is based on ede6686 plus published30991dd; runtime
fc4d87f is frozen in artifacts/gpu-workload-ede6686. The test/receipt child is
artifacts/gpu-paint-allocation-fc4d87f. Main7693d6b/Source25 and the private
Source24/26 union have not been combined with this component.

The optimized regular-user cost audit measured direct CPU 2048² plain composites
at 641–697ms and 1024² at 159–173ms. Complete packed CPU costs were 14.3–14.8ms at 512,
57.1–57.4ms at 1024 and232.8–267.3ms at 2048. Arc process-first 512 at 179.8ms includes runtime/
JIT; later 512 at 8ms,1024 at 33–47ms and 2048 at 150–153ms include packing, upload, execution,
readback, image construction and cache work. These uncontrolled observations are
not guaranteed VR frame times or universal speedups.

The route handles all 14 byte channels, visible layers/opacity, masks and exact
padding. Immutable sparse tiles/padding are packed privately. Selected kernels
produce 16-byte output records in 65536-pixel chunks; all readbacks finish before
any caller image/cache is changed. Errors never retry on CPU. Cancellation
reaches Scene/native/Bake work, including late staging; old 1-arg preview and
composite ABI remain. There are no class-field or archive changes. Separate
backend TUs preserve published brush/quality source and flags exactly.

Eligibility is 512..2048 with contributing tiles, at most 128M pixel-layer visits,
and no source maps, bake, color/normal projection or independent PBR tile owner.
Existing unsupported owners deliberately retain their CPU route with no new
refusal. IMPORTANT FUTURE UNION GATE: this header baseline has no Source24
paint_sampling_domain/attachment. Any non-null independent sampling/normal
metadata owner must retain CPU eligibility at semantic union until its actual
packet representation is implemented; do not infer compatibility from absent
source_maps/baked or equal geometry counts. Source24/26 are not qualified here.

Shared blend math retains float expression boundaries. Native unit/sRGB decode
LUTs and exact float-to-sRGB-byte thresholds avoid device pow approximations.
Independent immutable input is essential: initial GCC/Clang stamp generation
differed BEFORE compositing (one 512 tile byte, three 1024 tile bytes, each by 1).
Those original failures/logs remain. No tolerance or stamp arithmetic changed.
The exact same serialized Paint input reproduces all 10 legacy CPU maps
(26,214,400bytes) byte-for-byte on CPU and Arc, twice.

Finished evidence:

- CPU primitive 756623 / actual PCI03 Intel Arc A770 primitive 756624 checks;
  sparse/redirected padding, multiple layers/every channel, invalid packets,
  cancellation/retry and chunk tails. Original 743 hashes unchanged.
- Actual cache/native 33 on CPU-host control flow and A770: exact CPU maps,
  reuse, completed-backend/late validation errors, original cache/held images,
  early/middle/late cancel/retry, real posed skin+morph paint, ordered Save,
  oneUndo/Redo, native cancel/stale generation.733 Arc hashes unchanged.
- CPU 7/7 regression in 5.65s; whole-owned C/C++ O1 ASan/UBSan/leaks 7/7 in 39.94s
  (CPU backend plus labeled host aggregate seam, not SDK/device instrumentation).
- Measured 27 persistent HOST allocation prefixes, Release and matching fully
  owned sanitizer archives: actual injection, exact paint/mesh owner/cache/held
  pixels, successful same-source retry. No foreign SDK heap-denial claim.
- CUDA and HIP complete core/new module+component compile; actual missing-device
  probes pass. No NVIDIA/AMD positive numerical/performance/device-loss proof.
- Fresh coherent optimized OneAPI core/render/native FLTK/OpenXR application.
  Independent cgltf/PNG/GLSL consumer 85 checks each softwareGL+Arccompute and
  actualArcGL+compute:12 current/emitted cases at 512/1024, lit/unlit,
  Opaque/Mask/Blend; all native-vs-GLB full-image errors 0, exact legacy maps,
  source and Save/restart equality.12 emitted GLBs: Khronos 0 errors/0 warnings.
- Actual native real-loader tracked paint at 512/controller palette/stereo/save 7
  checks: both eyes A770, balanced images, exact nonpaint source. URtrace proves
  production paint kernels and nine authenticated 1MiB device-to-host chunks;
  equally sized padding uploads excluded. Synthetic tracking, no physical HMD.
- Actual FLTK 512 posed paint: accepted-endpoint/queuedSave/Close 3 and
  EscapeCancel/Save/Close 3, deterministic worker gate and real client status/
  DBus acknowledgements. Accepted app trace confirms selected paint kernel.
  SoftwareGL with actualA770compute. All 806 tracked source/core/render/frontend/
  test hashes unchanged through consumer and GUI jobs.

Reproduction uses tests/paint-composite-compute.cpp, native.cpp, allocation.cpp,
render.cpp, fixture.cpp and paint-composite-menu.py / paint-composite-xr.py.
The menu fixture reuses existing native-strokes-menu.py client acknowledgements;
new optional helper/output overrides leave its default behavior unchanged.
The small XR script requires APP RUNTIME SOURCE INSPECTOR OUTPUT arguments,
ONEAPI_DEVICE_SELECTOR=level_zero:0 and DRI_PRIME=pci-0000_03_00_0. Generate SOURCE
with the fixture's --xr-fixture; choose LIBGL_ALWAYS_SOFTWARE explicitly. Existing
native-gui-gate is preload-only, not a production hook.

See docs/validation/GPU-PAINT-COMPOSITE.json for exact paths/hashes/scopes.
Main integration/rebuild remains parent-owned. This is plain derived image
compositing, not GPU stroke mutation, complex source/baked compositing, GPU
baking/local meshing, Source24/26 integration or completion of the full goal.
