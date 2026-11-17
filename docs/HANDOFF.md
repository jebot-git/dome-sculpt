# Dome Sculpt current handoff — 2026-11-18

The full production goal is ACTIVE after the user's resume. It is incomplete.
Read [REQUIREMENTS.md](REQUIREMENTS.md) and the original
[HANDOFF-466ebd1.md](HANDOFF-466ebd1.md) for the complete scope. This concise file
replaces the accumulated checkpoint diary; all previous text is preserved,
byte-for-byte, in [the historical handoff](HANDOFF-HISTORY-20261117.md).
Update this current summary at milestones rather than prepending more history.

## Source and integration boundary

- Main now admits product1bc13125 and public fixturef1e193c7 after complete
  Release and strict sanitizer qualification. Read
  [the current admission](NORMAL-RUNTIME-ADMISSION-20261117.md) for exact
  owning-source/test/runtime roles. The newer advanced90fad product remains
  separate and is not admitted by this milestone.
- The earlier3d164382 baseline with the explicitly exercised
  c54 test reporter/02d coordinator remains separately qualified. Read
  [baseline admission](GUARD87-RUNTIME-ADMISSION-20261115.md): Release87 and strict
  sanitizer87, desktop208, syntheticXR776 and separately qualified software/Arc.
- The successor normal-source product1bc13125 has native/sanitizer and
  software/Arc component evidence. Read
  [the normal-source qualification](ARTIST-NORMAL-GRAPHICS-20261115.md).
- Frozen combined candidate090f009ede2c0623ca616fe892223b4f68f1c2c4 is pushed on
  work/authoring-normal-integration-20261115. Its worktree is
  artifacts/authoring-normal-integration-20261115. It merges main302e847d and
  test source6c8 through real merges; production remains exact1bc.
  Do not edit this source during its build/audits.
- A fresh App1bc component passes loader/version and the existing52-check GUI
  regression. This is separate from the full combined build and new workflow.
  Read [the GUI proof](APP1BC-SOURCE8-GUI52-PASSED-20261115.md).

## Build results and running jobs: inspect before acting

Authoritative state is the current service/process and its output, not this
snapshot. Never restart a job merely because an observation handle times out.

1. Release owning build: ds-native90-release-build-20261117.service,
   MainPID1978514, InvocationIDf18eb32adee84544b99a1c6404d1d403.
   Plan: artifacts/native90-release-planning-20261115/
   release-090f00-20261117-audit/plan.json, SHAccf9931035481459d7f51ec03e829ee2572e424d381d2bbc69a29c1fe26e1661.
   Status is execution-native90-v1/status.json under that audit directory.
   Actual build and final audit terminate0 in799.133s:291 fresh compilations,
  745 verified independent whole objects,737 fresh links,3 generators,1776
   outputs and1036 dependency owners/434920 incidences with0errors/drift[].
   Status SHA3c4fd01b2a6702ee0069a744375b9d4a087ef4d8ed6a7faff829d22fe49be36c.
   The unit is independently captured as successful and stopped. No reused
   object is called newly compiled; old/new compiler records remain separate.
   Native90 and both whole-App public desktop64/syntheticXR260 matrices now
   pass; see NATIVE90-RELEASE-ACTUAL-20261117.md and
   WHOLE090F-PUBLIC-UI64-XR260-20261117.md for exact source roles.
2. Strict sanitizer build: ds-native90-sanitize-build-v2-20261117.service,
   InvocationIDf2bcbf89dda84a1dae2590e06d016dd4. Plan-v2 SHA
  62116c27b70b37459d186e037acc51984d34fac8262760e5f50351ac8c21f8d9.
   Status is artifacts/native90-sanitize-planning-20261115/
   sanitize-090f00-20261117-audit/execution-native90-v2/status.json.
   Prepare-only was actual0. First executor launch stopped before staging:
   its policy incorrectly rejected two existing optimized CPU/FMA test owners.
   The [v2 correction](SAN90-EXECUTOR-V2-20261117.md) preserves exactly those
   two O3/march=native owners and O1 for the other1034, with ASan/UBSan and
   frame-pointer instrumentation for ALL1036. Source/CMake/commands are unchanged;
   original failure and expected pins remain preserved. The v2 owning build
   now terminates0 in5645.490s with1036 owners/1776 outputs, zero drift and
   no survivors; read NATIVE90-SANITIZE-OWNING-20261117.md. Its separately
   reviewed strict native90 run also passes ALL90 suites plus version,
   workspace27/373 and raw90159, with final drift[]/owned[]/released lease.
   Actual unit2934532/Invocation7d8e087a completed0 and was stopped only after
   root captured terminal properties and journal. Read
   NATIVE90-SANITIZE-ACTUAL-20261117.md for the scoped runtime proof.


