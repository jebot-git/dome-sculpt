# Live weights and visible-surface picking — 2026-10-29

This extends e65db2a; its exact record is archived in VALIDATION-e65db2a.md.
The broad run below corresponds to bf4fb86. Final displayed-frame/UV1/layout
refinements have focused tests, explicitly separated below. The full objective
in REQUIREMENTS.md remains incomplete. artifacts/ and build/ contain ignored
local evidence; those logs/captures are not distributed by a fresh clone.

## Release and real-loader synthetic XR

All49 registered Release CTests pass in333.39 seconds, zero failures:
artifacts/tests-live-weights-full.log and tests-live-weights-full-detail.log.
Includes new material-visibility, expanded weight-authoring and MToon rendering,
as well as every retained sculpt/paint/remesh/UV/bake/history/archive/export suite.

The OpenXR CTest passes in223.22 seconds. build/xr-client/report.json has73 checks,
failed=0, complete=true and status=passed. It retains controller/stylus pressure,
focus/input ordering, stereo linear/sRGB transport and native bake replays.
Tracking is synthetic and GL is software; this is not physical-device evidence.

## Worker/history and whole-core sanitizers

Four selected whole-core C/C++ ASan/UBSan/leak suites pass in28.72 seconds:
artifacts/tests-live-weights-sanitize.log and its -detail.log companion.
Suites: document-assets, material-visibility, rig-editing and weight-authoring.
UI/XR are OFF; C and C++ flags include ASan/UBSan/frame pointers, Release uses
-O1 -g1 -DNDEBUG. Native Mikk/xatlas remain instrumented. Runs set
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 and UBSAN_OPTIONS=halt_on_error=1.

New weight cases independently verify actual live scalar values before release,
unchanged Scene/source and immutable prior snapshots, coalesced sample counts,
frame-density invariance, exact pointer restoration on Undo/Escape, rapid
cancellation/reacquisition, accepted weight-dependent posed displacement, failed
final computation restoration, rotated posed normals and real nineteen-part CC0
avatar preview cancellation across document replacement. Existing linked locks,
no-op, skin/paint/source identity and one-gesture global history checks remain.

## Final frame-time, UV1 and BDF layout refinements

After the broad run, the native canvas captures its last drawn material time and
passes it to the picking overload. This matters when an animated texture is not
being continuously redrawn. It also refuses a stale displayed geometry revision.
Packed UV1 corners now validate before a query; BDF hint height is measured.

Three focused Release suites pass in3.91s (material-visibility, weight-authoring,
mtoon-render): tests-live-weights-final.log / -detail.log. Material visibility has
22 checks; the production MToon/linear GPU suite reports75 checks, including
animated-alpha GPU/picking agreement at explicit frame times, authored mask/
blend opacity, per-object bindings and geometry behind transparent cutouts.
Two final whole-core sanitizer suites (material-visibility/weight-authoring)
pass in14.09s: tests-live-weights-final-sanitize.log / -detail.log.
The successful final build log is build-live-weights-final-font.log.

A final review found that the pending-weight guard could throw before clearing
desktop input ownership when another source-changing command finished a held
gesture. Cleanup now precedes that guard. A native regression rejects selection
until the accepted gesture completes, confirms its exact weights/selection, then
performs a subsequent XR pressure gesture. Weight-authoring passes again in2.94s
(tests-live-weights-lease.log) and with whole-core sanitizers in12.96s
(tests-live-weights-lease-sanitize.log). The native application and test were
rebuilt (build-live-weights-lease{,-sanitize}.log). This final focused run covers
the ownership change after the broad and native GUI runs above.

## Native FLTK GUI

Final normal-size suite:14/14, exit0, report failed0;
artifacts/menu-live-weights-final-popup.log and menu-rig/report.json. It uses the
actual FLTK popup to choose the tip joint, holds Shift+mouse pressure, checks a
live canvas color change before release without changing the saved scene,
checks exact viewport restoration on Escape, then accepts another stroke and
uses native universal Undo to restore the complete posed source. Existing modal
identity, pose/morph editing and source-isolation checks remain.

Compact640x360,2x Unifont BDF:4/4, exit0, failed0;
menu-live-weights-compact.log and menu-rig-compact/report.json. Measured hints fit,
content scrolling changes controls, both action buttons stay fixed, and Escape
retains the project and main window. Captures visually inspected:
menu-rig/{weight-options,weight-live-preview}.png and
menu-rig-compact/weight-compact-{before,after}.png. The earlier native GUI also
passed14/14 before the final frame/layout change (menu-live-weights.log).

Actual native linkage in linkage-live-weights.log resolves the pinned
artifacts/toolkit-authoring/lib FLTK libraries with no Qt/GTK/Pango frontend.
All30 source_sha256 entries match third_party/sources.json; bdf_font.hpp remains
byte-identical to canonical qimgvr. No toolkit/third-party source was changed.

## Diagnostics and limits

The initial added CPU fixture tried to encode UV1 in VRM0, which its source codec
correctly refuses. The corrected fixture uses V0 UV0 and V1 UV1. A test initially
used unavailable Vec3 equality and now compares components. A hint measurement
initially called dx::font as a function; it is the canonical font variable, as
used by the bake editor. Those initial build/test logs are diagnostics, not passes.

The first final native GUI attempt selected the already-fully-weighted root joint
through an asynchronous keyboard popup sequence, so Add correctly produced no
color change and the harness timed out. Its weight-options capture confirmed the
root selection. The harness now drags to the second row of the real popup; the
complete14-check final run is the one cited above. A partial failed0 report is
not a pass without its process exit and complete expected checks.

Weight previews intentionally freeze posed geometry until release. Full stroke
recomputation/snapshot upload does not prove production VR timing or bounded
large-asset history/memory performance. Picking uses base-level nearest/bilinear
samples, not exact mip/anisotropic footprints. XR strokes remain swept-volume
brushes. Full automatic rig/bind/transfer, source UV adoption, production sculpt/
atlas/plugin workflows and full glTF/VRM/MToon interoperability remain incomplete.
No fresh external Khronos validation, physical device/accelerator execution,
system installation or active desktop/session restart is claimed.
