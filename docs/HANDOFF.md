# Dome Sculpt production objective remains active

Read AGENTS.md, REQUIREMENTS.md, HANDOFF-466ebd1.md and the parent
DESIGN_CONVENTIONS. Complete the whole native C++20 FLTK/OpenXR sculpt, texture,
UV, rig/skin/morph and avatar suite; animation and final offline rendering are
excluded. Do not mark the goal complete at a build or checkpoint.

## Combined Retopo and authoring integration

The publication tree integrates remote main3d19776 (fdb132b/251cd3d Retopo)
with the fully verified authoring source6df3cf2 and metadata500f321. All histories
are preserved. Skin stays on page14, Masks reserves15, Retopo uses16. The schema
writer uses Scene8 for immutable materials, dormant deformation or Retopo; both
complete legacy Scene7 feature shapes remain readable. Partial feature records,
invalid UTF-8, cyclic references and aggregate resource overruns reject.

Retopo runs on its own worker with the same predicate-mutex shutdown handshake
as the other four workers. Native Place/Move commands are idempotent. XR New,
Resume and Mode close the menu to expose pressure input. Cancel covers both the
pre-asset drain and cancellable asset phase; standalone shutdown discards a
remaining unconfirmed draft before final save. Preparing another mesh cannot
overwrite retained editable topology. Missing references report a useful error.

The immutable overlay cache retains local topology in GPU buffers. Canvas,
object and stereo transformations are GPU uniforms; changing view does not
rebuild or upload topology. Existing material visibility, picking, PBO composition
and weight previews are retained. Generated/duplicated Unicode names preserve
UTF-8 code points. The project-info payload digest treats absent and null optional
Scene8 keys identically; it still hashes all nonnull source payload data.

Use fresh build-retopo headers/libraries, never pre-Retopo archives. Its67 native
Release suites pass121.84s. Full real-loader syntheticXR117/117 passes (including
all original96 and21 Retopo checks). Whole-core C/C++ ASan/UBSan/leaks14/14
pass102.12s. Retopo GPU fixtures pass on actual Arc and llvmpipe. Combined native
GUI/universal-menu/DomeD verification is in progress; inspect live jobs/logs before
repeating. No physical-device claim is made for this newer source.

Canonical DomeD/FLTK main was rechecked at282467e727df78c8e7f2452aa05a7747bac854a6.
Use Release toolkit22patches prefix artifacts/toolkit-282467e/lib64 explicitly;
inherited /opt libraries may be older. Matching standalone compositor is under
artifacts/domed-282467e-target/release. No active session restart or system
installation occurred. Build as regular user; sudo only for system files.

## Physical test boundary

The user confirmed rudimentary operation of exact8f4d67d on the real WiVRn HMD
and both controllers, then requested ending it. Only the test process stopped;
recovery snapshots and physical-test-report.json are retained under
artifacts/xr-milestone-8f4d67d. DomeD remained running. This proves basic operation,
not complete workflow/stylus/reconnect/performance validation or later sources.
Subsequent SIGINT/SIGTERM handling is verified with the real-loader synthetic
runtime: completed edits save, unfinished gestures cancel, and XR resources exit.

## Verified checkpoint and next integration

The authoritative combined application checkpoint is source `6df3cf2`, built in
artifacts/publish-final, branch work/production-integrated. All64 Release suites
pass208.57s, including complete real-loader syntheticXR155.07s. Final native
menus53/53, Skin bind/transfer/history11/11, Unicode input/clipboard/history3/3
and actual latest DomeD Wayland/universal-menu8/8 pass with zero failures.

Whole-core C/C++ ASan/UBSan/leak coverage:15 authoring/export suites pass165.53s;
then the final shutdown/file/recovery/GLB/atlas6-suite pass91.27s covers the latest
worker and atlas corrections. Atlas17 focused checks also pass in Release and
whole-core sanitizer overrides, including exact faces and actual low-resolution
bake capacity; the unchanged bake fixture's39 assertions pass.

All16 generated avatar variants pass the15 hash-checked pinned official VRM0
schemas and Khronos with zero errors; each retains20 legacy/tangent warnings.
Actual Arc material/picking25/25 and Unicode palette rendering also pass.
The complete ONEAPI frontend built from this source passes96/96 syntheticXR
with actual Arc brush compute; software EGL isolates loader/input behavior.
Native Arc compute5suites7.10s plus shared-document6.38s are separate evidence.

Earlier full integration failures remain in logs:60suite58/60 exposed UV slivers
and recovery epoch cleanup;63suite61/63 exposed avoidable power-of-two band
capacity refusal and an intermittent shutdown hang. Both have real fixes.
The deterministic shutdown fixture forces the old lost-wakeup schedule and
fails against old code; all worker notifications now handshake predicate locks.
No timeout or strict geometry/paint assertion was weakened. See VALIDATION.md.

This earlier checkpoint remains separately published at
checkpoint/authoring-6df3cf2 with metadata500f321; see HANDOFF-500f321.md.
Remote3d19776's independent pre-merge proof is archived separately. Neither its
proof nor the64-suite checkpoint substitutes for combined verification above.

## Live unfinished work

The primary main tree may contain later agent work not in the publication build.
Query collaboration ownership and git state before merging or staging. Preserve
all such work. The old lazyUV/sourceUV/skin stash is an applied backup, not work
to apply again. Obsolete prototype trees are not authoritative applications.

- Production masks/symmetry: durable sparse COW masks, constrained brush arithmetic
 and single-dispatch symmetry are being developed privately with new Sample,
 Brush and Volume layouts. Desktop/XR authoring, flatVolume3 decoding, full scene
 quota accounting and matching CPU/SYCL/HIP/CUDA evidence must follow. Do not
 link this new header layout with the older published core.
- Original/native frames: private corner-layout, explicit-matrix skin and Shape
 Jacobian helpers exist; runtime/archive/export integration remains unfinished.
 Creator Shape evaluates base/target keys individually before linear morphs.
 Preserve current editable skin/pose while validating relevant original rest and
 morph identities. No public sourceUV Apply guard has been removed.

Live sourceUV user editing stays guarded: v7 UvDerivative pixels cannot be
relabelled original-frame data. Future Scene v9/materials v2 records need explicit
provenance, original/native frame identities, current skin/morph evaluation and
validated re-bake from immutable origins while preserving current edits.

## Full remaining scope

Every incomplete REQUIREMENTS row remains required: production sculpt
masks/symmetry/resolution/local meshing and sustained VR; layered projection,
stencils/texture sets and production baking; complete retopo/cut/procedural/plugin
workflows; interactive rig/landmarks/fingers/automatic weights and editable morphs;
robust anime/toon humanoid authoring; full selected glTF/VRM1/constraints/expressions
and independent graphical consumers; physical headset/controllers/stylus/3D mouse,
session loss/reconnect, performance, and all eligible compute workloads.

CUDA toolchain/hardware remains unavailable; HIP7.1 compiles gfx1030 but no AMD
GPU/ROCK device is present. Compile-only or software graphics evidence cannot
stand in for accelerator execution. CPU remains the default backend. See
COMPUTE-VALIDATION.md and the exact reports in VALIDATION.md.