Builds use regular-user bounded units, two compiler jobs and8GiB aggregate RSS.
No system installation, physical-headset launch or session restart occurred.

## New prepared-source public workflow

Initial6c8 fixture/runtime/font complete TUs and three links pass. Native91 also
passes, but the new capsule producer fails before UI: it compares PaintedMesh
wrapper pointers across Scene::snapshot(), which intentionally detaches wrappers.
The original failure is preserved under
artifacts/artist-prepared-public-link-v3-20261115/native-public-v3-20261117-v1.

Test-only childf1e193c7563eacc4db1d40518235450975460b15 captures the pointer baseline
inside the detached scene before Bake, additionally retaining full encoded input
and original-scene checks. Production is unchanged. Its complete fixture TU and
fresh link pass; runtime/font/scenarios remain explicitly6c8. Native capsule/
font qualification is now actual0, with all47 commands,4 genuine cases,
32 positive phases,8 wrong-Original refusals and all bitmap checks passing.
Read [the native seal](ARTIST-PUBLIC-NATIVE-20261117.md) and actor artifact namespace
artifacts/artist-public-snapshot-link-20261117; do not blindly rerun old failures.

The earlier component App passes desktop64/Arc syntheticXR260 in its explicitly
recorded3d context. The fresh whole090f App now ALSO passes desktop64 and
Arc syntheticXR260, with ordinary loader/version0, all7 critical ELF roles,
actual Arc PCI03/renderD129/i915/liveFD, exact source/normal/Save/history/reopen
checks, unchanged time/frame bounds,0drift/noowned and stopped units. Read
[whole-App qualification](WHOLE090F-PUBLIC-UI64-XR260-20261117.md). The two
matrices are distinct, not relabeled. Both use fixturef1 and runtime/font/
scenario6c8; no whole-f1 owning build is claimed.

The fresh wholeApp also passes14 actual native Wayland/DomeD checks on Arc:
application GL and canonical compositor Vulkan, visible clay canvas, real
universal menu/text/modal commands, desktop-side action, Undo and normal close.
Read [native Wayland GPU proof](WHOLE090F-WAYLAND-GPU-ACTUAL-20261117.md).
The canonical compositoracc build is optimized and uses the current SDK/private
bitmap stack; Cargo may reuse valid dependency artifacts. Private Xvfb presents
the nested compositor, which has a diagnostic copy. This is not native DRM
scanout/zero-copy or physical-headset acceptance. Failed staging/display/observer
attempts remain preserved separately. The test-only observational EGL hook
queries the actual context and changes no application rendering.

Operational drivers are in artifacts/prepared-public-operational-20261117 and
artifacts/whole090f-wayland-20261117. Use the common graphics lease before ldd/
products/GL and release only after owned-process cleanup. The whole SAN runtime
is now qualified separately above. New advanced-family products still require
their own reviewed component gates.

Combined native90 runner is prepared in artifacts/native90-release-gates-20261117;
its exact driver SHA is55f06be3963aaff1726962b224d7a534015ea89b179cb13e9e935dfe4744f057.
It now passes ALL90 actual native suites plus app-version/workspace27/373/
raw90159, with full posthash0drift/noowned and successful stopped user unit.
Actual status SHA8f496335c8b92cedc8887a9a53112d61c798ab20ecb465b37dbc853ceeddaaf2.
The native run uses the genuine whole090f build. Whole-App GUI/XR and native
Wayland evidence are recorded separately above. Use the common lease
/tmp/dome-sculpt-arc-device.lock before ldd/products/GL and release only after
owned cleanup. Original87/SAN/runtime failures remain honest separate records.

A further coherent source branch work/authoring-public-integration-20261117
(commit5605520b) merges the test-onlyf1 correction
with090f without modifying the frozen source. Product/tests/build
files matchf1 exactly. No whole-build560 claim follows from this source merge.
The separate pushed admission draft637ea1f4 on
work/normal-runtime-admission-20261117 merges that source with main771031f8;
read NORMAL-RUNTIME-ADMISSION-DRAFT-20261117.md in that worktree. Its strict
sanitizer condition is now satisfied; main merges this history in the current
qualified admission. No whole-f1 owning build is claimed.

## Next production slice

