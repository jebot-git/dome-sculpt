# Combined plane-cut and production verification — 2026-10-30

Application source: 2ec8a5a59990f2d64887434560a6402d5a9ec665, branch
work/plane-cut-integration. This merge preserves plane checkpoint de3f433 /
source 5b0f9f5 and production main 2c89e8d / source 79fe64a. Subsequent handoff edits
are documentation only. Earlier proof is archived separately in
VALIDATION-2c89e8d.md and VALIDATION-de3f433.md; retain their exact revision scopes.
Artifacts/builds are ignored local evidence and do not accompany a fresh clone.

Verification below is complete at this source. Final fetch discovered newer
main 9a0cfc4 / source 20c132d (sculpt masks/symmetry), which is NOT integrated here.
This feature checkpoint preserves that newer main; none of these results proves
the future Volume v3/Brush/Sample integration.

## Release and real-loader synthetic XR

All 69 non-XR Release CTests pass, zero failures, in 461.56s. Logs:
artifacts/tests-cut-integration-native.log, -native-detail.log and the preserved
full individual output -native-output.log. The registered OpenXR harness is
verified separately below; there are 70 registered suites in this configuration.

The full real-loader XR harness passes 127/127 checks, failed=0, complete=true,
status=passed. It retains 117 existing checks and adds 10 cut workflow checks.
Log: artifacts/tests-cut-integration-xr.log. Report:
build-cut-integration/xr-client/report.json, also copied to
artifacts/report-cut-integration-xr.json. All client subprocesses and the harness
exit 0. The registered harness command was invoked directly during non-XR CTest,
with the same explicit toolkit environment and unchanged 40-second client limit.

The full XR harness is the same Python command registered by openxr-client,
invoked independently while non-XR CTest runs so their CTest temporary logs do not
collide. Its 40-second per-client timeout is unchanged. Tracking and graphics are
synthetic/software; this is not physical headset/controller/stylus evidence.
New cut cases wait for actually enabled Apply sectors read from real GL images,
then use joystick navigation and an actual ray slider. They inspect native files
for exact retained source data, a closed grouped shell/cap and independently
expected 60-percent-slider plane at x=0.06928203m. An unconfirmed live adjustment
must save the exact original scene through the real standalone shutdown path.

The cut-authoring suite now has 56 assertions, including independent Skin page 14 /
reserved Masks page 15 / Retopo page 16 / Cut page 17 routing. Existing mapped-paint, nearby-surface,
transformed pose, exact history, cancellation/no-op/stale guards and native/glTF
persistence checks remain. Six new MToon clipping checks and the actual cut GPU
surface/gizmo/menu captures remain alongside main's material/Unicode/retopo
fixtures. Cut menu capture now uses page17. Source is frozen during validation.

## Whole-core sanitizers

Nine whole-core C/C++ ASan/UBSan/leak suites pass in 91.44s, zero failures:
retopology-authoring, cut-authoring, document-assets, bake, skin-authoring,
worker-shutdown, scene-schema-compat, retopo-cancellation-boundaries and
cut-cancellation-boundaries. Logs: artifacts/tests-cut-integration-sanitize.log,
-sanitize-detail.log and preserved full -sanitize-output.log. Complete selected
build: artifacts/build-cut-integration-sanitize.log.

UI/XR are OFF in build-sanitize. Core C/C++, Mikk and xatlas use
-fsanitize=address,undefined, frame pointers and Release -O1 -g1 -DNDEBUG.
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 and UBSAN_OPTIONS=halt_on_error=1.
The combined headers/core were rebuilt; no pre-layout static archive was reused.
No thread-sanitizer or new accelerator execution proof is claimed.

The deterministic shutdown fixture now covers six workers and their actual
predicate-mutex handshakes. The new cut boundary fixture gates an earlier real
save at fsync, queues Apply, cancels through desktop and joystick commands,
checks exact source/preview retention and prior-save isolation, then retries
and verifies exactly one global Undo. Before the fix it fails with the actual
accepted-cut cancellation exception (artifacts/test-cut-boundaries-before.log).
The corrected source also passes six focused Release suites in 6.22s:
artifacts/test-cut-integration-boundaries.log.

## Native FLTK, universal menus and BDF

All 103 GUI checks pass; every harness process exits0 and every report has
failed 0:

| Flow | Checks | Log under artifacts/ |
| --- | ---: | --- |
| Plane cut | 12 | menu-cut-integration.log |
| Plane cut, compact 2x BDF | 6 | menu-cut-integration-compact.log |
| Retopology | 12 | menu-cut-integration-retopo.log |
| Retopology, compact 2x BDF | 6 | menu-cut-integration-retopo-compact.log |
| General universal menus | 53 | menu-cut-integration-general.log |
| Skin binding/transfer | 11 | menu-cut-integration-skin.log |
| Unicode input/clipboard/history | 3 | menu-cut-integration-unicode.log |

