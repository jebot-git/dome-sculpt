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

## Additional asset-worker/import milestone

Root added Document::edit_scene(label, SceneOperation, AssetSource expected={})
on the ordered asset worker: detached snapshots, cancellation, validation outside
model lock, alias detachment, generation/revision/content/selection guards and
one exact global history transaction. File > Import mesh creates paintable
supported static OBJ/glTF/GLB objects; unsupported source channels are explicitly
rejected, and importer diagnostics appear in bounded completion messages.
See ASSET-WORKER.txt. A conservative prepare-mesh guard now rejects replacing
retained paint data; existing UV paint transfer is unchanged.

Component milestones now also include4839a1b native tangent-space normal/AO/PBR
baking, fdb0a7d predicate/budget/cancellation BVH queries and8345a18 constrained
adaptive triangle remeshing. CMake includes these committed sources/tests.
Full Release21/21 passed before the added remesh target; subsequent remesh,
asset-worker/UV/shared-document/baker checks and native OpenXR passed after the
paint guard/warning reporting refinements. Native universal menu39/39 passes,
including actual Import file-chooser ownership/cancellation. Logs:
artifacts/tests-assets-final.log, tests-assets-remesh.log, tests-assets-report.log,
tests-assets-xr.log and menu-assets.log. Final isolated sanitizer verification
is being run against this exact commit because agents continue editing shared
paint/import files. Do not claim its result until the log completes.

Current agents: sculpt_mesh extends lossless PreserveGltf raw node/skin/morph
import; finish_ab3d1 implements projection/stencil paint and O(1) paint-data
presence; sculpt_rig implements the CC0 MakeHuman-based humanoid creator with
VRoid workflow references. Do not stage their uncommitted files. Creator renders
are artifacts/humanoid-preview.png and humanoid-face.png: coherent authored body
and original hair/eyes, still a toon/doll foundation rather than polished VRoid
artwork. Root requested further intentional anime face/preset quality work.
Next root work: persistent scene groups/creator parameters and native desktop/XR
creator UI, then rig/weights/morph authoring, paint/bake/import integration and
remaining full requirement rows. Keep the production goal active.

## Active integration after db1d710 (not yet a completed creator workflow)

User specifically directed actual CC0 VRoid Studio models as the humanoid base.
Official old-beta CC0 grant: https://vroid.pixiv.help/hc/en-us/articles/4402614652569 .
Current A-Z samples are not those assets. Creator agent verified exact original
Base_Female/Male metadata/hashes against that grant; assets live under
assets/humanoid/vroid. Native schema2 controls use baseline strings
vroid-beta-female / vroid-beta-male / makehuman-hm08; old schema1 decodes MH.
Default is neutral authored female VRoid, preserving source textures/factors.

Milestone db1d710 WAS pushed to origin/main after six source-pinned whole-core
ASan/UBSan/leak suites passed (3.10s): artifacts/tests-assets-pinned-sanitize.log.
Source is detached worktree artifacts/verify-assets at db1d710; do not remove
that worktree while any associated verification process is live. Earlier shared
sanitizer build also passed, but pinned evidence is authoritative. Original
production goal and all remaining requirements stay active; never complete at
these component milestones.

Subsequent committed components include65a1343 MH generated UV correctness,
21b9509 preserving glTF, ea1d0c5 native projection/stencil,9850473 original source
texture maps and image archive,0becd8d exact original creator model archives,
acc73a1 native memory GLB/VRM0 compatibility. Some are not pushed yet. Agents
own their working files; stage only owned files. Use per-command Git author.

Root CURRENT UNCOMMITTED integration:
- Scene v5 (reads2/3/4) groups with parent/world transforms, creator controls,
  exact snapshots, group delete/ungroup, renderer/picking world composition.
- Native append_humanoid helper: parts grouped, correct primary/body selection,
  original source maps beneath overlays and immutable CreatorAppearance.
- Shared source_images and source_models archives, exact raw original GLB plus
  all decoded source images/material/MToon/accessors survive restart, aggregate
  image/remap/source budgets and full256MiB file cap. Deep source JSON validation
  stays on file/asset workers; native scalar property commands reuse validated
  immutable sources without reparsing16MB GLBs under model lock.
- Explicit native material alpha Auto/Opaque/Mask/Blend, cutoff and unlit flags,
  native persistence, GPU preview and glTF unlit/alpha output. This is a standard
  unlit fallback, not MToon. MToon normals/AO/render/export are still incomplete.
- Nested group glTF nodes and correct grouped skin identity-root semantics,
  independent cgltf world/inverse-bind test. Removed test-local cgltf implementation
  because runtime importer now links through creator archive; one implementation.
