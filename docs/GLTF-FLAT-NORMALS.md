# Source-preserving glTF flat-normal import

The native GLB/glTF authoring importer now derives editable flat-normal geometry
when the source omits NORMAL. Original bytes, accessor identities, supplied
tangents, images, colors, UVs and deformation data remain immutable. The current
editor/export can contain generated attributes without rewriting that original.
This is an extension of the existing FLTK/OpenXR editor and shared importer.

The [glTF2 specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html)
requires flat normals when NORMAL is absent, including each morph target, and
requires ignoring supplied tangents and their displacements on this path.
The implementation generates directions for individual keys, then uses the
existing morph and skin transport; it does not invent a different interpolation
rule by recomputing the blended pose's normals.

## Implementation

- A bounded, cancellable derived mesh splits ordered corners and computes face
  normals with double-precision edge differences/cross products. No area epsilon
  deletes valid small geometry. Degenerate/nonfinite keys refuse explicitly.
- Missing-normal sources ignore the importer's smoothed display fallback. A
  normal texture generates original-basis Mikk base/target frames from the
  correct key positions and flat normals. Without a normal texture, supplied
  source tangents remain ignored and the retained native frame is genuinely N-only.
- Native split vertices retain exact original vertex and ordered corner IDs.
  POSITION morphs, all joint influences, authored/implicit binds, COLOR_0 and UV
  authority follow those mappings. Existing FixedAttributes frame persistence
  records the result; no public NormalFrames layout or scene schema is changed.
- Native and spatial import use the same guarded background asset transaction,
  raw-source capture, edit authority and single Undo/Redo operation.
- The new four-argument atlas API propagates import cancellation into actual
  xatlas progress callbacks. The old three-argument ABI and numerical output stay
  unchanged. Callback exceptions unwind vendor tasks before propagation. Other
  workers still need adoption, and a vendor iteration can run between callbacks;
  this is cooperative cancellation rather than a hard latency guarantee.

## Qualification

Exact sources, binaries, runtime, logs and scope are recorded in
validation/GLTF-FLAT-NORMALS.json. The full preceding holding/import qualification
remains separate in HOLDING-IMPORT-INTEGRATION.md.

The geometry/import/atlas proofs below qualify source ec537a8. The subsequent
296f86d followup changes the bitmap helper/palette/toolkit and pkg-config ordering;
all those geometry sources and public data layouts remain exact. Its current
DomeD eee23b7 runtime qualifies 138 native GUI assertions, Wayland/universal-menu8,
native PCF glyph/1x2x/GL tests on software, Arc, OneAPI and sanitizers, and all four
existing real-loader synthetic XR suites on CPU and OneAPI. These component
scopes are explicit in the receipt; no new all-suite sanitizer claim is made.
The later bdb0a47 refresh copies the canonical alias helper at DomeD 3024fb6;
FLTK/runtime inputs are unchanged. Four current PCF/native-font variants verify
exact exported media identity, hidden generated host choices, glyphs/pixels/1x2x
and GL. The rebuilt 3024 compositor also passes native Wayland/menu8. The receipt
keeps this small alias followup separate from the earlier138 GUI/four-XR proof.

The independent graphics fixture parses the original source's positions, UVs and
POSITION key, derives expected face normals and triangle tangent derivatives,
and checks the actual emitted accessors. Its own cgltf/PNG/PBR/normal-map/morph/
skin consumer compares exported pixels against the real Document worker/render
snapshot. It covers both authored and implicit binds with mixed influences,
three alpha modes, three morph weights and native save/reopen. Removing the normal
texture must change visible pixels; alpha coverage must be nonempty.

All 324 graphical assertions pass on software GL and Intel Arc A770 with maximum
pixel error0. All 36 emitted GLBs pass pinned Khronos2.0.0-dev.3.10 validation with
zero errors and warnings. Native GLB/glTF chooser/options/Save/Undo/Redo flows
pass 25 assertions, including exact retained original bytes and generated frame
authority. The 39-check native controller suite now navigates the real Import21
menu to import a missing-normal source on the asset worker and verifies its
generated frames and exact single-operation Undo/Redo on CPU, OneAPI and sanitizers.
The low-level derived-frame helper survives35 actually injected
persistent C++ allocation failures and18 cancellation boundaries with exact
source owners and deterministic retry; this is not whole-import JSON OOM proof
or C-malloc fault injection.

CPU and OneAPI each pass seven affected suites. Fresh whole-own C/C++ core/render,
xatlas, Mikk and ufbx ASan/UBSan/leak instrumentation passes the same seven.
Eight additional CPU source/VRoid/frame/UV regressions pass. Separate actual Arc
OneAPI backend and CSG oracles pass without CPU substitution. The atlas suite's
13 checks include genuine chart/pack/output cancellation, exception/lifetime/
concurrency behavior and actual missing-normal/missing-UV import cancellation
from a real vendor chart callback. The existing17 atlas robustness checks pass.

## Required remaining work

PaintSourceTexture still explicitly accepts only UV0 sampling. A UV1 normal-map
source therefore refuses before native authoring; the test preserves this guard
and exact caller state. Full UV1 sampling, painting and durable provenance remain
required. This checkpoint also does not complete affine/source-pose or native FBX
skin/blend/material import, broader glTF/VRM extensions, COLOR/UV morphs or native
shapekey authoring. Whole-import JSON allocation safety is separate active work.

This is scoped native/controller/graphics evidence. No new full real-loader
missing-normal authoring scenario or physical headset/controller proof is implied.
No system installation or active-session restart occurs. Every unfinished row in
REQUIREMENTS.md stays required and the complete production goal remains active.
