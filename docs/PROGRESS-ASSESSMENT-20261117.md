# Dome Sculpt progress assessment — updated 2026-11-18

The full production objective is active after the latest user resume and remains
incomplete. The project has a functional native FLTK/OpenXR authoring baseline;
recent work strengthens its
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
All eight authenticated Arc synthetic-XR source-use cases now pass520 checks,
and all eight desktop UV-creation cases pass112 checks. Read
[source-use XR](ADVANCED-PUBLIC-SOURCE-XR-MATRIX-20261118.md) and
[UV creation](ADVANCED-PUBLIC-UV-GUI-MATRIX-20261118.md).
These newer production changes are not integrated into main. UV-creation XR
passes all eight cases/208 checks using the observation-only child that preserves
the actual input, render-skip and tracking-loss stream; read
[the XR matrix](ADVANCED-PUBLIC-UV-XR-MATRIX-V2-20261118.md).

The advanced graphical test's first software run agrees on correct PBR colours
but fails required contrast against omitted normals. Declared display-only
presentation and genuine interior probes address contrast. A later MToon
material-control mismatch was traced to incorrectly dropped generic source
colour metadata and corrected without changing production images or thresholds.
The next actual run exposes real between-texel normal-detail loss when retained
source fields are baked into a compressed64-pixel target atlas. Fidelity checking
and a genuine explicit higher-resolution workflow are now required; the original
low-resolution failures remain. This is not an advanced graphics pass; see
[the preserved results](ADVANCED-NORMAL-CONTRAST-ACTUAL-20261117.md).

A canonical normal-field source adapter is implemented separately and passes
all32 native cases/525092 checks, including both normal roles, UV/key/pose,
field replacement/Clear, physical emission, history and ordered Save. The four
fixture/core component passes preserve both earlier compile and test failures;
read [canonical source proof](ARTIST-CANONICAL-NORMAL-NATIVE-20261118.md).
Native32/canonical writable-paint and joint source-colour/normal SOURCE adapters
are implemented on a further isolated child. All six Release component suites
pass, including64 writable cases/619634 checks and8 joint cases/346305 checks,
with121 exact ordered Save pairs. Read
[the actual component result](ARTIST-WRITABLE-JOINT-NATIVE-20261118.md).
Their fresh strict sanitizer gate now also passes all six native suites:
1754756 checks and121 exact ordered Save pairs. Root independently verifies
the evidence, actual214 object/MD pairs and all saved pairs. Read
[the scoped sanitizer proof](SOURCE441-STRICT-SANITIZE-20261118.md).
New-family graphics and combined application qualification remain pending.

The coherent canonical/public candidate8a8ecea1 completes a fresh optimized
application build:242 actual compiler dependency owners including all193 core
members,260 build steps, ordinary loader72 and version0. Read
[the exact build scope](CANONICAL-PUBLIC-RELEASE-APP-20261118.md).
Its production files match canonical85. One actual fresh-App desktop16 and
matching Arc syntheticXR65 canary also pass; read
[the precise source roles](CANONICAL-WHOLE-APP-SOURCE-CANARY-20261118.md).
The fresh strict sanitizer core/four-fixture gate also passes all four native
suites, including32 canonical cases, with leak detection and live runtime
witnesses; read [the sanitizer proof](CANONICAL-NORMAL-STRICT-SANITIZE-20261118.md).
These are distinct scoped results, not all-target or newer-product441 acceptance.

