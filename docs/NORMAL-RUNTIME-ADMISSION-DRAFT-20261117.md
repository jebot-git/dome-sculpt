# Normal-source runtime admission draft

This branch merges the tested normal-source production body1bc13125 and public
test bodyf1e193c7 into current main while preserving both histories. Product,
test, asset and build files matchf1 exactly. It is a reviewable integration
draft; main admission remains conditional on the outstanding strict sanitizer
owning/runtime gates. The full production objective remains incomplete.

Actual current evidence:

- [Whole optimized Release build](NATIVE90-RELEASE-OWNING-20261117.md):
  frozen090f source,1036 complete owners (291 freshly compiled/745 independently
  proved whole objects),737 fresh links,3 generators,1776 outputs, zero errors/drift.
- [Whole Release native90](NATIVE90-RELEASE-ACTUAL-20261117.md), version,
  workspace27/373 and raw-source90159 pass.
- [Whole application public workflows](WHOLE090F-PUBLIC-UI64-XR260-20261117.md):
  actual fresh whole090f App, fixturef1 and runtime/font/scenarios6c8; desktop64
  and authenticated Arc syntheticXR260 pass with exact normal/source/history
  checks. No whole-f1 build or physical headset claim.
- [Native Wayland/DomeD Arc](WHOLE090F-WAYLAND-GPU-ACTUAL-20261117.md):14 actual
  controls, viewport, GPU context and ordinary-close checks pass. Nested
  diagnostic presentation includes a copy; native DRM scanout is unverified here.

Strict SANv2 remains the same live frozen090f build; its actual terminal status
and subsequent native90/product evidence must be recorded before admission.
The newer advanced Rasterize/RetainSource adapter is isolated in another branch
and is not included in this draft. Unsupported families and every unfinished
original requirement remain required. No system installation, physical headset
launch or active session restart is part of this draft.