Agent artist_normal_completion is implementing owning Rasterize/RetainSource
normal Bake-source adapters in a separate branch, preserving source domains,
PBR/MToon roles, frame/pose transport and exact normal-event/material pairing.
Checkpoint102f0af4 now passes all16 actual native combinations with249415
checks, plus prepared91 and artist139; read
[the component proof](ARTIST-ADVANCED-NATIVE-20261117.md). Its separate child
90fad800 bounds the full owning ancestor replay graph before deep validation;
fixtureaa8db16a passes17 native cases/263495 checks plus prepared91/artist139,
including chained edits, every observed traversal cancellation boundary, cycle
refusal and atomic worker/Save quota refusal. Read
[the sealed child proof](ARTIST-ADVANCED-CHAIN-NATIVE-20261117.md);
native success is not strict sanitizer, graphics
or whole-App admission. Genuine first-Bake fixture setup
and stale-event negatives were corrected without weakening source guards;
original failures remain preserved. Canonical/native32/independent writable-paint
domains remain explicit refusals pending their own actual adapters.

All six new strict component TUs and three fresh links pass in
artifacts/artist-advanced-normal-sanitize-20261117. The separate native-v2
runtime now also completes0: advanced17/263495, prepared91 and artist139, with
live ASan/UBSan witnesses for each ELF, leak detection, all17 exact ordered-save
pairs, drift[]/owned[]/released lease. Status SHA
f8e9c649ebf0a324be9e3ba7ef48693eb662cf3fedcdb4281d47f694733dad03.
Root captured actual unit3089942/f021bca5 success before stopping it; the scoped
proof is integrated in main0094ee24. Graphics/whole-App admission remains
outstanding for advanced90fad.

Public source checkpoint8bb4072d adds separate source-use and UV-creation
desktop/XR scenarios. Its two complete Release test TUs and three fresh links
passed. The first retained-source producer exposed four geometry-only calls to
context-free PaintedMesh encoding, which requires an owning sampling archive.
Fixture-only child27ff940b compares the actual geometry directly instead; the
original failures remain archived. Its fresh native TU/link and native-v3 gates
pass: advanced17/263495, seventeen ordered-save pairs, eight source-use and eight
UV-creation cases, 128 positive verifications, 32 expected negative refusals and
bitmap checks. Read [the native proof](ADVANCED-PUBLIC-NORMAL-NATIVE-V3-20261117.md).

All eight actual desktop source-use cases pass128 checks in sixteen fresh Apps,
including real source/normal controls, second Bake, Save, Undo/Redo and reopen.
The normal-error maximum is below .00510 against .035; wrong-domain/omitted/
swapped controls retain .05 sensitivity. Read
[the desktop matrix](ADVANCED-PUBLIC-SOURCE-GUI-MATRIX-20261117.md).
These are explicitly product90fad/fixture27ff/runtime-and-scenarios8bb, unchanged
090f UI/support and font6c8. They do not admit advanced production into main.
All eight source-use syntheticXR cases pass65 checks each on authenticated Arc,
total520. All eight separate desktop UV-creation cases pass112 checks. Read
[source-use XR](ADVANCED-PUBLIC-SOURCE-XR-MATRIX-20261118.md) and
[desktop UV creation](ADVANCED-PUBLIC-UV-GUI-MATRIX-20261118.md).
UV-creation XR first fails a test reporter/capture contract: intended pressure
was logged for unused float actions, and captures14/27 conflict with actual
render skip/tracking loss. Test-only childb7d6ff17 records returned pressure and
observes13/29 while preserving all action timings and original render/tracking
events. Its full runtime TU/fresh DSO pass; all-eight rerun remains pending.
Operational statuses remain under artifacts/advanced-public-normal-operational-20261117.
Each case retains the original bounds and common device lease. No
physical-headset result follows from synthetic XR.

The independent advanced renderer compiled and linked successfully, but its
first actual software run failed the negative-control contrast requirement.
Correct PBR RGB errors were0,1,0,1; omitted-normal differences3,4,1,6 never
reached7. A separate wrapper rejected a non-ELF deleted Mesa lp_dma_buf mapping.
Both failures and all images are preserved. A CPU forecast across68 views
calibrates all16 known PBR control colours within1 and finds interior PBR
witnesses. Authored MToon estimates remain insensitive and graphically
uncalibrated. Read [the actual contrast record](ADVANCED-NORMAL-CONTRAST-ACTUAL-20261117.md).
The declared display-only presentation and extra interior probes now compile
and link. Its source2c687a1a preserves genuine normal data/source ownership and
publishes through Document. The CPU forecast passes68 views and selects438
probes including all272 original probes, with predicted contrast in both roles.
The first corrected software GL fails a MToon material control:231,217,209
against196,207,197. Dropped generic-source colour metadata caused that mismatch;
the corrected ordinary control now agrees exactly, with the initial six views
passing. The next actual failure is real continuous normal-detail loss: source
RetainSource64 sampled through a compressed current atlas into target64 exceeds
.035 between texel centres, with original-probe error up to .21789. All original
low-resolution cases and probes are preserved; production fidelity checking and
an explicit higher-resolution retry workflow are in progress. Results remain under
artifacts/advanced-normal-display-owner-build-20261118/software-display-v1-20261118.
The agent is diagnosing actual material/normal transport; no advanced GL/Arc
success is claimed and no threshold or genuine normal data is weakened.

