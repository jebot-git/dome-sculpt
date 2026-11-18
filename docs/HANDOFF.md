# Dome Sculpt current handoff — 2026-11-17

The full production goal is ACTIVE after the user's resume. It is incomplete.
Read [REQUIREMENTS.md](REQUIREMENTS.md) and the original
[HANDOFF-466ebd1.md](HANDOFF-466ebd1.md) for the complete scope. This concise file
replaces the accumulated checkpoint diary; all previous text is preserved,
byte-for-byte, in [the historical handoff](HANDOFF-HISTORY-20261117.md).
Update this current summary at milestones rather than prepending more history.

## Source and integration boundary

- Main's admitted production baseline is3d164382 with the explicitly exercised
  c54 test reporter/02d coordinator. Read
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
   Native90 execution waits for the public GUI/XR graphics lease handback.
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
   original failure and expected pins remain preserved. The v2 build is live;
   require actual terminal0 and separate review before any sanitizer products.


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

All4 actual desktop cases pass16 checks each/64 total. All4 authenticated Arc
syntheticXR cases pass65 each/260 total, with input/owned/unit/lease closure.
These use the App1bc trial executable in the earlier3d component context; they
do not qualify the fresh whole090f executable. A small additive whole-App
loader/cohort is being prepared for that separate application acceptance. Operational
wrapperv2 is in artifacts/prepared-public-operational-20261117, with an explicit
fixturef1/scenario6c8/runtime+font6c8/product1bc source tuple. Originalv1 remains
unexecuted. Each actual phase/case must bind the true native terminal status.
Desktop is private-Xvfb software GL; Arc syntheticXR and native Wayland/DomeD
are distinct evidence. Physical-device acceptance remains required.

Combined native90 runner is prepared in artifacts/native90-release-gates-20261117;
its exact driver SHA is55f06be3963aaff1726962b224d7a534015ea89b179cb13e9e935dfe4744f057.
It now passes ALL90 actual native suites plus app-version/workspace27/373/
raw90159, with full posthash0drift/noowned and successful stopped user unit.
Actual status SHA8f496335c8b92cedc8887a9a53112d61c798ab20ecb465b37dbc853ceeddaaf2.
The native run uses the genuine whole090f build; GUI/XR above remain component
App evidence. Use the common lease
/tmp/dome-sculpt-arc-device.lock before ldd/products/GL and release only after
owned cleanup. Original87/SAN/runtime failures remain honest separate records.

A further coherent source branch work/authoring-public-integration-20261117
(commitc9de1c7cd00a18b23ce54eba650ca6fccdd5684c) merges the test-onlyf1 correction
with090f without modifying the still-building frozen source. Product/tests/build
files matchf1 exactly. No whole-buildc9 claim follows from this source merge.

## Separate writable/joint SOURCE component checkpoint

Canonical normal-field SOURCE product85eafc42 passes canonical32/525092 plus
advanced17/prepared91/artist139; see
[the canonical proof](ARTIST-CANONICAL-NORMAL-NATIVE-20261118.md).
Its32-case matrix is separate from native32 writable paint. The owning
writable/joint SOURCE child441d9985 now passes all6 Release native suites,
including64 writable cases/619634 checks,8 joint Source8 cases/346305 checks,
and121 exact ordered Save pairs. Read
[the scoped native proof](ARTIST-WRITABLE-JOINT-NATIVE-20261118.md) for exact
source/fixture/owner roles and preserved failures. This child remains separate
from main admission: strict sanitizer441, graphics and whole-App qualification
are outstanding. Earlier product85/90fad results do not admit this child.

## Canonical toolkit and full remaining goal

[Nov17 effective-closure checks](CURRENT-TOOLKIT-20261117.md) confirm canonical
DomeD local/remoteacc2b70 and both SDKs remain current. Use
artifacts/sdk-20261111/toolkit-{release,sanitize} and the explicit private bitmap
runtime /opt/fltk-vr/bdf-runtime. Updated unrelated /usr font-library copies are
outside the actual selected72-provider closure; do not rewrite old expectations
or rebuild solely because an unused historical/global manifest differs.

The [progress assessment](PROGRESS-ASSESSMENT-20261115.md) describes remaining
production work; its requested-pause status is superseded by the user's resume.
Complete production sculpt/local meshing/recovery, painting/texture sets/baking,
remesh/retopology/UV workflows, humanoid/rig/weight authoring, glTF/VRM coverage,
physical XR/input and accelerator execution remain required. Controller trays
and spatial node/history editing are additional required later phases.
ArmorPaint is assessed, not integrated as a hosted engine. Preserve the accepted
selection interface and CC0 VRoid-only creator bases. Passing subsets, build
completion or an integration checkpoint do not complete the goal.
