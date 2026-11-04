# Source-aware mesh baking and colored cuts

This is a private implementation milestone. The original Dome Sculpt production
objective remains active and incomplete. This branch is not installed or admitted
to main; the frozen full regression remains unchanged.

Raw source captures can contain an explicit empty texture-role bundle. Such a
capture is an untextured material, and can independently own writable paint.
The previous bake validator rejected it before colored source baking and cuts
could perform their real work. The generic bake worker now accepts that explicit
bundle, samples each original texture from its actual coordinate domain, and
composes immutable sparse color, metallic, roughness, occlusion and legacy
emission contributions at the projection hit. Vertex tint is applied exactly
once, following the native renderer. It does not create a dense source composite
or read pixels back from the GPU. Cut authentication also checks paint packet
identity. Physical radiance lacking this byte-bake adapter is explicitly refused,
preserving the source.

Only `src/bake.cpp` and `src/cut-color.cpp` change product behavior relative to
qualified atlas baseline8de. No public layout, archive version or schema changes.
Complete matching product TUs replace their own members in the f89 paint/document
component archive; remaining support, renderer and frontend inputs come from the
completed, identical c092 cohort. This is component qualification, not a fresh
whole-application build or full-regression pass.

## Closed evidence

- The genuine Source4 fixture passes85 checks in Release, owned ASan/UBSan and
  strict Intel Arc A770 execution. Its24 image/export cases cover empty or
  textured source bundles, UV1, real sparse paint, hidden layers, fractional
  opacity, vertex tint, lit/unlit shading and opaque/mask/blend alpha. Compared
  pixels differ by at most1 encoded level against the unchanged bound2.
- Each baked GLB is independently parsed and rendered. All24 corresponding
  Release/sanitizer/Arc files are byte-identical. All72 files pass Khronos with
  zero errors and72 retained generated-tangent-space warnings.
- Equal-valued foreign source owners refuse. Actual cancellation preserves the
  original paint. The same corrected fixture linked to the old core fails at
  the original empty-role rejection, establishing a runtime counterfactual.
- Existing Release workflows pass49 source Bake checks, all158 direct cut
  admission allocation failures and exact retries,157 native/controller cut
  checks,72 plane-cut checks,133 source Bake render checks,185 simultaneous
  source/target checks and70 joint-avatar authoring checks. Corrected boundary
  and legacy Bake fixtures also pass Release and owned sanitizers.
- The CMake graph declares the new test in590 registered commands. Executed
  complete-TU compile flags match the generated declaration except output path;
  link flags match after explicit matching archive, object and output path
  substitutions. This is not a590-test pass.
- The relinked native application passes21 actual OpenXR-loader checks under
  synthetic tracking: accept, unconfirmed cancellation and holding, balanced
  stereo resources, exact retained source and native save. This is Release
  software graphics, not a physical headset test.

## Preserved failures and ongoing work

The first new renderer fixture omitted required native tangent corners; the
actual tangent kernel now supplies them. An existing cut boundary test corrupted
byte zero of a now-empty dense color vector. Its Release crash and sanitizer
trace are preserved. The corrected test forges the actual immutable role-bundle
owner instead, retaining the intended refusal check. Both test-only corrections
pass in Release and owned sanitizer builds.

The sanitizer standalone-XR attempt still fails on the known112-byte Mesa report,
with two allocations originating in shader rendering. It has not been suppressed,
ignored or counted as a clean exit. The complete Release858 packed and4397 precommit allocation-failure sweeps now
pass, with exact source/scene/owner preservation and successful full-helper retry
at every measured prefix (185.29 seconds,2227 unchanged inputs). The wider
selected sanitizer cohort and matching sanitizer fault sweep retain their
original finite handles in the receipt. Sanitizer packed858 has finished, while
precommit remains running; snapshots are not final whole-sweep pass claims.
Inspect current status before continuing those handles.

Generic Source4 target baking, general MToon and physical-source baking, broader
normal/filter/minification fidelity and coherent integration with published
atomic IO and final27/poll FLTK remain required. All original and later
requirements, including controller drawers after the basic targets, remain in
scope. No system files, desktop session or physical headset changed.

Complete command/input/output receipts and preserved negatives are recorded in
`validation/SOURCE-AWARE-MESH-BAKE.json`.
