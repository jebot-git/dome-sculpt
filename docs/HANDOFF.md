# Current handoff — actual CC0 VRoid creation integration

The original full production objective is ACTIVE and INCOMPLETE. Read
REQUIREMENTS.md, HANDOFF-466ebd1.md, workspace AGENTS.md and DESIGN_CONVENTIONS.
Previous detailed history/state is archived in HANDOFF-creator-integration-history.md.
User explicitly authorizes delegation and milestone commits/pushes. New avatars
must use verified historical CC0 VRoid Studio models as the primary baseline;
MakeHuman is an explicit alternative, not a substitute for that requirement.

## Current implementation

Native C++20 FLTK desktop/OpenXR. Source/image/model work runs on ordered workers;
renderers acquire immutable snapshots, exact global history handles transactions.
Canonical22-patch DomeD FLTK Release toolkit is artifacts/toolkit; see TOOLKIT.md.
Use build/run-dome-sculpt or prepend its lib64 directory to inherited library
paths. Do not restart sessions or overwrite unrelated DomeD/qimgvr work.

Scene v5 reads2/3/4. Groups have parents, transforms, creator parameters and exact
snapshots; desktop picking/XR input/render/GLB all compose group transforms once.
Explicit source alpha modes/cutoff and standard unlit material preview/export are
implemented. This is not full MToon/VRM: normal/AO/MToon shader/export and avatar
meta/constraints/spring bones still require implementation and verification.

Actual CC0 beta female/male sources are bundled under assets/humanoid/vroid,
with exact official author grants/hashes and matching embedded CC0 metadata.
Native controls schema2 chooses vroid-beta-female/male or makehuman-hm08; legacy
schema1 preserves MH. Source bytes/rig/images/UVs/morphs/material metadata survive
native projects through shared source_models/source_images archives. Original
source payloads remain intact even for features not yet previewed/exported.

Creator parts use source-authored UV painting only for verified source appearance:
shared/mirrored charts paint together,28 zero-area UV faces retain rendering
geometry but are explicitly unpaintable. Ordinary authoring remains strict.
Source/archive/image budgets and whole native256MiB file limit remain enforced.
Deep source JSON checks stay on asset/file workers; source-preserving native
scalar commands use shallow checks on already validated immutable payloads.

Native Scene > Create humanoid offers female/male/MH bases, presets and body/face/
hair/colors, exact modal cancel and keep/replace choices; geometry runs on worker.
Headset Objects > Avatars offers default CC0 creation, part navigation and group
operations with exact undo. Full parameter editing/rig/weight/morph workflows
inside headset remain incomplete. Groups and creator/native/source diagnostics
need further usability/quality work; do not equate default creation with full VR
authoring. Parent source data remain retained while geometry/materials are edited.

## Fresh evidence

- All28 Release CTests passed in56.86s: artifacts/tests-avatar-controls.log.
  Includes real-loader synthetic OpenXR, both actual grouped CC0 creators/native
  restart, joystick female creation/group delete/ungroup/undo, independent cgltf
  grouped skin evaluation and source alpha/unlit GPU cases.
- Native universal menu42/42 passed: artifacts/menu-creator.log. Actual creator
  dialog ownership/isolation/cancel, import/recovery chooser cancel included.
- Explicit GPU source unlit/opaque/mask/blend tests pass: tests-source-alpha.log.
- Actual append_humanoid ->Scene::save/load ->GLB QA by creator agent passes:
  artifacts/vroid-native-workflow-qa.log. Female119,795,368bytes, male124,948,360bytes,
  below256MiB; original19parts/25images/28unpaintable/36,738linked texels preserved.
- Per-component Release/ASan/UBSan/leak evidence is in component docs. Root final
  combined pinned-source sanitizer/build verification is next; shared agent ABI
  can change while working. Verify a detached checkpoint before pushing.
- No physical headset/controller/stylus/3D mouse or compute accelerator suite
  completion is claimed. MToon agent reports hardware Arc A770 EGL checks for its
  standalone module, not integrated renderer or accelerator-backend completion.

CMake now registers an additional source-uv suite with actual female/male args,
and supplies actual baseline args to source-maps/creator-archive tests. These
expanded registered commands need their final pinned run; do not call the earlier
28-suite result a29-suite success.

## Agents and ownership

/root/finish_ab3d1: source UV committed79440d0, now optimizing bounded immutable
composite caching in paint files/tests. Correct fork-aware content tokens are
required, not revision_ alone. Actual female uncached Scene::surfaces costs~5s
and120.8MB derived RGBA; unchanged calls must reuse results. Cache bounds unique
original Image identities to avoid overcharging and thrashing. Later raw GPU
bindings/overlays are necessary to avoid first CPU bake; no renderer ownership yet.
/root/sculpt_rig: CC0 adapter/assets committed33164e6, performing actual textured
native save/restart/GLB/Blender/validator QA with root append helper. Correctly
reports current GLB normal/AO/MToon limitations; do not overstate appearance parity.
/root/sculpt_mesh: preserving/memory/legacy VRM0 import committedacc73a1, now NEW
mtoon.hpp/mtoon.cpp/mtoon-shader.hpp/tests plus focused license/docs. Standalone
numeric +hardware EGL cases being finalized, no Scene/render/export ownership.

Root owns CMake/Scene/groups/creation/Document/app/palette/render/export/central
docs. Stage only owned files; preserve agents' uncommitted cache/MToon work.
Use Git author jebot-git,326713999+jebot-git@users.noreply.github.com per command.
Last pushed checkpoint before this integration is db1d710; it has six pinned
whole-core sanitizer suites passed (artifacts/tests-assets-pinned-sanitize.log).

Next: commit current reviewable integration, create detached worktree and run
optimized/pinned sanitizer checks against that exact source, then push milestone.
Continue native parameter/rig/weights/morph editing in desktop AND headset,
material/normal/AO/MToon/VRM integration, GPU source/paint efficiency, all remaining
sculpt/UV/retopo/procedural/device/compute rows. Do not shrink or complete the goal
at this milestone.
