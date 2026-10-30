# Integrated native authoring milestone

The full production objective in REQUIREMENTS.md and HANDOFF-466ebd1.md remains
active. Additional user requirements are in ADDITIONAL-REQUIREMENTS-2026-10-30.md.
This extends the existing native C++20 FLTK/OpenXR application.

## Integrated behavior and proof

This candidate combines source-preserving procedural sculpt/bake/paint/UV authoring,
generic Document transactions, static source colors, conservative paged picking,
resolved VRM roles, VRoid-only humanoid creation, Scene11 authored glTF binds and
original source receipts, and the native static FBX/OBJ capture adapter. See
PRODUCTION-INTEGRATION.md and validation/PRODUCTION-INTEGRATION.json for exact
sources, binaries, logs and limits. The earlier workflow handoff is preserved in
HANDOFF-bc9cfc1.md; the full original requirements remain authoritative.

Optimized CPU native146/146 and actual Arc OneAPI native147/147 suites pass.
Whole C/C++ core/support sanitizer37/37, actual FLTK51 checks, eight direct Arc
hardware graphics fixtures and all four real-loader synthetic OpenXR suites on
both builds pass. Synthetic XR is150 original +17 resolution +11 procedural +11
projection checks; eye rendering there uses software GL and compute uses the
selected backend. No newer physical-headset result is implied: only the old8f4d67d
rudimentary milestone was physically confirmed and the user ended that test.

The default meshes/creator choices now contain only verified CC0 VRoid female/male.
MakeHuman compiled defaults/converter are removed; existing authored legacy scenes
retain load/export. Broad FBX/OBJ options are not completed by the static adapter.
Large-mm normalization, full affine/skin/morph/material cases and unsupported glTF
features remain required. General colored-source baking is still incomplete.

CPU allocation fault tests retain persistent denial. GPU tests use actual one-shot
faults and typed memory errors while preserving every exact state assertion; two
cold owned staging sites are separately covered. Permanent heap blackout crashes
inside the installed Intel Level Zero driver and remains an explicit external
limitation. See PAGED-VOLUME-ALLOCATION-VALIDATION.md; no runtime or driver fix is
implied by the corrected fixture.

## Active agents and next work

- merge_contracts: full Wings3D-style polygon authoring; stable-ID N-gon kernel and
  immutable workspace are frozen private checkpoints. Full Scene12/Document/native
  and XR workflow is in progress. Actual polygon point rendering still needs its
  Arc discrepancy fixed. Shared source-format receipts and native N-gon provenance
  must remain; do not substitute the existing retopology patch workflow.
- merge_palette: mesh visibility, physical holding, ghost rendering and exclusive
  edit authority. Native/controller, simultaneous held paint/sculpt/weights and
  GPU traffic checks are qualified privately; final whole-core sanitizer and
  checkpoint are pending. After holding, resume the parked general colored-source
  bake work with its retained hashes/backups, not an unqualified prototype merge.
- merge_docs: shared native/XR Import21/options23 controls; final native unit/format
  choices and large millimetre-coordinate normalization are being qualified.
  Full affine/source poses, native-format skin/morphs and normative missing-normal
  glTF adoption remain required after that checkpoint.

Reserved spatial pages:18 procedural,19 polygon,20 holding,21 import,22 polygon
secondary,23 import options. Preserve all command capabilities and unify admission
bounds through page23. Agent branches must merge into a fresh integration and
rebuild every public-header ABI consumer before qualification/publication.

## Workspace and toolkit

Canonical DomeD remote/source remains282467e727df78c8e7f2452aa05a7747bac854a6.
Use the prepared FLTK1.4.5+22 prefix artifacts/toolkit-282467e, whose libraries are
in lib64; verify actual linkage. Build Release as the regular user. Follow TOOLKIT.md
for preparing/staging the matching runtime before a system installation. No system
installation or session restart occurred for this milestone. Do not automatically
launch a physical-headset test after the user ended it.

The primary worktree's old frame WIP remains safely preserved as stashc46d1d907a9ca0cdb56cda17e54ab20284939fc1
and byte-verified artifacts; do not drop/reapply it blindly. Preserve all frozen
agent branches, backups and local commits. Milestone commits/pushes are authorized.
The complete goal stays active until every requirement has actual implementation
and appropriate evidence; this passing integration is not completion.
