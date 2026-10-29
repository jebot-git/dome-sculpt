# Resume checkpoint — 2026-10-29

The user requested: “Commit and push current stage, work will resume. Prepare
agents handoff.” This checkpoint adds documentation only. The application source
remains exactly at 7817ca2b16faa7b2455f501039b6a6eb7b0c1d23, the native desktop/XR
bake-authoring milestone already published to origin/main and
origin/work/native-bake-controls. The working branch is work/native-bake-controls.
No weight-painting integration was edited during the subsequent planning turn.
The full production objective remains incomplete; retain REQUIREMENTS.md.

## First action on resumption: inspect both public histories

Repository: https://github.com/jebot-git/dome-sculpt

Fetch on this checkpoint discovered a separate public branch:
`origin/checkpoint/native-xr-authoring-8f4d67d`, currently 88cde79. It diverges
from this branch at af492eea03dec34d8fa3d2b6fff2691405cf67da. It contains eight
commits beyond that common base; this branch contains 7817ca2 beyond the base.
The other branch already contains native/XR weight painting, conformal UV and
stitching, original MToon rendering, source morph transport and background avatar
export. Its handoff says it still needs the incoming 7817ca2 bake controls.

Inspect its committed source and validation before implementing these features
again. Preserve both histories. Do not reset/force-push either branch or describe
either branch's tests as proof of a future merged tree. The remote handoff also
mentions work that was dirty in its original workspace: that work is not present
in this checkout, and its local agent names are not live ownership here.

Useful read-only starting commands, from the repository root:

```sh
git fetch origin
git status --short --branch
git log --oneline --left-right HEAD...origin/checkpoint/native-xr-authoring-8f4d67d
git show origin/checkpoint/native-xr-authoring-8f4d67d:docs/HANDOFF.md
git show origin/checkpoint/native-xr-authoring-8f4d67d:docs/VALIDATION.md
git diff HEAD...origin/checkpoint/native-xr-authoring-8f4d67d -- include/dome/document.hpp src/document.cpp src/render.cpp src/input.cpp
```

If combining the stages, use an isolated integration branch/worktree, resolve the
shared worker/input/render/UV/export contracts, and build/test the exact merged
source. Both branches modify these contracts. Pay particular attention to menu
page/command identities, weight scalars through Mikk corner expansion, immutable
preview ownership, cancellation leases, original-source material fidelity and
ordered save/export behavior. Preserve the native bake dialog and XR page12.

## This branch's verified state

Read HANDOFF.md, VALIDATION.md, BAKE-CONTROLS.md and AGENTS.md. Existing evidence
was inspected again when preparing this checkpoint; no new application test run
is claimed for this documentation-only change:

- Release: 41/41 suites, 280.73 seconds, tests-bake-controls-full.log.
- Synthetic real-loader OpenXR: 62 checks, failed=0, complete=true, passed.
- Whole-core C/C++ ASan/UBSan/leaks: six selected suites, 133.78 seconds.
- Final worker lease fix: three Release suites in 2.67 seconds and two
  whole-core sanitizer suites in 1.49 seconds.
- Native GUI: general menus 53/53; bake acceptance/errors 19/19; running
  cancellation 3/3; compact 640x360 with 2x Unifont BDF 4/4.

Exact logs and qualification of final focused runs are in VALIDATION.md. Reports,
captures, builds and logs under artifacts/ and build/ are local ignored evidence,
not files distributed by a clone. The other branch's 47-suite evidence belongs
to its own pinned source, not to this branch. No hardware headset/controller,
stylus, 3D-mouse, accelerator, installation or active-session restart is claimed.

No build/test/application validation processes were running at this checkpoint.
Inspect live processes before starting jobs; do not restart work because a tool
observation timed out. Do not edit files while their compiler may be reading them.

## Weight-painting planning on this branch: unimplemented

The native edit_weights kernel is implemented and registered; see WEIGHT-EDIT.md,
include/dome/weight-edit.hpp, src/weight-edit.cpp and tests/weight-edit.cpp.
Its desktop/XR integration is absent on this branch. Before the remote branch
was discovered, the following design concerns were identified; they are review
questions for the existing remote implementation, not completed behavior:

- Keep heavy posed-geometry/weight work on workers with bounded input queues,
  immutable previews, exact source guards, cancellation and one gesture-level
  global Undo. Define behavior for focus loss, Undo, save and shutdown while a
  gesture or final publication is pending.
- Pick the displayed skinned/morphed surface rather than rest geometry. Current
  Document::raycast on this branch picks the rest PaintedMesh. Consider visible
  linked parts, alpha/backface rules and an explicitly bounded physical brush.
- Preserve authored joint order, topology, morphs, source archives and locked
  weights. Do not infer symmetry from bone names or silently prune influences
  using a default four-influence limit. Define repeated-sample strength semantics
  so controller frame density does not unexpectedly increase painting strength.
- Weight heatmaps should be worker-built render-only data, including correct
  mapping through tangent corner expansion; settings should not dirty the scene.
  Native and XR controls must follow BDF, universal-menu and controller-slider
  conventions with actual enabled/check states and visible selected-joint feedback.

Relevant local extension points are in include/dome/document.hpp and
src/document.cpp: begin/stamp/end/cancel_action_locked, record_stroke_locked,
publish_locked, queue_scene_operation_locked, file_loop, mesh_loop and raycast.
Rig-linked part validation is in src/rig-edit.cpp; native picking/input is in
src/app.cpp; spatial gestures/menus are in src/input.cpp; shared drawing is in
src/render.cpp. Existing bake controls demonstrate guarded asynchronous scene
publication and native/XR parameter dialogs.

## Environment and retained constraints

Actual repository path: /home/blux/de-xr/dome-sculpt. The parent de-xr directory
is not a repository. Its other repositories were clean in the checkpoint scan;
do not stage or alter them as part of this handoff. No agents were spawned here.

Use the canonical DomeD FLTK toolkit described in TOOLKIT.md. This checkout's
artifacts/toolkit contains its verified 22-patch FLTK 1.4.5 build. Launch through
build/run-dome-sculpt or explicitly prepend its lib directory, because inherited
LD_LIBRARY_PATH can resolve older same-SONAME libraries. The separate remote
checkpoint documents a newer toolkit pin: reconcile source manifests and verify
actual linkage when integrating; do not silently substitute evidence.

Keep the C++20/FLTK/EGL/OpenXR runtime and BDF/Fontconfig/FreeType conventions.
Shared desktop helpers remain canonical qimgvr copies. All 30 source_sha256
entries in third_party/sources.json are checked at this checkpoint. Cold private
HOME startup can scan installed fonts before App/Document construction; the
native GUI harness allows 90 seconds for initial startup and 30 seconds for
interactions. Optimize that discovery in the canonical helper before propagating
copies. Retain full cancellation/history, worker-only geometry/file processing,
immutable snapshots, source/generation guards and original-source fidelity.

For the complete remaining scope use REQUIREMENTS.md. After branch integration,
continue production weight/rig/UV/retopology/cut/procedural workflows, full
MToon/glTF/VRM fidelity, production sculpt quality and all real-device/backend
verification. This handoff does not narrow those requirements.
