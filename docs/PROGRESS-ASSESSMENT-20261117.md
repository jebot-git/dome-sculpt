# Dome Sculpt progress assessment — updated 2026-11-18

The full production objective is active and incomplete. The project has a
functional native FLTK/OpenXR authoring baseline; recent work strengthens its
combined build, source-aware baking and actual application qualification. This
assessment does not replace the complete [requirements](REQUIREMENTS.md).

## Qualified baseline and combined successor

Main now admits product1bc13125 and public fixturef1e193c7; read
[the current integration record](NORMAL-RUNTIME-ADMISSION-20261117.md).
The earlier3d164382 baseline remains separately qualified. The suite includes
sculpt layers/masks/symmetry, polygon and UV editing,
layered paint/projection, rig/weight/morph tools, import/export subsets, ordered
saves, recovery and edit history. These are implemented workflow subsets, not
proof of the complete production requirements.

The frozen090f successor (product1bc13125) completes its optimized Release
owning build and90 native suites. Its fresh application also passes64 public
desktop checks and260 authenticated Arc synthetic-XR checks, with explicit
fixturef1/runtime6c8 source roles. Native Wayland through current canonical
DomeD passes14 controls, canvas and GPU checks. See
[whole Release](NATIVE90-RELEASE-OWNING-20261117.md),
[native90](NATIVE90-RELEASE-ACTUAL-20261117.md),
[public application](WHOLE090F-PUBLIC-UI64-XR260-20261117.md) and
[native Wayland](WHOLE090F-WAYLAND-GPU-ACTUAL-20261117.md).

The same whole sanitizer build completes successfully with1036 owners and1776
outputs; its strict native90 run and version/workspace/raw checks also finish0,
with zero input drift and no survivors. See
[owning qualification](NATIVE90-SANITIZE-OWNING-20261117.md) and
[runtime qualification](NATIVE90-SANITIZE-ACTUAL-20261117.md).
Main now merges the qualified successor. The owning source remains090f, while
the separately exercised public fixture isf1; no whole-f1 build is claimed.
Synthetic XR and nested diagnostic Wayland presentation do not establish
physical headset or native DRM acceptance.

## Further production work in progress

The advanced normal Bake adapter102f0af4 supports genuine Rasterize and
RetainSource normal sources. Its16-case native component matrix passes249415
checks plus prepared91/artist139. Read
[the scoped proof](ARTIST-ADVANCED-NATIVE-20261117.md).
The separate prospective ancestor-budget child90fad800 with fixtureaa8db16a
passes17 cases/263495 checks, prepared91 and artist139; read
[its sealed native proof](ARTIST-ADVANCED-CHAIN-NATIVE-20261117.md).
The strict component runtime now also passes17/91/139 with leak detection,
actual ASan/UBSan mappings and zero drift/survivors. The corrected public native
fixture now passes eight source-use and eight UV-creation cases with128 positive
endpoint checks and32 expected negative refusals. All eight actual source-use
desktop workflows pass128 checks across sixteen fresh Apps, including Save,
Undo/Redo and reopen; see
[the desktop matrix](ADVANCED-PUBLIC-SOURCE-GUI-MATRIX-20261117.md).
These newer production changes are not integrated into main. Their source-use
XR, UV-creation desktop/XR, graphical and combined qualification remain pending.

The advanced graphical test's first software run agrees on correct PBR colours
but fails required contrast against omitted normals. A calibrated CPU forecast
finds additional genuine PBR witness points; MToon needs further presentation
and graphical calibration. This is an unresolved verification issue, not an
advanced graphics pass; see
[the preserved results](ADVANCED-NORMAL-CONTRAST-ACTUAL-20261117.md).

A canonical normal-field source adapter is implemented on a separate branch.
Compilation stopped after28 successful source files because of a missing include.
The include-only correction is pushed, and the25-file continuation is reviewed;
native32, sanitizer and application validation remain pending. Other unsupported
source families remain explicit refusals pending their actual adapters.

The user-accepted controller selection/hierarchy/multiselection/ghost interface
is preserved. Only the verified CC0 VRoid female/male creator bases are offered.
Current canonical DomeD FLTK and bitmap-font builds remain selected; see
[toolkit checks](CURRENT-TOOLKIT-20261117.md).

## Main remaining goals

1. Qualify the newly integrated combined candidate in the physical
   headset/controller workflow and finish the next baking adapters' own
   sanitizer, graphical and public application qualification.
2. Complete production-size sustained sculpting, local meshing, allocation and
   recovery hardening, topology/paint preservation, remeshing and retopology.
3. Finish texture sets, broader layered paint/normal/AO/material baking and
   source/UV/deformation handling, with GPU execution where viable. ArmorPaint
   remains assessed rather than integrated as a hosted painting engine.
4. Complete humanoid proportions/face/hair authoring, imported-rig rest/reparent
   transport and reliable automatic weights across diverse deformation cases.
5. Complete required glTF/VRM expressions, physics, constraints, metadata and
   source-preserving interoperability, plus broader FBX/OBJ handling.
6. Finish all required authoring entirely in XR; verify physical controllers,
   stylus, trackball/3D mouse, reconnect/session loss and sustained performance.
7. Extend accelerated workloads and cross-backend verification. Positive Arc
   execution exists; CUDA/ROCm compilation and explicit missing-device failure
   do not establish positive NVIDIA/AMD hardware execution.
8. Implement controller tool/function/selection trays and spatial node-based
   editing/history after the basic production targets are met.

The user resumed the full goal on2026-11-18. Three agents are continuing the
canonical adapter, advanced graphical verification and public desktop/XR work.
No percentage is assigned: implemented rows differ greatly in scope and several
still have only partial qualification. No system installation, physical-headset
test or active-session restart occurred during this round.