- Native FLTK Create humanoid dialog, baseline/preset/body/face/hair/colors,
  source-token draft, modal cancellation, keep/replace objects choice, all geometry
  on worker. Needs final native GUI and actual VRoid insertion tests. VR creator
  menu/controls are still missing and must be implemented for full VR workflow.
- CMake includes groups/creation/creator-editor, VRoid source, projection,
  source-maps, creator-archive. Source/installed asset macros and data install
  directory are wired. No system install/session restart performed.

Fresh combined Release build succeeded. Ten focused suites passed in4.03s,
including grouped MH creator/native roundtrip, both creator kernels, source maps,
creator archives, cgltf grouping, scene/deformation/projection and GPU. Logs:
artifacts/build-vroid-integrated.log and tests-vroid-integrated.log.
Explicit GPU unlit color/opaque/mask/blend checks subsequently pass in2.97s:
artifacts/tests-source-alpha.log. These do NOT prove actual default VRoid
paintable scene insertion yet: original model UVs have shared/mirrored charts
and28 zero-area faces, so ordinary strict PaintedMesh rejects them. Never weaken
strict authored meshes or re-unwrap/reindex/drop source data to make tests pass.

Agents now:
- finish_ab3d1: Creator archive committed; implementing explicit
  PaintUvPolicy::SourceAuthored via fourth from_maps PaintSurfaceOptions argument,
  bounded sparse shared-UV surface candidates/nearest eligible brush projection,
  zero-area UV face diagnostics while retaining all rendering geometry. Ordinary
  ctor stays Strict. Policy persists. Wait for stable API and all19 parts of both
  actual models before root append selects it for verified source appearances.
- sculpt_rig: actual CC0 beta native adapter/assets; requested focused commit of
  validated adapter now. Header fields include CreatorAppearance.original_bytes,
  source_material/source_primitive, shared decoded images and raw metadata;
  HumanoidPart.source_maps/appearance/roughness, CreatedHumanoid.primary_part.
  Original UVs, weights, morphology and exact source bytes remain intact.
- sculpt_mesh: memory/VRM0 compatibility committedacc73a1, now typed MToon0/1
  schema/math/GLSL in NEW mtoon files. No Scene/render/export ownership granted;
  root integrates typed materials/shaders after validation. MToon0 gamma-vs-linear
  factors and painted composite factors require care to avoid double application.

Immediate root steps: wire approved source UV options/diagnostics when ready;
finish actual female/male grouped creation/CBOR/export/render/GUI tests; add XR
creator controls and safe cancellation. Complete metadata/material/normal/AO and
MToon binding/export. Final combined optimized/native/GUI/compositor/sanitizer
verification should use pinned worktree to avoid shared-agent ABI races, then
commit/push a concrete milestone. Update toolkit provenance if new canonical
FLTK changes exist; preserve unrelated parent DomeD/qimgvr modifications.

Latest continuation:33164e6 actual verified CC0 VRoid adapter/assets is committed;
all27 registered targets currently build after removing duplicate cgltf test
implementation. Native creator entry dispatch was fixed (menu existed but the
old conditional pattern missed the handler). Native universal menu now passes
42/42 checks, including actual creator dialog ownership/isolation/cancellation:
artifacts/menu-creator.log. All10 integration suites passed and explicit source
unlit/opaque/mask/blend GPU tests pass in tests-source-alpha.log.

Paint agent now has working-tree SourceAuthored UV API (PaintSurfaceOptions fourth
from_maps argument, serialized policy, sparse linked candidates +diagnostics).
Root append_humanoid opts into that ONLY for verified retained CreatorAppearance,
passes cancellation; ordinary new meshes remain Strict. Root tests/groups.cpp
now creates BOTH actual CC0 avatars via guarded Document worker, checks20objects,
one original source/25shared images, native encode/reopen and one-step undo.
This combined build/test is RUNNING from exec session64397:
artifacts/build-linked-vroid.log -> artifacts/tests-linked-vroid.log. Poll that
specific handle/log; do not restart merely from missing output. It is not a
claimed pass yet. Creator source policy agent is still finishing focused tests.

Root uncommitted Scene v5 groups/material/archives/creator UI/renderer changes
remain intentionally unpushed until this real workflow passes. Prior db1d710
milestone WAS pushed and pinned-sanitizer verified. Agent commits after it are
pending the combined milestone push; current remote lags main. Update provenance,
requirements, native GUI/XR controls, source diagnostics and pinned verification
before pushing. Whole original production objective plus actual CC0 VRoid base
requirement remains active, with much work in XR authoring/MToon/VRM/compute.
