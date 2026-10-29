# Agent handoff — voxel remesh authoring checkpoint

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

See the newest entry in [VALIDATION.md](VALIDATION.md). Generated logs stay under
artifacts/ and are ignored. The complete Release build passed, including the FLTK frontend and XR. All
six affected native suites passed in 21.88 seconds; exact names and log paths
are recorded there. The initial focused voxel-authoring run also passed.
The new test exercises Document worker integration, posed source retention,
scale/material, undo/redo/sculpting, in-process XR ray/stick/fine-wheel controls,
focus cancellation, invalid inputs, cancellation and generation guards.

Earlier 22-suite, GUI 23/23, universal-menu 39/39 and five-suite sanitizer results
are for the previous UV integration, not fresh proof of this added dialog or
workflow. No feature-specific real-loader OpenXR replay, physical device,
accelerator, new sanitizer, system installation or session restart is claimed
unless explicitly recorded in the latest validation entry.

## Resume next

1. Review this checkpoint before extending it. Add native GUI/universal-menu
   coverage for the new modal: exported enabled state, Cancel-default Enter,
   Ctrl+W/Escape, slider interaction, explicit Confirm and background failure.
   Check layout with large BDF sizes/small displays; the dialog currently uses
   eight font-sized rows and caps width but not height.
2. Extend the real-loader OpenXR fixture for Objects/Mesh tools/Voxel remesh,
   exact accepted size, preserved hidden source, cancel/focus and global undo.
   Add a nonempty scalar-field source fixture alongside the posed painted cube.
   Run focused whole-core ASan/UBSan/leak verification of the affected paths.
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
