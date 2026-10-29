# Agent handoff — validated voxel remesh authoring

Repository: https://github.com/jebot-git/dome-sculpt, branch main. This checkpoint
records work in progress for a later agent; the full objective remains in
[REQUIREMENTS.md](REQUIREMENTS.md). Read AGENTS.md and TOOLKIT.md before resuming.
The previous checkpoint at 90601ca is preserved in
[HANDOFF-90601ca.md](HANDOFF-90601ca.md), including earlier integrations and tests.
No agents were spawned here. Remote handoff agent names refer to another
workspace; do not treat them as live handles. Fetch before overlapping work or
pushing, preserve remote changes, and never force-push this shared branch.

## Current implementation

- Native Mesh > Voxel remesh... opens a shared logarithmic millimetre slider.
  XR reaches the same operation through Objects > Mesh tools > Voxel remesh;
  ray dragging, horizontal stick and the vertical fine wheel adjust the size.
  The FLTK slider also supports wheel/arrow adjustments with a millimetre
  readout. Content wraps and scrolls on compact screens; action buttons stay
  fixed and reachable with doubled bitmap fonts.
  Objects/Move left moved into Mesh tools; existing movement remains available.
- Confirm queues the existing source-guarded detached scene asset worker.
  A new editable scalar-field object samples the displayed posed/morphed mesh,
  or extracts a sculpt source surface first. World millimetres account for the
  source scale. The new object retains transform and scalar material factors.
- The complete original object is retained and hidden, including paint layers,
  rig/pose and sculpt layers. Paint and deformation are not transferred to the
  new sculpt. This is source-preserving voxel conversion, not completion of
  painted/adaptive/quad remeshing or the full bake workflow.
- Options alone produce no scene/history change. Cancel/focus loss restores the
  previous size; starting another command cancels this action rather than
  implicitly running it. One scene Undo/Redo restores exact object identity,
  source visibility/selection and voxel data. The result accepts sculpt strokes.
- Closed, consistently oriented surfaces are required. Invalid/open sources,
  coarse grids capturing no interior, sample budgets, cancellation and stale
  document generations fail without publishing partial scene state.

Implementation: mesh-authoring.hpp/cpp (detached operation), mesh-editor.hpp/cpp
(FLTK modal), Document asset/control APIs, input.cpp/palette.cpp (XR controls).
The asset queue gained a private lock-held helper so controller confirmation can
queue without recursively locking the model mutex. Heavy work stays off the
input/render thread. Runtime remains C++20, FLTK, BDF Fontconfig/FreeType and
native EGL/OpenXR; canonical shared desktop helpers are unchanged.

## Verification for this checkpoint

The new verification extends c27dadf. See [VALIDATION.md](VALIDATION.md).
Generated logs/captures remain under artifacts/ and are ignored.

- Complete Release build passed with canonical FLTK. All 22 non-XR native/
  render/appearance CTests passed in 76.25 seconds:
  artifacts/tests-voxel-native-final.log. The separate real-loader OpenXR CTest
  passed in 159.81 seconds with 53 complete assertions:
  artifacts/tests-voxel-xr-verified.log and build/xr-client/report.json.
- Native GUI regression passed 23/23: artifacts/gui-voxel-regression.log,
  retaining sculpt/material/paint and exact UV gesture/Apply/global-Undo checks.
- The expanded universal menu suite passed 50 checks:
  artifacts/menu-voxel-final-verified.log. It exercises native modal identity,
  context-aware enabled state, Cancel-default Enter, Escape/Ctrl+W/universal
  Close, real slider and wheel feedback, visible grid-budget failure, accepted
  remesh recovery persistence and global Undo. Three additional compact-screen
  checks pass at 640x360 with a doubled Unifont BDF; content scrolls and buttons
  stay fixed: artifacts/menu-voxel-compact-final.log. Captures were inspected;
  a clipped hint discovered visually was shortened/wrapped before the final run.
- Three whole-core ASan/UBSan/leak suites passed in 64.33 seconds:
  controller-controls, document-assets and voxel-authoring;
  artifacts/tests-voxel-sanitize-final.log. The first build wrapper ended with
  signal 143 before tests; a fresh up-to-date build and final test run completed
  with exit 0. No sanitizer failure is hidden by that interrupted invocation.
- The voxel fixture now covers a nonempty scalar-field source, exact retained
  field/interior/spacing/Undo, and cancellation of options by unrelated commands,
  in addition to the posed/painted source and worker/transaction checks.
- OpenXR fixtures navigate actual Mesh tools and the aim slider through the
  real loader, verify exactly 50 mm spacing and the intact hidden source, and
  restore the exact baseline scene by ray Cancel, focus cancellation and Undo.
  The initial Undo replay selected Save due to the fixture's wrong sector;
  the corrected replay and full 53-check run passed. A final fixture audit
  separated focus loss from the earlier tracking-loss event: the focus replay
  reports one focus loss/regain and zero tracking losses before exact scene
  restoration. These remain synthetic
  tracking and software GL, not physical headset/controller verification.

The --project-info diagnostic now includes scene_hash and an objects list with
visibility, mesh state, spacing and payload_hash (all encoded object data except
visibility). These support persisted cross-process source-retention checks;
they are diagnostic hashes, not a new project schema or security checksum.
Linkage: artifacts/linkage-voxel-final.log selects artifacts/toolkit and lists
no Qt/GTK/Pango dependencies. No installed prefix or session was changed.

## Resume next

1. Preserve the verified desktop/XR voxel workflow and inspect remote state
   before overlapping changes. Continue scene/authoring integration toward the
   full objective. Keep slider values in world millimetres and the retained
   source intact; do not silently substitute scalar materials for paint.
2. Expand production fixtures and resource/cancellation profiling for large
   meshes and retained scenes. Native GUI/real-loader/sanitizer checks above
   cover the initial workflow, not all sizes, topology classes or hardware.
3. Continue full native remeshing/baking authoring with explicit source/target
   paint transfer, robust budgets/error feedback and source preservation. Inspect
   remote changes first: other workspaces previously advanced creator/import/
   projection/rig/deformation foundations concurrently.
4. Preserve the full production requirements: desktop and headset authoring,
   retopology, complete UV workflows, stencils/projection/texture sets, rigging,
   weights/morphs, anime/toon creator, full MToon/glTF/VRM, plugin-equivalent work,
   physical device/lifecycle testing and CUDA/ROCm/OneAPI verification. This
   voxel milestone does not narrow or complete that objective.

Use artifacts/toolkit (canonical DomeD742d009, FLTK1.4.5 and all 22 patches).
GUI/menu harnesses need its lib directory first in LD_LIBRARY_PATH; use private
Xvfb. Read INPUT.md, UV.md, IMPORT-AND-MESH.md and UNIVERSAL-MENU-AUDIT.md. Keep
immutable render snapshots, source/revision guards, ordered saves and exact
cancellation/history. Inspect live processes and logs before restarting tests.
No system prefix or active session was changed for this checkpoint.
