# Native resolution and immutable VRM expression remap verification — 2026-10-30

Product source803479d7478af8e771cdc3dbb6b2d1ad72728d77 preserves maina4af86d
and resolution/cutfff726f via5985858, then fixes the independent VRM0 bind bug.
Test-onlyee2b57d adds an actualDomeD resolution workflow. Test-only19ad5aa fixes
cross-protocol Unicode test ordering and adds optional burst input. No product
code changed after803479d. The full production goal remains incomplete. Prior
a4 proof is archived exactly in VALIDATION-a4af86d.md; earlier archives remain.

Ignored local manifest:
artifacts/resolution-production-final-manifest-19ad5aa.json. It pins product/test
hashes, binary hashes, compilers/options, reports, terminal jobs and logs. Current
worktree artifacts/resolution-production-a4 has fresh separate coherent Release
CPU/OneAPI and C/C++ whole-core sanitizer build directories. Logs below live in
its parent artifacts directory and are not shipped in a fresh clone.

## Builds and toolkit

Canonical DomeD282467e727df78c8e7f2452aa05a7747bac854a6 was rechecked at remote
HEAD2026-10-30. Release FLTK1.4.5+22 patches is locally built under
artifacts/toolkit-282467e, using
**lib64**. Both app builds use explicit absolute FLTK_DIR ending share/fltk;
linkage-resolution-production-{cpu,oneapi}.log verifies all3FLTK libraries there
and no unresolved/Qt/GTK/Pango frontend library. Parent design conventions apply.
No toolkit/system installation or active-session/display-manager restart occurred.

Local CPU is i7-12700, GCC16.2.1; native Release-O3/-DNDEBUG, four build jobs.
OneAPI uses the pinned official DPC++7.2 compiler5680988bb38bd45f13081394fba43588c5f5e395,
archive385586a25e75ac727883f0be5e442dec0ee81b3c20f9a29037a79921ad2f1728 and
unchanged separate libumf compatibility provenance. Complete FLTK/OpenXR builds
succeed; no old-header/archive ABI combination was used.

## Release and XR

CPU88/88 non-XR suites pass177.13s with-j2. Log tests-resolution-production-native.log.
This includes new Volume4/band/resolution/quality/allocations/raycast/worker/lease
checks, all existing source/paint/UV/skin/morph/VRM/cut/geometry regressions and
software GL fixtures. Full actual CC0female/male VRM variants include the new
immutable original-bind regression. Suite wall times are not frame/stroke benchmarks.

Both actual-loader tests pass in273.14s: Resolution17/17 in27.17s, main150/150
in245.97s. Log tests-resolution-production-xr.log and -xr-detail.log. Main report
build/xr-client/report.json has complete=true,failed=0,status=passed. Resolution
build/resolution-xr/report.json has17 checks/failed=0; that harness does not define
complete, so terminalCTest success closes its run. Private HOME/runtime/projects,
synthetic tracking and real GL swapchains exercise actual native joystick/ray/
pressure, source saves, physical band, cancellation/focus/history, original140
checks plus10cut checks and17resolution checks. Original per-client bounds remain.

Eligible compute uses CPU here, and GL transport uses llvmpipe; these are not
physical headset/controller/stylus,3D-mouse or production-pacing results.

## Actual Arc OneAPI

Complete current OneAPI frontend passes25 selected native suites13.35s. Log
 tests-resolution-production-oneapi-native.log identifies the exact selection:
root17 plus raycast, resolutionallocation/values/progress/document/controller/
resampling/authenticated-cache cases. CPU quality/resampling remains CPU work;
eligible source construction/brush math executes actual Arc, without CPUfallback.
The existing oracle retains184 independent endpoints,224 reference batches/
445088 samples, maxerror8.9407e-08, chunk/concurrency and explicit no-GPU rejection.
A final standalone actualArc run retains its exact device/numeric output in
tests-resolution-production-oneapi-oracle.log (exit0); no pacing claim follows.

Both real-loader runs pass297.61s: Resolution17/17 in29.41s and main150/150 in268.19s.
Logs tests-resolution-production-oneapi-xr.log and -xr-detail.log; corresponding
build-oneapi/{resolution-xr,xr-client}/report.json reports17/150,failed=0, with main
complete=true. Compute is actualArcA770/LevelZero; tracking and GL transport are
synthetic/software. This is not a GPU resampling or physical device claim.

Separate direct EGL fixtures explicitly require hardware and print actual Mesa
Intel ArcA770/DG2. All pass, with log suffix after tests-resolution-production-oneapi-:

| Fixture | Assertions | Suffix |
| --- | ---: | --- |
| Actual Document→GL |22| document-arc.log |
| Static/scalar VBO/UV1/error recovery |24| buffers-arc.log |
| PBR/MToon/linear/clip/outline |81| mtoon-arc.log |
| Mask/symmetry overlay |38| masks-arc.log |
| Material visibility/picking |25| visibility-arc.log |

