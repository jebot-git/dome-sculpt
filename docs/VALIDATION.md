# Native retopology authoring — 2026-10-30

The main implementation is fdb132b, based on checkpoint 3aa47e5. The final
source is 251cd3de1390f6e6748c73aed4428af5f3e41ece. Broad evidence and final
focused refinements are separated below. The full production objective in
REQUIREMENTS.md remains incomplete. Previous live-weight validation is archived
in VALIDATION-33f26a2.md; older integration records remain intact. Artifacts and
build directories are ignored local evidence, unavailable in a fresh clone.

## Broad Release and real-loader synthetic XR

At fdb132b, all 50 registered Release CTests passed in 411.19 seconds, zero
failures: artifacts/tests-retopo-authoring-full.log and its -detail.log companion.
This includes the new authoring suite and all existing sculpt, paint, UV, bake,
archive, humanoid, source/material, format, controller and graphics suites.

The OpenXR client CTest passed in 250.10 seconds. build/xr-client/report.json
has 73 checks, failed=0, complete=true, status=passed. This retains the existing
real-loader runtime/rebase/pressure/menu/material/UV/bake coverage. Tracking is
synthetic and graphics are software. New retopology controller gestures are
covered separately through the production Document/SpatialInput path; the
73-check loader script does not specifically replay the new topology workflow.

## Final source and whole-core sanitizers

Final source 251cd3d passes eight focused Release suites in 33.05 seconds:
retopology-authoring, retopo, document-assets, weight-authoring,
controller-controls, spatial-gpu-render, scene-authoring and document-export.
Logs: tests-retopo-authoring-verified.log and -verified-detail.log.

Four whole-core C/C++ ASan/UBSan/leak suites pass in 23.94 seconds:
retopology-authoring, retopo, document-assets and weight-authoring. Logs:
tests-retopo-authoring-verified-sanitize.log and its -detail.log companion.
UI/XR are OFF in build-sanitize. C and C++ use address/undefined sanitizers and
frame pointers; Release uses -O1 -g1 -DNDEBUG. Native Mikk/xatlas are instrumented.
Runs set ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 and
UBSAN_OPTIONS=halt_on_error=1. No thread-sanitizer or accelerator proof is claimed.

The final retopology suite has 56 assertions. Independent expectations cover
source projection, ordered outward quad construction, exact immutable gesture
cancellation and draft/global history, pins/seams and atomic incidence deletion,
world-distance bounds under rotated/scaled groups, combined skin/morph projection,
source isolation, real native save/open and exported GLB reimport, malformed source
references, clean orphan-source refusal, invalid Apply correction, stale document
replacement, idle BVH release, joystick-only authoring, the distance slider and
accepted Apply immediately before XR shutdown. Reopen/cancel checks separately
verify the high reference extent and restored smaller authored target geometry.

The final GPU suite checks visible point/edge feedback, source-surface occlusion,
linear XR overlay composition and changing controller mode feedback. Captures:
build/gpu-retopo-{baseline,front,linear,occluded,menu-place,menu-move}.ppm.
The final mode capture was converted for inspection to
artifacts/retopo-menu-move-verified.png; shorter New patch / Tool mode / Snap range
labels fit their sectors. Final successful build logs:
build-retopo-authoring-shutdown.log and -shutdown-sanitize.log.

## Native FLTK universal menus

Final product source passes the dedicated normal-size topology flow 12/12 and
compact 640x360 with 2x Unifont BDF 6/6; both processes exit 0, both reports have
failed=0. Logs: menu-retopo-authoring-verified.log and -verified-compact.log.
Reports/captures: artifacts/menu-retopo and artifacts/menu-retopo-compact.

Actual menu export verifies availability and Place/Move radio state, source/save
isolation, native modal identity and Universal Close, held point feedback without
scene changes, exact Escape viewport restoration, ordered face Apply/Save,
retained original payload, one global Undo/Redo and reopening/cancellation.
Compact coverage checks the real projection slider, fixed action buttons,
modal cancellation and unchanged native scene. Visually inspected:
menu-retopo/ordered-corners.png and
menu-retopo-compact/projection-large-bdf.png. Earlier same-flow runs also passed.

The existing general native menu suite passed 53/53 at fdb132b, exit 0,
failed=0: menu-retopo-general.log and artifacts/menu/report.json. Its coverage
includes modal text/UV routing, chooser cancellation, host loss/return, sculpt
layers and voxel authoring. Do not relabel this broad GUI run as final-source
coverage of the later topology lifetime/shutdown fixes.

Actual linkage in artifacts/linkage-retopo-authoring.log resolves the pinned
artifacts/toolkit-authoring/lib FLTK libraries with no Qt/GTK/Pango frontend.
All 30 source_sha256 entries match third_party/sources.json; the BDF helper is
byte-identical to canonical qimgvr. No toolkit or upstream source changed.

## Diagnostic attempts and limits

The first sanitizer build exited 143 (SIGTERM) after completing the core and
three selected executables, before finishing the weight-authoring target. Its
process and all compiler children were confirmed absent before continuing. The
three built whole-core suites passed in 3.10 seconds; that partial build is not
claimed as the final four-target build. The later shutdown build completed all
four targets and is the final evidence above.

A final reopen/cancel fixture initially pressed the joystick immediately after
reopening reset input ownership. The required fresh neutral sample was missing,
so no menu opened and the cancellation assertion failed in Release and sanitizer
runs (tests-retopo-authoring-final*.log). The corrected fixture explicitly rearms
and verifies page 14 before Cancel; the successful 56-assertion final runs above
include the corrected sequence. The earlier failed runs are diagnostics, not
passes. An intermediate 52-assertion refinement run passed before the reopen
check was added. The final shutdown test also guards accepted input draining
before the asset job becomes visible to wait_asset().

These are manual point/edge/triangle/quad workflows. Automatic quad generation,
bridging/extrusion, edge collapse, wire-only scene objects/draft recovery and
production-size topology/overlay performance remain unfinished. Source picking
uses the geometric reference rather than alpha-cutout/geodesic semantics. Atlas
UVs are derived separately; native seam/pin metadata is not yet an atlas
prescription. Changed topology creates a new retained version rather than
implicitly transferring old paint/skin/morph data. Retain the broader unfinished
sculpt, texture, rig, plugin and full glTF/VRM/MToon requirements.

All validation/build handles were checked to terminal and no validation/compiler
jobs remained when inspected. No system install, active-session restart, fresh
external Khronos validation, physical headset/controller/stylus/3D-mouse testing
or real OneAPI/ROCm/CUDA execution is claimed.
