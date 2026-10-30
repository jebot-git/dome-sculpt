# Dome Sculpt production objective remains active

Read AGENTS.md, REQUIREMENTS.md, HANDOFF-466ebd1.md and the parent
DESIGN_CONVENTIONS. Complete the whole native C++20 FLTK/OpenXR sculpt, texture,
UV, rig/skin/morph and avatar suite; animation and final offline rendering are
excluded. Do not mark the goal complete at a build or checkpoint.

## Current integration

Application source `6df3cf2` combines both authoring histories, remote main3aa47e5
(including33f26a2 live immutable weight preview/material-visible picking), native
skin bind/transfer, v7 immutable material provenance/sourceUV kernels, painted
VRM0, lazy UV initialization, graceful standalone signals, Arc SYCL, Unicode
palette and ordered native project Open/Save. All local and remote commits are
retained; do not force-push/reset. Historical handoffs/validation remain archived.

The scale-robust mesher retains tiny valid closing faces. Full integration found
xatlas then collapsed some of their UVs. The new bounded atlas repair preserves
exact geometry/corner order and requested chart padding, promotes only affected
faces to padded independent charts, and refuses insufficient capacity atomically.
Strict painting and watertightness assertions are unchanged; see ATLAS-ROBUSTNESS.md.

The private worker argument order is operation, expected, report, work, changed,
rollback-preview. Skin passes an empty work pointer before its changed token;
weight gestures supply rollback-preview last. Never mix enum/header layouts or
pre-change archives from the earlier independent integration.

Canonical DomeD main was rechecked at282467e727df78c8e7f2452aa05a7747bac854a6.
The Release toolkit22patches and compositor match that source. Use explicit
artifacts/toolkit-282467e/lib64 at runtime; inherited /opt toolkit paths may be
older. Build/install as regular user and request sudo only for system files.
No active session/display-manager restart or system installation occurred here.

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

Remote main subsequently added native desktop/XR retopology in3d19776
(fdb132b/251cd3d). That newer source is NOT part of the64-suite checkpoint.
Integrate it next while preserving both histories: Skin already owns page14,
Masks reserves15, so incoming Retopo must move to16. The divergent Scene v7
retopo and material archives need explicit combined versioning/compatibility.
Rebuild all header consumers, include the new worker in safe lifecycle handling,
and verify the final combined source before claiming publication complete.

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
relabelled original-frame data. Future Scene v8/materials v2 records need explicit
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
