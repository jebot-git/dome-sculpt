# Agent handoff — editable UV and universal-menu checkpoint

The user requested committing/pushing the current stage and preparing a handoff;
feature work stopped at this checkpoint. The original production objective is
still active and incomplete. The preceding public checkpoint is `d0ba3c5`.
Repository: https://github.com/jebot-git/dome-sculpt, branch `main`.

## Objective and preserved constraints

Build the complete native FLTK desktop/VR sculpting, texturing, skinning and
avatar-authoring suite based on Blender 5.4 Alpha and previous Gmsh/OpenBrush work.
Incorporate Wings 3D, Seamless3d, OpenFX, MeshLab and other suitable native work
where beneficial. Preserve the full Medium/Substance-like authoring scope,
controller/stylus and joystick-menu workflow, trackball/3D-mouse input, baking,
remeshing, compact editable UVs, PBR/MToon, humanoid rigs, robust automatic weights,
shapekeys, full glTF/VRM and basic CATS/Auto-Rig Pro/Divine Cut/Retopo Flow/Random
Flow equivalents. No Python runtime; animation and final scene rendering are
excluded. CPU/OneAPI/ROCm/CUDA are build choices; only CPU is verified here.
The complete requirement/evidence matrix remains in REQUIREMENTS.md.

Recent steering: XR controls should suit controller input with continuous
sliders/wheels and other large targeting elements. Universal-menu conventions
must apply across the software. Commit/push current stage, then resume later.

## Implemented in this stage

- `uv.hpp`/`uv.cpp`: shared UV workspace, chart partitioning across UV-continuous
  non-cut edges, explicit corner cuts/fan splitting, island and vertex edits,
  pins, boundary/pin-preserving relaxation, packing with fixed pinned charts,
  bounds/degeneracy/analytic-overlap validation, bounded draft undo/redo.
- `uv-editor.cpp`: actual native FLTK editor (Mesh → UV editor, Alt+U), wheel
  scaling/Shift-wheel turning, chart/vertex tools, draft history, Cancel/Apply.
- XR Materials → UV editor uses the same workspace. Move/Scale/Turn select
  continuous stick/ray transforms; vertical scale/turn input is a fine wheel.
  Buttons expose pins, relaxation, packing, cuts, history and Apply/Cancel.
- `paint.cpp`: per-texel triangle/barycentric mapping and filtered transfer of
  all editable paint layers after UV edits. Metadata-only changes retain exact
  texture bytes. UV edits preserve triangle geometry/order. Optional binary
  `uv_pins`/`uv_seams` fields extend version-three mesh persistence.
- `document.cpp`: background UV transfer, stale-source checks, one global undo
  transaction, input capture, draft cancellation and modal/local history routing.
  Pending accepted asset work is dirty; save/export rejects pending work, while
  standalone XR final save waits for its asset job.
- `app.cpp`/`ui-conventions.hpp`: complete modal menu availability, focused-field
  Cut/Copy/Paste/Select All, UV/text/document Undo routing, consistent brush bounds,
  layer limits, actual persistent UV generation and safe chooser cancellation.
  Ctrl+W/Escape dispatches a modal's close callback before text consumes the key.
- `packaging/org.domed.Sculpt.desktop` and default FLTK xclass give all windows
  one identity. DomeD keeps application Quit/About/Hide and Window presentation
  controls. Canonical qimgvr exporter/helper files remain unchanged.
- Wings AutoUV basis pinned at `2f6672721d83af7bc39717c86deb12a2970ade9f`, with
  verbatim permissive license and source hashes. Chart continuity/cut design is
  adapted to C++ disjoint sets; no Erlang runtime is linked.

Read UV.md and UNIVERSAL-MENU-AUDIT.md for behavior and limitations. Advanced
seam-based conformal re-unwrapping/stitching, production mesh optimization,
import/remesh/bake, rig/weights/morph/MToon/VRM and plugin-equivalent work remain.

## Verified evidence and current failure

- Latest FLTK GUI run: 18/18 checks passed (`artifacts/gui-uv-final.log`,
  `artifacts/gui/report.json`). Includes visible UV edits, exact draft undo,
  retained surface appearance after Apply and global undo.