Scripts were copied unchanged to artifacts/cut-integration-ui/tests; its build
symlink points to build-cut-integration, not the older build/ executable. Reports
and captures are under artifacts/cut-integration-ui/artifacts. Optional test
orchestration uses uv with dbus-next, python-xlib and pillow, private HOME/bus/Xvfb,
90s cold-font startup and 30s interactions. Python is not a runtime dependency.

Checks exercise actual exporter availability/check/radio state, modal ownership,
text/UV/draft/global history, Cancel-default confirmations, chooser Close/Ctrl+W,
host loss/return, cut clipping, Apply/Save retained source and grouped output,
retopo persistence and skin/reference isolation. Visually inspected this source's
menu-cut/cut-live-preview.png and menu-cut-compact/cap-controls-after.png: clipped
textured geometry/edge-on plane and readable large BDF sliders with fixed actions.
A real-loader Apply-ready image was also inspected after PPM-to-PNG conversion.
No fresh actual DomeD Wayland-host run is claimed; the previous 8-check host proof
remains scoped to its earlier source in VALIDATION-2c89e8d.md.

All 30 source_sha256 entries match third_party/sources.json; the vendored BDF
helper matches canonical ../qimgvr/fltk/bdf_font.hpp. Final actual linkage in
artifacts/linkage-cut-integration.log resolves FLTK from
artifacts/toolkit-authoring/lib with no Qt/GTK/Pango frontend. Toolkit revision
282467e / 22 patches is unchanged. Build setup is in HANDOFF.md and TOOLKIT.md.

## Actual Intel graphics

Both additional hardware graphics processes exit 0 on
Mesa Intel(R) Graphics (ADL-N), with LIBGL_ALWAYS_SOFTWARE=0. The production
MToon renderer's --hardware guard rejects llvmpipe/softpipe; all 81 assertions
pass, including the six new V0/V1 clipping/isolation checks. The full spatial
render fixture also passes: actual plane/cap/gizmo/menu, immutable cached retopo
geometry and transform reuse, PBR/toon, BDF palette, canonical normals and AO.
Logs: artifacts/tests-cut-integration-hardware-mtoon.log and -hardware-spatial.log.
Captures: artifacts/gpu-cut-integration-hardware-*.ppm. This proves local Intel
OpenGL rendering, not Arc/OneAPI compute or physical XR input/tracking.

## Diagnostics and remaining limits

The first combined build passes (artifacts/build-cut-integration.log). Final
incremental product/test build passes (build-cut-integration-final.log), followed
by the runtime progress-report build (build-cut-loader-progress.log). GCC15 issued
-Wfree-nonheap-object in the unchanged write_glb vector path during the full
build; the new cut/retopo sanitizer glTF cases pass. No new warning in the edited
cut/integration code was observed. Initial four focused suites pass in 13.12s,
before the accepted-cut fix and loader extension; they are not final broad proof.

Two isolated cold-cache 80-frame cut probes hit their 45-second process limits.
The second preserved actual progress through 56 completed frames, enabled preview
and Apply plus the real slider sample; the timed-out process was confirmed gone
before retry. No worker deadlock or geometry failure was inferred from timeout.
Repeating with the populated private graphics cache passes accept in 26.43s and
unconfirmed in 7.96s, with exact closed geometry/source checks. Logs/reports are in
artifacts/cut-loader-first and artifacts/cut-loader-progress. This comparison
suggests cold graphics compilation/startup cost; it is not a hardware timing or
profiling result. Cold startup/software latency remains a performance limit.
The final full harness uses its normal shared private HOME and unchanged timeout.

Current cuts still act on one object. Repeated cuts across shell/cap siblings,
general Booleans, nested/touching contours and self-intersection repair remain
unfinished. Closed caps require oriented manifolds and reject ambiguous/near-
vertex/empty cases. Render-corner source expansion still needs compaction; GPU
preview caching and production-size memory/latency remain open. Static 512-pixel
PBR outputs retain original assets but do not recreate every MToon/UV1/extension,
new skin/morph binding, durable recipe or unconfirmed-draft recovery feature.
The full REQUIREMENTS.md objective remains incomplete.

No system installation, active-session restart, fresh external Khronos validator,
physical XR/input-device test or new OneAPI/ROCm/CUDA execution was performed
this stage. Actual Intel graphics proof is separate above. All stage build,
validation and app handles were terminal and the final process inventory found
no remaining compiler/test/application jobs.
