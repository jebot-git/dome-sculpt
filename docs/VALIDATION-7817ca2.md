<!-- Historical evidence from incoming 7817ca2, before the local authoring merge. -->

# Native bake authoring controls — 2026-10-29

This stage extends af492ee. The previous exact record is archived as
VALIDATION-af492ee.md / HANDOFF-af492ee.md. The complete objective remains
incomplete; native controls and passing fixtures do not prove every production
or device requirement. See BAKE-CONTROLS.md for actual user operations and limits.

## Native Release and OpenXR

All 41 registered Release CTests passed in 280.73 seconds, zero failures:
artifacts/tests-bake-controls-full.log and tests-bake-controls-full-detail.log.
Includes the existing actual-avatar/source/GLB/GPU, UV/paint/worker/history,
remesh/bake/rig/weight/groom suites plus the new bake-controls suite.

The real-loader synthetic OpenXR report now has 62 checks, zero failures,
complete=true and status=passed: build/xr-client/report.json. Four new replays
use the actual joystick Objects/Mesh tools/Baking route, aim-drag world-distance
slider and saved fixture. Accept publishes blue baked maps and retains both
complete hidden sources; ray Cancel, focus loss and later global Undo preserve
or restore the exact initial scene hash. Generic tracking loss is deliberately
excluded before the focus case so the intended event is isolated. All runtime
tracking is synthetic and GL is software.

After final layout and invalid-sector refinements, controller-controls and
bake-controls passed together in 6.87 seconds:
artifacts/tests-bake-controls-visible.log. The new native controller suite has
20 assertions. It checks hidden/locked source selection, logarithmic multi-row
sliders, integer AO/padding, staged settings without dirty/history changes,
ray/joystick confirmation, exact Bake/Clear Undo/Redo, focus cancellation,
generic returned-candidate cancellation, monotonic projection progress and
actual headset-menu Cancel during running AO work. These final focused tests
supplement the broad run; the full run preceded the final label-only refinements.

## Whole-core ASan/UBSan/leaks

All six selected whole-core suites passed in 133.78 seconds:
artifacts/tests-bake-controls-sanitize.log. Includes document assets, baked
textures, the complete sculpt/paint/voxel/atlas/bake/save/export workflow,
bake controls, source preview and rig editing. Both C and C++ retain ASan/UBSan
and frame-pointer flags, including native Mikk/xatlas. UI/XR are OFF; Release
uses -O1 -g1 -DNDEBUG. ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 and
UBSAN_OPTIONS=halt_on_error=1 apply to the runs.

The final expanded bake-controls boundary/interaction suite separately passed
in 0.85 seconds: artifacts/tests-bake-controls-sanitize-visible.log. Cancellation
continues to retain the previous candidate/scene/history even if a detached
operation returns normally after the request. No cancellation flag or progress
callback grants workers access to mutable model/UI state.

A final review moved the scene-operation busy/status release after every source
rejection check, closing a window where another operation could start before the
old failure handler. Document-assets, bake-controls and rig-editing pass again
in 2.67 seconds (tests-bake-controls-lease.log); document-assets and bake-controls
pass under whole-core sanitizers in 1.49 seconds
(tests-bake-controls-sanitize-lease.log). These focused runs cover that final
worker change after the broad Release/sanitizer runs above.

## Real native FLTK, BDF and universal menus

- Existing general universal menu suite: 53/53, exit 0, failed=0;
  artifacts/menu-bake-controls-general.log and artifacts/menu/report.json.
- Bake acceptance/error suite: 19/19, exit 0, failed=0;
  artifacts/menu-bake-controls-visible.log and artifacts/menu-bake/report.json.
  Checks actual modal ownership/command isolation; Return/Escape/Ctrl+W/
  universal Close options cancellation; logarithmic wheel feedback; asynchronous
  completion; baked source paint and exact retained source payloads; native
  Save/Undo/Redo/Clear; visible projection failure with the prior bake unchanged.
- Running bake cancellation: 3/3, exit 0, failed=0;
  artifacts/menu-bake-controls-cancel-visible.log and
  artifacts/menu-bake-cancel/report.json. A 512px/256-sample bake reaches live
  projection progress before Ctrl+W. No partial scene/history is published.
- Compact 640x360 / 2x Unifont BDF: 4/4, exit 0, failed=0;
  artifacts/menu-bake-controls-compact-final.log and
  artifacts/menu-bake-compact/report.json. Measured wrapped text fits the screen,
  content scrolls and exact action-button footer pixels stay fixed. Cancellation
  retains the complete source scene. Captures were visually inspected.

Readouts explicitly use the current DomeD bitmap face/size. Visual inspection
caught default bottom-aligned slider labels and undersized numeric readouts;
left alignment, measured wrapping and explicit text metrics correct these.
The final progress-bar accent-only change uses FL_SELECTION_COLOR and passes
all 19 acceptance/error checks again: artifacts/menu-bake-controls-accent.log,
exit 0 / failed=0. Its final capture was visually inspected. Screenshots:
artifacts/menu-bake/{bake-options,bake-complete,bake-projection-error}.png and
artifacts/menu-bake-compact/bake-compact-{before,after}.png.

## Diagnostics and evidence limits

Initial private-HOME GUI startup attempts exceeded 30 seconds before any main
window. A private debugger launch traced Fontconfig's FcConfigGetFontDirs /
FcConfigBuildFonts / FcFreeTypeQueryAll path inside canonical BDF discovery,
before App/Document construction: artifacts/bake-startup-backtrace.log. This is
cold font discovery, not a model deadlock. The harness permits 90 seconds only
for initial startup; interaction/operation assertions retain 30-second deadlines.
Cold-start optimization remains a canonical-helper follow-up. Shared BDF helper
matches qimgvr byte-for-byte; all 30 pinned source hashes remain valid.

The compact test initially sent wheel events without explicitly focusing the
modal. It now focuses the dialog and uses the visible native scrollbar, matching
the proven voxel harness. Its content/footer assertions are unchanged. The
first native focus-cancel fixture compared a logarithmic float to literal 20;
it now compares the exact prior confirmed setting. An initial sanitizer build
used a nonexistent assets target; the correct document-assets target was then
built and all selected checks completed. FLTK readout-width adjustment uses the
installed 1.4.5 header's value_width API. Earlier diagnostic logs are not passes.

artifacts/linkage-bake-controls-final.log resolves the canonical 22-patch FLTK
and has no Qt/GTK/Pango frontend dependency. Python remains test orchestration;
application workers, geometry, baking and controls are native C++20. No system
installation, active-session restart, physical headset/controller/stylus/3D-mouse
or accelerator-backend completion is claimed. No new external Khronos validator
run is claimed. Durable recipes, custom cages, multi-source/partial masks and
production-scale quality/performance remain further work in the full objective.