The next canonical normal-field source adapter is implemented separately at
85eafc42. Its changed private headers invalidate142 existing owners; the selected
headless slice is49 core plus4 fixtures. The first compile stopped after28
successful complete TUs on a missing HDR-authority include. The include-only
child preserves that failure; compile-v3 prepares25 remaining complete TUs and
authenticates the original28 through actual dependency files and byte-identical
child inputs. Compile-v3 completes0 with all25 fresh TUs and no drift; the193-member
core archive (49 refreshed owners,144 authenticated unchanged owners) and four
fresh fixture links also complete0. The first canonical runtime fails a test
assertion comparing a deliberately detached conversion paint wrapper with its
mutable original. Fixture-only childb11f9d42 preserves the failure and compares
actual geometry/maps/event/content and detached deformation instead. Its fresh
fixture compile/link pass. Native execution passes advanced17/263495, prepared91
and artist139 independently, then canonical32/525092 with32 ordered Save pairs.
Read [canonical qualification](ARTIST-CANONICAL-NORMAL-NATIVE-20261118.md).
Other affected consumers, sanitizer, graphics and public application
qualification remain pending. Do not reuse an
old renderer with the changed private packet layout. The32-case canonical
matrix does not implement the separate native32 writable-paint source family.
The latter and canonical writable-paint SOURCE adapters now have isolated code
atf7428248 plus source-selection child4c7eb3bb; direct native first-case sampling
passes but the whole64-case/native/public closure remains pending. Joint
source-colour/normal handling is being implemented in a separate child.

## Coherent integration build

Isolated branch work/authoring-canonical-public-integration-20261118 is pushed at
8a8ecea1. It merges current main, canonical85eafc42, public fixture27ff940b and
independent graphics2c687a1a through real merges. All765 production/build/asset
files match85 exactly; selected test/runtime roles are checked explicitly in
its integration receipt. This is source composition, not main admission.

Fresh Release configuration and the whole production application target pass in
artifacts/authoring-canonical-public-release-20261118 under
Invocation7d8c6f8b6e694690b7970cc3e097a259:242 compiler owners,93903 dependency
incidences, all193 core members and260 build steps in670.415s. Ordinary loader
exact72 and version also pass. Terminal/journal captured before stopping the
completed unit. Read [fresh build proof](CANONICAL-PUBLIC-RELEASE-APP-20261118.md).
No all-target/test or new application GUI/XR pass is claimed. Keep its source
worktree frozen, including tests and CMake files.

Fresh strict ASan/UBSan full core plus4 fixtures is now building from immutable
b11 fixture/product85 under ds-canonical-headless-sanitize-build-20261118.service,
MainPID267731/Invocation3470aba927444b6bb00cdd284a382716, in
artifacts/canonical-normal-headless-sanitize-20261118. It uses the actual previous
qualified O1/g1/instrumented CMake flags, two jobs and6GiB memory. Inspect the
actual unit/status before acting; no sanitizer runtime result is claimed yet.

## Canonical toolkit and full remaining goal

[Nov17 effective-closure checks](CURRENT-TOOLKIT-20261117.md) confirm canonical
DomeD local/remoteacc2b70 and both SDKs remain current. Use
artifacts/sdk-20261111/toolkit-{release,sanitize} and the explicit private bitmap
runtime /opt/fltk-vr/bdf-runtime. Updated unrelated /usr font-library copies are
outside the actual selected72-provider closure; do not rewrite old expectations
or rebuild solely because an unused historical/global manifest differs.

The [current progress assessment](PROGRESS-ASSESSMENT-20261117.md) describes
remaining production work. Earlier requested pauses are superseded by the
user's resume; the full goal remains active.
Complete production sculpt/local meshing/recovery, painting/texture sets/baking,
remesh/retopology/UV workflows, humanoid/rig/weight authoring, glTF/VRM coverage,
physical XR/input and accelerator execution remain required. Controller trays
and spatial node/history editing are additional required later phases.
ArmorPaint is assessed, not integrated as a hosted engine. Preserve the accepted
selection interface and CC0 VRoid-only creator bases. Passing subsets, build
completion or an integration checkpoint do not complete the goal.
