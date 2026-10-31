# Viable implementation candidate: 993e8af

On 2026-10-30 the user reported: “Test passes, record as viable candidate for
further implementation, keep paused”. The user then requested ending the test.
The goal remains paused and incomplete; agents are idle.

The tested source is `993e8af7ebc247e3239df3a1acd27fc47fb8e999`, branch
`work/production-authoring-c1`, local worktree
`artifacts/production-authoring-c1`. Its tracked files were clean at launch and
when recording the result. Preserve this branch and its optimized binary.
At the time this test was recorded in `b075982`, the combined sources remained
private. The later XR selection milestone publishes a descendant; see
XR-OBJECT-SELECTION.md. This original binary and physical test remain the
baseline, and no system installation is implied.

The coherent optimized Release build passed all247 configured CPU regression
suites in383.34s. The selected own-core/render/support ASan/UBSan suites passed
24/24 in41.83s. That sanitizer selection does not instrument the frontend or
physical OpenXR runtime. The earlier color-authority diagnostic-order failure
was repaired without weakening the test before these final runs.

The physical test used the actual WiVRn runtime, STAGE reference space, Touch
Controller Pro profiles and Intel Arc A770 at PCI0000:03:00.0. Canonical private
FLTK1.4.5+23 and bitmap runtime libraries were loaded. The test submitted129755
stereo frames; user confirmation is recorded as a passing candidate test, not
as requirement-by-requirement production acceptance.

On the user's end-test instruction, SIGTERM was delivered only to the recorded
candidate process through its pidfd. It terminated gracefully, reported final
XR statistics and wrote the130990125-byte project. The saved session is at
`/home/blux/.local/state/dome-sculpt/headset-tests/20261030-232856-993e8af/Sculpt.dsculpt`.
The application log and launch receipt are alongside it. The save's existence,
size and SHA-256 were verified; it has not been reopened as part of this record.
No desktop/runtime restart or system installation occurred.

This candidate combines affine placements and held contact, current polygon
corner colors, explicit source-retaining normal preparation, source-retaining
shapekey tools/freehand editing, worker failure recovery and pinned JSON
exception-safety repairs. The full production objective remains in
REQUIREMENTS.md and HANDOFF-466ebd1.md. In particular, prepared-normal/shape and
UV/source adapters, current-color bake/native-cut integration, general affine
import admission and broader authoring/interoperability/device qualification
remain unfinished. Do not remove their guards based on this headset pass.

Resume checkpoints are retained in
`artifacts/production-integration-status.json`. Unmerged private checkpoints
include authored-key consumer proof0c6de7e, prepared-normal/shape handoff7a66e71,
affine import WIP0b65130 and corner-cut WIP91aa08c. Preserve their exact proof
scopes and all existing local branches, worktrees and backups.

Machine-readable evidence and hashes: validation/HEADSET-CANDIDATE-993e8af.json.
