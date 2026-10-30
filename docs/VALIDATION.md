# Combined native gestures and retained frames — 2026-10-30

Product `233e586` combines published paging `2a83eca` with retained frames,
existing-bake preview/capacity fixes and the ordered seventh native stroke worker.
Publication adds test-only XR menu acknowledgement and CUDA qualification;
all product C++ and public headers stay identical to233e586. CUDA qualification
source479a348 changes CMake/tests and optional SDK preparation, not brush math.
Prior exact proof is archived in VALIDATION-2a83eca.md and earlier versions.
The original production goal remains active and incomplete.

| Combined scope | Result |
| --- | --- |
| CPU optimized full native/UI/render | 103/103,248.68s |
| CPU real-loader synthetic XR | main150+resolution17,2/2,298.26s |
| Whole C/C++ core/xatlas/Mikk/UI/render ASan/UBSan/leaks | 103/103,598.19s,explicit exit0 |
| OneAPI selected native | 31/31,14.52s |
| OneAPI real-loader synthetic XR | main150+resolution17,explicit exit0 |
| Actual Intel Arc GL | 11 fixtures, counts below,all exit0 |
| Native FLTK/actual DomeD Wayland GUI | 216 assertions,21 final flows |
| CUDA13.4.1 optimized native FLTK/OpenXR/alltargets | complete build,exit0 |
| CUDA missing-device dispatch | CTest1/1,exit0; host samples byte-identical |
| CUDA positive oracle without eligible GPU/driver | expected exit1, explicit insufficient-driver error |

Hardware GL counts: retainedframes47,UVtransfer22,independentPBRconsumer25,
actual19-partVRMconsumer23,ordinarycapacity19,MToon81,emission43,masks38,
visibility25,bufferreuse24,Documentbuffers22. Every direct fixture checks hardware
and reports Mesa Intel Arc A770/DG2. Tracking and GL in the XR loader suites are
synthetic/software; eligible compute still uses the selected Arc backend.
The compute oracle covers184 independent endpoints,224 CPU-reference batches/
445088 samples and2000007-sample chunk/reuse/concurrency checks. No physical
controller/headset/stylus or device-pacing evidence is inferred from these runs.

GUI flows include canonical menu53,nativeCancel/SaveClose/posedPaint/failure3each,
masks17+compact5,resolution12+compact4,cuts12+compact6,skin11,Unicode4,
actualDomeD general8/mask9/resolution10,VRMexport5,rig14,retopo12,bake19/cancel3.
The first PAINT-only run exercised cancellation because ACCEPT was absent.
Its output is retained with-cancel-initial; the corrected PAINT+ACCEPT run proves
actual saved UV-layer changes with exact rest/skin/morph/source preservation.
The stale cancellation screenshot was removed from the later output only after
verifying a byte-identical backup. Compact resolution and initial cancellation
images were visually inspected. The old off-centre ray-error GUI attempt is not
proof of actual exhaustion status; the separate picking task supplies corrected
centred status-observer coverage for later integration.

The first sanitizer build wrapper returned143 after reporting build completion,
before CTest started. No signal source is established. Its original log/status
are retained; a separate no-change build verification exits0. A fresh separately
status-recorded CTest process then passes the complete103-suite scope above.
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 and UBSAN_OPTIONS=halt_on_error=1;
C and C++ use-O1/-g1/-DNDEBUG with matching instrumented archives. No suppression
or excluded failing native suite is used. See local final-status JSON.

The earlier40ea548 OneAPI run failed a two-frame stylus menu visibility window;
CPU passed. Test-only4943af9 waits for an actual released-eye palette image before
the closing click and keeps monotonically advancing physical time. The original
failure remains recorded. Full unchanged40ea and combined233 products then pass
all150 checks; the current combined fixture is built from publication680c374.
All original visibility/ownership/edit/save assertions remain. See
XR-MENU-ACKNOWLEDGEMENT.md for the bounded logical/physical script distinction.

CUDA toolkit components are staged as the regular user, with all five official
archive SHA256/size checks and licenses retained. Repeating preparation matches
all2436 toolkit file hashes. NVCC13.4.59/GCC16.2.1 compile C++20,compute86+sm86,
without an unsupported-compiler override. The test masks devices, checks the
runtime's availability result and invokes a real nonempty backend dispatch;
silent CPU success fails. No NVIDIA device execution or numeric parity is
claimed. ROCm remains compile-only as previously recorded. Both still require
hardware proof and broader eligible workload acceleration.

Latest remote DomeD282467e/FLTK1.4.5+22 patches is used explicitly via share/fltk
and lib64. Actual linkage is checked including the inherited-library-free case.
All Release builds are regular-user-O3/-DNDEBUG. Frozen DomeD compositor SHA is
recorded with the manifest. No system installation, desktop/display-manager
restart or physical test launch occurred.

Local ignored artifact manifest: artifacts/paged-frames-strokes-final-manifest.json.
It records source/binary/log/report hashes, exact compiler flags, completed
process statuses, the corrected GUI setup and retained initial failures.
Product build worktree: artifacts/paged-frames-strokes; publication and CUDA
qualification are separate checkouts with explicit unchanged-product provenance.
The full sources and reproduction methods are tracked; logs/assets generated by
validation remain local rather than being assumed present in a fresh clone.