The newer isolated combined draftab497007 merges SOURCE441, filtered-quality8eba
and creator75 through actual history. Fresh configuration passes and the whole
App target now completes with244 fresh compiler owners/195 core members,
actual dependency/archive/link audit and ordinary loader72/version0; see
[the combined build result](COMBINED-AUTHORING-APP-BUILD-20261118.md).
This draft is not a new admitted application. A separate pushed63956 child adds
bounded2048 resizing and the creator reporter fix; its fresh App/seven-fixture
build and audit now also pass251 owners/195 core members. Read
[the selected compilation proof](COMBINED-CHECKED-APP-FIXTURES-BUILD-20261118.md).
Its combined strict sanitizer build/audit now passes217 owners/195 core members.
Selected canonical512/writable512 strict runs pass; the old joint helper reaches
its unchanged900s deadline. A helper-only immutable-map correction preserves all
samples and bounds, and its separate strict joint64 plus creator44 run passes.
The four distinct positive suites total67108 checks, with live ASan/UBSan and
leak detection; see [the exact scope](COMBINED-STRICT-SANITIZE-20261118.md).
This is not full combined104/legacy121 or new-domain acceptance.

The filtered normal workflow now passes the full17-family native current cohort
at explicit2048, with source64 unchanged,646 current checks,263495 historical
checks and55 normalized-sampler checks. It retains the correct64/1024 refusals,
all history and complete Save/reopen comparisons. A valid RGBA8 interpolation
rounded above1; correcting its producer preserves strict downstream validation.
Read [native qualification](ADVANCED-NORMAL-FILTERED-WORKFLOW-20261118.md).
The calibrated CPU forecast now passes68 views/37968 dense points; software
OpenGL passes68 views/432 probes/1796 images with maximum correct RGB error1.
The same Arc product reports all3721 checks passed and the complete independent
saved-evidence audit now passes, preserving the original wrapper's renderer-name
parser failure. Root independently verifies the complete scoped Arc proof. These component
results do not qualify the new combined App or a physical headset.

The independent owning HDR source preserves its source64 FLOAT grid while paint
uses512. Native Save/reopen, ordered byte equality, history, transformed owner
controls and dense fidelity checks pass. GPU productc524/fixture212 now also
passes independent PBR/MToon preview checks before/after current512 paint:
source coefficients are exact, max RGB error1 under bound2. Read
[GPU qualification](HDR-INDEPENDENT-GPU-20261118.md).
Actual GPU hardware and full combined/public workflow remain pending.

SOURCE8 and retained HDR export60a1 now preserves the complete visible source
and target, separate64/512 coefficient witnesses and native Scene43 provenance.
Native16400 controls and23707 independent probes pass under unchanged2e-4
(error2.513e-6); direct/second/worker GLBs are byte-identical and independently
pass Khronos with0 errors. Broader/mixed-source guards and current strict
sanitizer runtime remain pending; this is component evidence, not admission.

VRoid creator productb75/fixturede60 now passes448 VRM,44 workspace,352 controls
and1 fixture checks, including all six female/male styled/reopened/posed exports
and real stale/foreign/forged refusals. All six exports pass official schemas
and Khronos with0 errors. An unrelated empty-object fixture dereference and
an outer fixture-count expectation were corrected in explicitly scoped evidence;
original failures remain. Read [native VRM qualification](CREATOR-NATIVE-VRM-20261118.md).
Current strict and actual desktop/XR UI qualification remain pending.

The user-accepted controller selection/hierarchy/multiselection/ghost interface
is preserved. Only the verified CC0 VRoid female/male creator bases are offered.
Current canonical DomeD FLTK and bitmap-font builds remain selected; see
[toolkit checks](CURRENT-TOOLKIT-20261117.md).

## Main remaining goals

1. Complete the fresh build and qualification of composed candidatef5998a18,
   including current HDR GPU/export, creator VRM and normal fidelity components.
   Finish broader regression, strict sanitizer, actual GPU hardware, public
   desktop/XR and physical headset/controller gates. Component passes and old
   combined builds do not admit this newly composed application.
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

The latest user resume revokes the previous round's pause. Current GPU,
export, creator and normal-quality components are merged into frozen candidate
f5998a18; its fresh App/13-fixture Release build and actual audit now pass
259 owners/197 core members. Loader and combined runtime gates remain pending. The component proof
links and exact pending gates are in the current handoff.
No percentage is assigned: implemented rows differ greatly in scope and several
still have only partial qualification. No system installation, physical-headset
test or active-session restart occurred during this round.
