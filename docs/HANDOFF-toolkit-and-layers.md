# Agent handoff — current toolkit and UV persistence evidence

Previous checkpoint and complete background: [HANDOFF-466ebd1.md](HANDOFF-466ebd1.md).
The full production objective in REQUIREMENTS.md remains active and incomplete.
Do not treat these repairs/tests as completion of the sculpt/avatar suite.

## Current follow-up

Cloned main at466ebd1 into /home/blux/de-xr/dome-sculpt. Read AGENTS.md,
REQUIREMENTS.md, the workspace DESIGN_CONVENTIONS and current DomeD README.
Fetched DomeD/fltk-vr; used latest committed DomeD742d009 and fltk-vr4f54821.
See TOOLKIT.md and third_party/sources.json for exact provenance/reproduction.

- Fixed a build portability failure: public Fl_File_Chooser.H declares
  fl_file_chooser; the checkpoint referenced an absent FLTK header.
- Built a new Release FLTK1.4.5 from canonical DomeD's22 current patches,
  including window-suspension, touch and pen. Older fltk-vr collection's builder
  references an absent patch; do not use that older builder for this task.
- CMake's build RPATH now follows the configured FLTK imported target.
  build/run-dome-sculpt and the OpenXR CTest prepend that actual runtime directory
  so inherited /opt library paths cannot silently select an older toolkit.
- XR harness retains synthetic project files on failures, writes a running/failed
  report instead of leaving stale successes, persists uv-comparison.json and
  reports UV/field/paint assertions separately. All original conditions remain.
- Added controller regressions for logarithmic continuous UV scaling, fine-wheel
  rate, rotation with retained scale, and exact focus-cancel/reopen coordinates.

## Initial evidence and subsequent ordered-input correction

The prior handoff's local logs were not committed, so they were unavailable in
this fresh clone. Its reported UV persistence failure has NOT reproduced here.
Do not claim a proven root cause or weaken its assertions.

Fresh runs pass with changed UV hash5162942432843560021 versus baseline
3423923137026321418, unchanged field hash11207114470605020999 and unchanged
paint hash3979263710525981571. Five additional native OpenXR replays from the
exact saved painting baseline all produced these same hashes and passed.
Artifacts/xr-uv-repeat.log and build/xr-client/uv-comparison.json retain evidence.

Current Release native suites:10/10 passed, including the new controller UV
checks and real-loader synthetic OpenXR36 checks. Native GUI:18/18 passed.
Universal-menu audit:25/25 passed. Logs: artifacts/tests-current-toolkit.log,
artifacts/gui-latest.log, artifacts/menu-latest.log. All used the new configured
FLTK libraries. No physical headset/controller/stylus or compute accelerator
was tested. The prior sanitizer result is historical; no new sanitizer run is
claimed for this follow-up.

Latest committed DomeD was separately built Release from an isolated clean
worktree at artifacts/domed-source using the new toolkit and cached Rust
build dependencies. This did not modify unrelated capture source changes or
restart the running compositor. Nested Wayland/universal-menu8/8 checks passed with that freshly built
compositor and toolkit (artifacts/dome-menu-current.log). The initial private
run failed because the harness hard-coded a nonexistent lvp_icd.json; it now
selects the architecture-specific software ICD too and supports the canonical
DOME_TEST_VULKAN_ICD override. No production renderer fallback was changed.

## Next work

1. If the historical UV failure recurs, inspect the retained projects and separate
   hashes before changing semantics. Check controller sampling/arming, mode
   selection, validity, Apply and worker commit timing. Current code has no
   proven UV persistence defect from that old failure.
2. Continue production work from REQUIREMENTS.md: durable sculpt layers/recovery,
   local meshing, remaining UV conformal/stitch workflows, importer/remeshing,
   layered painting/baking, rigs/weights/morphs/MToon/glTF/VRM and the named native
   plugin-equivalent workflows. Do not substitute a new reduced objective.
3. Verify physical devices and accelerator toolchains before making hardware
   claims. Use the current canonical toolkit and build/run wrapper.

No system installation or active user-session restart was performed. Generated
builds, toolkit sources/libraries and captured project evidence remain ignored.


## Active production goal and sculpt-layer milestone

The user explicitly activated the complete original requirement matrix and
requested delegation plus validated commit/push milestones. The goal remains
active; no partial library/build/test milestone completes it.

Native source volumes now have persistent sparse displacement layers, names,
strength, visibility, locks and stable IDs. Desktop Sculpt/Layers commands,
properties Apply/Cancel, universal-menu states and XR Object/Sculpt layers
commands share document transactions. A native XR fixture creates and sculpts a
new layer and verifies persisted authoring data. See SCULPT-LAYERS.md.
Volume v2 preserves layered data and rejects silent downgrade; old v1 volumes
load into Base. COW field/displacement maps keep snapshots immutable and avoid
whole-map copies until mutation. Field construction validates imported samples.

Validation:11 Release CTests pass (artifacts/tests-sculpt-layers.log),39 native
synthetic XR assertions,32 actual FLTK universal-menu checks. Eight ASan/UBSan/
leak-enabled core suites pass (artifacts/tests-layer-sanitize.log); final
storage-budget/canonical-sample changes also pass focused sanitizer field/layer
checks (artifacts/tests-layer-sanitize-final.log). No physical-device or
accelerator claims. The current toolkit and running desktop remain unchanged.

Parallel agents have committed native rig/weights/morph and recovery kernels
with focused direct tests; these are not yet complete scene/UI workflows.
Root owns central integration and sculpt controls. Mesh import/processing and
baking work is in progress independently. Do not overwrite those owned files.
Next integrate recovery into runtime/source generation, saved-revision cleanup,
worker discovery/restore and desktop/XR commands, then import/remesh and rigging
workflows. Keep the complete REQUIREMENTS.md scope, including all plugin
counterparts and glTF/VRM, active throughout.


## Remote input-order milestone merged during production work

Remote d8e3699 was published while local layers/recovery/rig foundations were
being developed. Both histories are preserved. Its archived environment/evidence
is in HANDOFF-d8e3699.md. It reproduced the old UV confirmation loss with a
burst: focus handling discarded earlier Apply/release events. The merged fix
orders focus cancellation after prior transitions, drains accepted input before
shutdown, rejects late input, and reopens input at each native OpenXR READY.
The original preservation assertions and both new test sets are retained.
The initial local inability to reproduce is historical, not the final diagnosis.

This local workspace DOES have the parent DESIGN_CONVENTIONS and the fresh
canonical artifacts/toolkit build; remote notes about their absence describe
the other checkpoint's environment. Continue using TOOLKIT.md's current runtime
selection and preserve the complete active production objective.

Merged remote milestone `36feedd`: continuous UV pointer and controller interactions now form exact undo gestures; cancellation restores the gesture baseline. Preserved the remote handoff in `docs/HANDOFF-36feedd.md`. Local toolkit provenance remains authoritative for this workspace.
