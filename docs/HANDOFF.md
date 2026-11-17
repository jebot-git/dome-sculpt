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

## Actual running jobs: inspect before acting

Authoritative state is the current service/process and its output, not this
snapshot. Never restart a job merely because an observation handle times out.

1. Release owning build: ds-native90-release-build-20261117.service,
   MainPID1978514, InvocationIDf18eb32adee84544b99a1c6404d1d403.
   Plan: artifacts/native90-release-planning-20261115/
   release-090f00-20261117-audit/plan.json, SHAccf9931035481459d7f51ec03e829ee2572e424d381d2bbc69a29c1fe26e1661.
   Status is execution-native90-v1/status.json under that audit directory.
   Actual291 compilations,737 links and3 generators have completed; the mixed
   dependency/output/final-input audit is still pending in the last observation.
  745 verified independent whole objects are explicitly reused. No reused object
   is called newly compiled; original and new dependency records remain separate.
   Require actual terminal0 and independent unit closure before product tests.
2. Strict sanitizer prepare-only: ds-native90-sanitize-prepare-20261117.service,
   MainPID2039800, InvocationIDb3efe8ba13c44307bd0571d5d4786dcb.
   It hashes original/completion providers and configures the same frozen source;
   no sanitizer compilation is authorized yet. Artifacts live under
   artifacts/native90-sanitize-planning-20261115. Source-aware two-lineage
   executor/auditor templates are prepared; see
   [SAN90 preparation](SAN90-EXECUTOR-PREP-20261117.md).

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
fresh link pass; runtime/font/scenarios remain explicitly6c8. The native capsule/
font qualification is authorized under a new unit and must be checked for its
actual result. Read the actor's artifact namespace
artifacts/artist-public-snapshot-link-20261117; do not blindly rerun old failures.

Next: native91, four genuine cases,32 positive verifier calls,8 wrong-Original
rejections, default4 bitmap masks unchanged plus22 additive labels; then actual
GUI/XR source selection, Bake, immediate Save, Undo/Redo and reopen. Operational
wrapperv2 is in artifacts/prepared-public-operational-20261117, with an explicit
fixturef1/scenario6c8/runtime+font6c8/product1bc source tuple. Originalv1 remains
unexecuted. Each actual phase/case must bind the true native terminal status.
Desktop is private-Xvfb software GL; Arc syntheticXR and native Wayland/DomeD
are distinct evidence. Physical-device acceptance remains required.

Combined native90 runner is prepared in artifacts/native90-release-gates-20261117;
its exact driver SHA is55f06be3963aaff1726962b224d7a534015ea89b179cb13e9e935dfe4744f057.
It requires true owning0/1776 outputs/1036 actual dependency owners and a separate
review binding the actual status SHA before any products. Use the common lease
/tmp/dome-sculpt-arc-device.lock before ldd/products/GL and release only after
owned cleanup. Original87/SAN/runtime failures remain honest separate records.

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