- Universal-menu exporter/dialog audit: 25/25 passed (`artifacts/menu-audit.log`,
  `artifacts/menu/report.json`). Includes clipboard, modal disabled states,
  parent identity, UV-local history, safe Save As cancellation and host fallback.
- Actual nested DomeD + native FLTK Wayland: 8/8 passed
  (`artifacts/dome-menu-final.log`, `artifacts/dome-menu/report.json`). Shell
  keyboard navigation activates exported commands; dialogs inherit menus;
  text Cut/Undo, modal File Close and shell Window→Desktop work. This is a private
  compositor test using the adjacent release binary and software Vulkan.
- Optimized core/appearance/GL suites passed in the latest ten-suite run. **The
  final native OpenXR suite failed one new UV persistence assertion** in
  `artifacts/tests-uv-menu-final.log` at `tests/xr_runtime.py` (the check comparing
  `uv_hash`, `field_hash` and `paint_hash` to the prepared baseline). The runtime
  exits successfully and renders 45 frames/38 stereo submissions/19 palette
  frames; `build/xr-client/uv.log` records this. Determine which hash comparison
  fails before changing behavior or weakening the assertion.
- Earlier complete ten-suite runs passed before the final continuous UV transform
  refinement (`artifacts/tests-uv-reviewed.log`, `artifacts/tests-uv-final.log`).
  They do not prove the current native UV fixture passes.
- **Do not trust `build/xr-client/report.json` as the latest run's success.** That
  script writes the report only at successful completion; the old 34-check report
  remains after the later failure. The log is authoritative for the final run.
- Desktop-entry validation and git diff whitespace checks pass. No physical
  headset/controller/stylus or accelerator validation has been performed.

## Sanitizer job completed during handoff preparation

The final seven-suite ASan/UBSan/leak run completed successfully in 331.55 seconds.
`artifacts/tests-uv-menu-sanitize.log` records all seven passes; its build log is
`artifacts/build-uv-menu-sanitize.log`. Exec session 94981 returned exit 0. Its
former shell/CTest PIDs were 3846791/3848630; they are no longer live jobs.

The optimized session 61954 exited 8 due to the XR assertion. GUI 55871, menu
audit 87236 and nested DomeD 67386 exited 0. No known validation job remains
running. Generated logs/captures stay local under ignored build/artifacts
folders. Pinned Wings reference/license bytes retain upstream whitespace;
.gitattributes excludes only that directory from whitespace errors. Hashes match.

## Resume first

1. Read the completed sanitizer and failing optimized logs. Check current
   processes before starting any new validation; do not rely on old handles.
2. Diagnose the new XR UV fixture. Capture `uv_info` and `basepaint` to persistent
   artifacts before its assertion (temporary projects currently disappear when
   the Python test exits). Review `DOME_XR_FIXTURE_UV` in `tests/xr_runtime.cpp`:
   joystick 8 opens root; 10 selects Materials; 12 enters UV; ray frame 14 chooses
   Scale; stick frames 15–18 shrinks; ray frame 20 applies. Tracking loss at 22
   must not cancel an already accepted edit. The last refinement changed top-row
   buttons from fixed steps to Move/Scale/Turn modes, so check mode selection,
   sampling, validity and commit timing with evidence.
3. Add a focused controller regression for continuous UV scaling/turning/fine
   wheel (the native UV test already covers ray-move/Apply and focus cancellation).
   Keep paint/geometry preservation assertions meaningful. Re-run the relevant
   OpenXR fixture, then required regression checks after any fix.
4. Extend the full production objective using the requirement matrix. Do not
   treat this checkpoint, a build, or the current passing subsets as completion.

Reproduction: CTest for native/GL/OpenXR; `uv run --with python-xlib --with pillow
python tests/gui.py`; add `--with dbus-next` for `tests/menu.py` and
`tests/dome_menu.py`. All UI tests use private buses/displays/HOME and no Qt/GTK.
No system installation or active user-session changes were performed.
