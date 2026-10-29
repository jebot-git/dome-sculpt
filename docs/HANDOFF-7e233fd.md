# Current agent handoff — native authoring milestones

The full production objective remains ACTIVE and INCOMPLETE. Read REQUIREMENTS.md,
the original HANDOFF-466ebd1.md, workspace AGENTS.md and DESIGN_CONVENTIONS.
The previous local toolkit/layer/input/UV history is archived unchanged in
HANDOFF-toolkit-and-layers.md; remote checkpoints are archived separately.

## User steering

Commit and push at validated milestones. The additional user requirement is
robust native anime/toon humanoid creation based on MakeHuman and VRoid Studio;
it is now a requirement row and assigned to the avatar agent. License actual
code/assets explicitly; proprietary VRoid assets/runtime and MakeHuman Python
runtime are not implied dependencies. The production suite is not complete.

## Environment

Use optimized Release builds and the canonical latest committed DomeD toolkit
in artifacts/toolkit. TOOLKIT.md records DomeD742d009, FLTK1.4.5,22 patches and
full hashes. build/run-dome-sculpt selects that actual toolkit before inherited
same-SONAME libraries. The fresh compositor was built separately; no active
session restart or system installation occurred. Preserve unrelated parent
DomeD capture and qimgvr appearance work. Do not merge old fltk-vr snapshots
by replacing local history. Git author per command is jebot-git / GitHub noreply.

## Implemented milestones

- c0adaa5: persistent COW sculpt displacement layers, exact transaction history,
  native FLTK and controller layer controls, nested volume v2 with legacy import.
- e00b5e8: bounded durable recovery worker and malformed/crash/storage fixtures.
- c6976a3 / ee9225a: skeleton, constrained sparse weights, graph binding,
  transfer/mirror and topology-identified morph foundations.
- a2177ba: bounded native static OBJ/glTF/GLB import, retained resource/attribute
  payload, cleanup source maps, immutable BVH and closed-shell mesh-to-volume.
- 7d11883: humanoid semantics, marked-human fitting, safe bone compaction and
  expression/morph identity kernels, with official specification provenance.
- c4df13b: stable-ID native retopology and attribute/provenance-preserving capped
  plane cuts, with explicit unsupported topology/contour failures.
- 0c5f323: immutable per-object deformation, scene v4 reading2/3, detached
  rest/preview geometry, full influence-group and POSITION morph GLB export.
  Both skin/humanoid fixtures have zero Khronos validator issues. Humanoid data
  is application extras; no VRM extension is claimed. See DEFORMATION.md.
- Merged remote d8e3699 and36feedd, preserving ordered input/focus/session fixes
  and gesture-level UV undo alongside the local milestones. Both histories
  were pushed through e78ddc0 before the subsequent component commits.

## Root integration in this milestone

All committed native foundations above are in CMake/CTest. Document meshing
uses deformation preview on its worker; GLB export retains rest meshes. UV
reparameterization/rest-mesh replacement rejects rig data until explicit
transfer/removal is implemented. No silent topology invalidation.

Interactive desktop and standalone XR enable recovery; ordinary Document tests
remain disk-free by default. Completed dirty transactions enqueue immutable
snapshots, excluding open strokes/control previews/asset work. File jobs capture
source identity and revision. Native save fsyncs file AND parent directory before
conditional checkpoint removal. Failed saves keep checkpoints. Recovery scans
and loads run on the ordered file worker with generation/content guards; restored
projects are dirty/untitled, and dirty work cannot be replaced. Desktop File >
Recovery has native Refresh/chooser controls. XR Objects > Sculpt layers >
Recovery has Refresh/Next/Previous/Recover. Status/errors remain visible. Native
chooser cancellation preserves the project path. Already durable old-source
checkpoints survive replacement; unflushed old-source jobs are invalidated.
The automatic XR output directory is now created on the file worker.

## Evidence

- artifacts/tests-authoring-milestone.log: all19 Release CTests passed,
  including real-loader synthetic OpenXR and native recovery joystick tests.
- artifacts/gui-recovery.log / artifacts/gui/report.json: all22 GUI checks pass.
- artifacts/menu-recovery-chooser.log / artifacts/menu/report.json: all37 native
  universal-menu checks pass, including actual persisted recovery discovery,
  modal ownership and cancellation. Chooser assertions exclude only the private
  recovery directory, preserving the check for unwanted user-file writes.
- artifacts/tests-recovery-sanitize.log: seven focused suites pass with the
  whole native core built using ASan/UBSan/leak detection. Later notification/UI
  refinements need their final focused rebuild; do not overstate this coverage.
- Per-component docs record additional optimized/sanitizer checks. Shared-source
  concurrent builds can catch an agent's intermediate code: the first new GLB
  fixture build encountered cgltf special targetNames/raw-extras behavior, fixed
  by the owning agent; the subsequent19-suite run passed. The first menu run's
  no-files assertion counted intended private recovery checkpoints; it now
  explicitly rejects all user-project writes outside that private directory.
- No physical headset/controller/stylus/3D mouse or compute accelerator tested.

## Continuing work / agents

User explicitly authorizes delegation; shared filesystem, four total slots.
/root/finish_ab3d1 owns bake.hpp/bake.cpp/tests and focused docs, then projection/
stencil paint foundations. Baker rejects genuine collapsed UV triangles;
dense xatlas64 has eight zero-area triangles and remains an atlas-quality gap.
/root/sculpt_mesh owns native remeshing and optional backward-compatible BVH
predicate queries, plus its focused tests/docs. Existing importer/retopo kernels
are committed; interactive workflows and lossless attribute integration remain.
/root/sculpt_rig owns NEW humanoid-creator native parameter/asset work, inspecting
MakeHuman/VRoid official primary sources/licenses. Prior Scene/export integration
is committed/stable. Do not overwrite agents' in-progress files.
Root owns Document/app/CMake/manifests/handoff and integration. Next integrate
native import/mesh operations, baking/projection, full rig/weights/morph authoring
and creator desktop/XR flows using the same exact history and worker/source
guards. Continue every remaining REQUIREMENTS row, including robust conformal
UV/stitch, MToon/VRM, local meshing/masks/resolution and actual compute evidence.
Do not mark the full goal complete at a component/checkpoint milestone.
