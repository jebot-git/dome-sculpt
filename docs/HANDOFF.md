# Verified source-aware bake / rig integration

The full native FLTK VR production objective remains active and incomplete.
Read AGENTS.md, REQUIREMENTS.md and TOOLKIT.md. Preserve every remaining sculpt,
UV, texturing, rig/skin/morph, avatar/MToon, glTF/VRM, plugin, device and compute
requirement. No agents were spawned in this workspace. Remote handoff references
to agents describe another workspace; do not treat them as live handles here.

## Integrated history

Current work/bake-source-integration joins checkpoint b879ce4 and origin/main
c95208b. Both histories and their full handoffs/validation are retained under
HANDOFF-{b879ce4,c95208b}.md and VALIDATION-{b879ce4,c95208b}.md. Historical evidence
in those files is not fresh verification of this combined source. The public
checkpoint/baked-creator-handoff-20261029 remains an unchanged recovery point.

Incoming work preserves direct original GPU source bindings/cache, shared grouped
GLB rigs, original joint/morph/coordinate provenance, native desktop/controller
pose and shapekey controls, fitted groom and detached weight-paint kernels.
The weight helper is now registered in the native core and CTest; it still needs
interactive weight painting/visualization and guarded scene publication. Source
CC0 VRoid female/male remain the primary creator bases; MakeHuman is explicit.

## New bake/source integration

- Scene-v6 baked persistence and legacy standalone baked-v5 compatibility remain
  alongside modern grouped-v5 source archives. Unbaked scenes still emit v5.
- Original GPU preview cannot bypass a baked base. Clear restores raw eligibility;
  scene Undo restores the baked path and immutable source/bake snapshots.
- Canonical baked normals use Mikk worker tangent corners; original normal views
  retain sampler/affine derivative GPU handling. Both use bounded shared GPU
  image/sampler storage. Corner-expanded ranges retain their vertex offsets.
- The CPU baker now accepts original source-only bindings and retained raw normal
  views. Default original normals use affine-UV derivative frames (mirrors and
  normal strength included); explicit supplied tangent frames remain authoritative.
  Native object baking no longer rejects original normal views. Source models,
  images, editable target geometry/rig/layers and source object remain intact.
- Protected/fractional normal coverage converts original source detail into the
  canonical baked basis before blending. Baked normal export overrides conflicting
  original views, and composite AO exports from ORM red without reapplying original
  strength. Source-only export preserves independent original bindings/strengths.
- Tests add analytic rotated/mirrored/singular normal views, raw normal/AO/alpha/
  factor transfer, explicit frames, protected detail, exact source retention,
  raw/baked/clear/Undo preview selection, GPU source-vs-bake shading comparison,
  and independently decoded mixed-source GLB normal bytes/bindings.

All 40 registered Release suites have passing evidence across a 39/40 broad
run and final nine-/two-suite corrections; do not describe it as a single final
40/40 run. Seven selected whole-core C/C++ ASan/UBSan/leak suites pass. Final
baked textures have 38 assertions; source GPU 42; source preview 13. Native general
menus pass 53/53, rig/shapekeys 8/8 and existing baked-v6 GUI compatibility 5/5.
The real-loader synthetic XR report passes all 53 checks. See VALIDATION.md for
exact scopes, timings, fixes, logs and remaining verification. Inspect actual
live processes/handles before restarting tests. No install, active-session restart or physical-device validation
has occurred. Runtime is C++20/FLTK/EGL/OpenXR/BDF; Python remains test-only.

## Next work

1. Fetch remote main before overlapping work and preserve any newer commits.
   The b879ce4 checkpoint remains unchanged; this stage incorporates c95208b.
2. Connect guarded Document::bake_from_object to actual FLTK and XR menus: explicit
   source picker, world-mm distance/AO/sample/padding sliders and fine wheels,
   progress/cancel, preview/rebake/clear and cage authoring. Preserve universal
   menu/modal ownership, Cancel-default Enter, Ctrl+W/Escape and one exact Undo.
3. Continue full in-headset rig/weights/morphs, production atlas/conformal/stitching,
   retained source maps during UV changes, layered texturing and multi-source
   baking. Current CPU raw sampling is bilinear at texel centers; mip footprints,
   full authored-tangent/UV1/vertex-color fidelity and production performance need
   further work. Static GPU fixtures do not establish full deformed interchange.
4. Integrate complete MToon/VRM and every remaining requirement. Other remotely
   reported UV/MToon/VRM work is not implementation evidence until committed and
   inspected. Do not shrink the goal to this integration milestone.

Preserve immutable render inputs, worker-only geometry/file/bake work, source and
revision/generation guards, exact cancellation/Undo, ordered saving and original
asset archives. Keep the Mikk C seed-rotation fix, hashes, patch and notices.
