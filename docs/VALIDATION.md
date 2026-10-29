# Combined native authoring validation

This combines 0141926 and 88cde79, preserving both public histories. Historical
evidence is archived in VALIDATION-0141926.md and VALIDATION-88cde79.md; the
results below were gathered on the combined application source, including the
new bake/weight history regression. The full REQUIREMENTS.md objective remains
incomplete. All logs/reports/captures below are local ignored artifacts, not files
distributed by a fresh clone.

## Reconciliation with concurrently advanced main

During final fetch, origin/main had advanced to13b79ab, independently merging
the same authoring/bake stages. Its exact records are archived in
HANDOFF-13b79ab.md / VALIDATION-13b79ab.md. Both histories are retained. The broad
48-suite/six-sanitizer/GUI evidence below was gathered for fa7aba4. The final
follow-up changes only two palette lines in runtime source: include asset_busy
in the raster cache signature and use “Working” for any running cancellable
operation. All document/geometry/input/export headers and implementation remain
byte-for-byte equal to fa7aba4, confirmed by the runtime diff scope in
artifacts/source-authoring-main-followup-scope.log. The incoming no-op test
assertion and mutually exclusive/implicit bake GUI fixture selection are retained.

Affected render/application and weight-test targets were rebuilt with GNU Make
/fast targets against the already verified unchanged core. Four focused Release
suites pass in17.05 seconds: controller-controls, spatial-gpu-render, bake-controls
and weight-authoring (tests-authoring-main-followup.log). The expanded weight
suite passes with whole-core ASan/UBSan/leaks in20.18 seconds
(tests-authoring-main-followup-sanitize.log). Build logs use
build-authoring-main-followup{,-sanitize}.log. These are focused verification of
the final delta; the full48-suite run below preceded those two palette lines.
The updated GUI harness also passes4/4 with only DOME_MENU_BAKE_COMPACT=1 (no
separate bake-flow flag), exit0 and failed=0:
menu-authoring-main-followup-compact.log. This confirms the incoming implicit
fixture selection while exercising the final rebuilt application at2x BDF.

## Release and synthetic OpenXR

All48 registered Release CTests passed in328.08 seconds, zero failures:
artifacts/tests-authoring-integration-full-retry.log and
artifacts/tests-authoring-integration-full-detail.log. Includes the complete
sculpt/paint/remesh/atlas/bake/save/export workflow, source/image archives,
GLB/VRM/source morphs, material rendering, UV solve/workflow, weight authoring,
worker/history/recovery and native controller controls.

build/xr-client/report.json has73 checks, failed=0, complete=true and
status=passed. It uses the real OpenXR loader with synthetic tracking/software
GL, including explicit linear and sRGB swapchains, controller/stylus/focus/input
ordering, UV controls and the four bake accept/cancel/focus/undo replays. The
OpenXR CTest passed in223.37 seconds within the full run. This is not physical
headset/controller/stylus evidence.

## Whole-core sanitizers and cross-workflow regression

Six selected whole-core suites pass in153.30 seconds:
artifacts/tests-authoring-merged-sanitize.log and
artifacts/tests-authoring-merged-sanitize-detail.log. Suites: GLB interoperability,
bake controls, source morph transport, UV workflow, weight authoring and document
export. Both C and C++ use ASan/UBSan/frame pointers, including Mikk and xatlas;
UI/XR are OFF, Release flags are -O1 -g1 -DNDEBUG. Runs use
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 and UBSAN_OPTIONS=halt_on_error=1.

The new weight-authoring integration regression starts from an actual guarded
source-aware bake into a skinned target. It performs a linked weight gesture,
checks release of the cancellation lease, compares complete painted payloads
and hidden source records, independently undoes/redoes Bake and Weights, then
clears the bake while retaining authored deformation and restores the exact
combined scene. Existing no-op/constraint tests verify no phantom history with
the merged operation-changed/cancellation paths. It passes in both builds.

## Native FLTK GUI and shared toolkit

All listed GUI processes exited0 and their reports contain failed=0:

| Native workflow | Passed checks | Log under artifacts/ | Report directory under artifacts/ |
| --- | ---: | --- | --- |
| Universal menus/modal Edit/chooser/host fallback | 53 | menu-authoring-integration-general.log | menu |
| Bake acceptance, failure, Undo/Redo/Clear | 19 | menu-authoring-integration-bake.log | menu-bake |
| Running bake Ctrl+W cancellation | 3 | menu-authoring-integration-cancel.log | menu-bake-cancel |
| Compact640x360, 2x Unifont BDF, fixed footer | 4 | menu-authoring-integration-compact.log | menu-bake-compact |
| Rig/weights, actual Shift-drag, exact Undo | 12 | menu-authoring-integration-weights.log | menu-rig |
| Native selected-avatar chooser/background VRM export | 5 | menu-authoring-integration-export.log | menu-export |
| Conformal unwrap, pins, seams/stitching and cancellation | 12 | menu-authoring-integration-uv.log | menu-uv-workflow |
| Real nested DomeD Wayland universal menu | 8 | menu-authoring-integration-wayland-retry.log | dome-menu |

The separate nested compositor retry exited0: real keyboard/header navigation,
modal Edit Cut/Undo, File Close cancellation and shell-owned Window actions all
pass. It used the existing local dome-compositor/target/release/dome-compositor
binary (SHA2563722d2835b685c895095181b3cc2e7c6ebd0f7116b26ac3d5ea3a63ef1ce6ce3;
its checkout is3565a58), plus this rebuilt client. This does not claim a fresh
282467e compositor build in this workspace; that revision pins the new toolkit.

Visually inspected menu-bake/bake-complete.png,
menu-rig/{weight-options,weight-gesture}.png and
menu-uv-workflow/unwrapped.png: bitmap labels/readouts, accent, native controls,
weight heatmap and explicit UV actions render correctly at the tested sizes.
The previous bake-only GUI directories were copied to evidence-7817ca2 before
the new runs. The new reports belong to the combined source.

The fresh canonical22-patch toolkit comes from clean282467e into
artifacts/toolkit-authoring. All22 patch hashes/ordered digest and all30 source
hashes match third_party/sources.json. build-toolkit-authoring-with-sysroot.log
records success; the earlier build-toolkit-authoring.log is a missing-GLU
configuration diagnostic, not a successful build. The existing local GLU sysroot
supplies headers/pkg-config without system installation. Actual native linkage
(linkage-authoring-integration.log) resolves this prefix's libfltk/libfltk_gl
and has no Qt/GTK/Pango frontend dependency. See TOOLKIT.md.

## Interrupted attempts and limits

The first broad run and concurrent nested compositor harness ended with143
(SIGTERM), without a preceding application/test failure. Their processes were
confirmed terminal; the two orphaned private host processes were identified by
their exact command/PID/group and terminated. The active desktop/session remained
intact. The signal's sender/cause was not established. Preserved diagnostics:
tests-authoring-integration-interrupted{,-detail}.log,
xr-authoring-integration-interrupted.json and
menu-authoring-integration-wayland-interrupted.{log,json}. Incomplete reports
are not passes. A fresh complete broad run then passed48/48 as recorded above.

This checkpoint does not prove live weight previews, alpha-aware weight picking,
large-asset VR timing, all source-map UV editing, generated/painted VRM avatars,
full VRM1/glTF/MToon interoperability or physical device/accelerator support.
No new external Khronos validator run, system install or active-session restart
is claimed. Python is optional test orchestration; shipped authoring is native
C++20/FLTK/EGL/OpenXR.