Actual glBufferData traffic and pixels prove unchanged mask/material/paint paths
reuse geometry, genuine field/transform/cap changes refresh it, second eye/clip-only
reuse and exact cancellation work. Full packed assembly/uploads for geometry
changes still remain; no tile-GPU/frame-rate/full-production claim follows.

## Whole-core sanitizers and VRM regression

Fresh C/C++ core (including xatlas/Mikk), render and matching consumers compile
-O1/-g1/-DNDEBUG withASan/UBSan/frame-pointers. ASAN_OPTIONS=detect_leaks=1:halt_on_error=1,
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1. All15 focused CTests pass121.31s:
cutauthoring/cancellation, raycast, resolutionallocation/values/progress/document/
controller/resampling/authenticated-cache, maskcontroller/document, six-worker
shutdown, authenticatedlocalmeshing and actualDocument→GL. Native UI is off;
direct EGL render is enabled. Logs build-resolution-production-sanitize.log,
tests-resolution-production-sanitize.log and -sanitize-detail.log.

Then the fast VRMmetadata program passes all13 assertions under the same whole-core
sanitizers/leak settings: tests-resolution-production-vrm-metadata-sanitize.log.
The broad avatar female/male path is separately in the full Release run.

Independent old-code regression: new testcompiled against its matching a4 source
headers and frozen coherent cf89/a4 Release archive exits1 after the original3
passing assertions: expression target outside source table. No resolution/Volume4
headers were mixed into that old archive check. Logs
build-vrm-original-bind-old-regression.log /tests-vrm-original-bind-old-regression.log.
With the fix, two emitted meshes use maps{1,2} and{0,1} for originaltarget1,
producing2,1 while originalmetadata stays exact. Every emitted bind starts from
immutable source_bind; a prior output cannot mutate the next input index.

## Native GUI/compositor and Unicode ordering

All151 primary assertions pass across private displays/buses/projects. Normal
and compact resolution screenshots were visually inspected. Optional orchestration
uses existing cached uv withdbus-next/python-xlib/Pillow, not product dependencies.
Use explicit canonical lib64 runtime; no system Python package install occurred.
Actual DomeD tests use frozen canonical282 compositor binary; teardown GVFS/bus
messages do not change successful terminal statuses or reports.

| Workflow | Assertions | Log suffix after tests-resolution-production- |
| --- | ---: | --- |
| FLTK resolution |12| menu-resolution.log |
| Compact2×BDF resolution |4| menu-resolution-compact-final.log |
| General menu |53| menu-general.log |
| Cuts normal/compact |12/6| menu-cut.log /menu-cut-compact.log |
| Masks normal/compact |17/5| menu-mask.log /menu-mask-compact.log |
| Native skin |11| menu-skin.log |
| Exact Unicode/native clipboard/history/save |4| menu-unicode-final.log |
| Actual DomeD resolution |10| wayland-resolution.log |
| Actual DomeD masks |9| wayland-mask.log |
| Actual DomeD general/history/Window |8| wayland-general.log |

Actual compositor resolution proof uses Super+F10/header/row navigation to native
controls, modal identity/availability, ReturnCancel, universal textCopy/FileClose,
Halfspacing/Apply, saved20mmVolume4 with original.16m band/layer metadata, exact
singleUndo/Redo andCtrl+W cancellation retaining the app/source. Native compact
controls fit640×360 with2×BDF and reachable scrolling/Cancel/Apply.

One initial compact invocation used the wrong test-only environment variable and
ran normal12checks again; only the later correctly configured4-check compact run
above is counted. An inherited Unicode fixture failed at exact clipboard equality:
keymap lifetime alone did not order X11 typing versus menu D-Bus callbacks. The
new harness copies via the SAME native keyboard queue, waits for new clipboard
ownership and exactUTF8data, then exercises universalSelectAll/Copy/Cut/Paste.
It keeps all original expectedbytes/model/history/save assertions. A prior ordered
normal run and final current normal run pass4; three fresh optional burst runs
(no inter-key sleep) also pass4 each. Logs menu-unicode-ordered.log,
menu-unicode-burst-{1,2,3}.log retain the stress scope; the original failure log
menu-unicode.log is retained. No product Unicode code or expected data was relaxed.

## Scope and continuation

Resolution layer/topology/quality, physicalband and source preservation are bounded
CPU functionality. Fine rays still need conservative empty/candidate support skip;
production highresolution/sustained strokes, paged storage/seventh-worker/frame
integration, COLOR_0 and full required import/export/device/accelerator work remain
open. New discoveries about history-publication allocation staging are being
hardened in the private desktop-worker branch; current tests do not prove every
possible global Document allocation boundary.

Only old8f4d67d has the user's rudimentary physical WiVRn confirmation; that test
was ended. No new HMD test/system install/session restart/AMD-CUDAexecution or
full-goal completion is claimed. Keep the full requirements matrix and all exact
old proof scopes intact while committing/pushing subsequent verified milestones.
