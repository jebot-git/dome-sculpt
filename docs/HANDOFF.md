# Agent handoff — background UV validation

The full native FLTK sculpt/texture/avatar objective remains incomplete in
[REQUIREMENTS.md](REQUIREMENTS.md). Preserve Blender 5.4 Alpha/Gmsh/OpenBrush and
beneficial native upstream/plugin work, BDF-only fonts, DomeD appearance and
universal menus, shell-owned Super+X and controller-oriented sliders/joystick
menus/fine wheels. Python is optional test orchestration only; no Qt/GTK/Pango
frontend/runtime. Animation/final rendering remain outside scope.

Repository: https://github.com/jebot-git/dome-sculpt, main. Previous UV checkpoint
`36feedd` is archived in [HANDOFF-36feedd.md](HANDOFF-36feedd.md). Public checkpoints
and handoffs are authorized. No installation or active-session restart occurred.

## Integrated upstream authoring milestones

Upstream `7e233fd` and its preceding ten commits added persistent COW sculpt
layers, durable recovery with desktop/XR menus, native static OBJ/glTF/GLB import,
BVH and mesh-to-volume kernels, native retopology/capped cuts, rig/weight/morph/
humanoid kernels, immutable scene-v4 deformation and validated skin/morph GLB
export. Their complete handoff is archived in HANDOFF-7e233fd.md; preserve all
source, tests, provenance and the additional anime/toon humanoid requirement.
The kernels do not establish complete interactive workflows or full MToon/VRM.

The UV worker stage was originally committed as local 69dc4af, backed up on
checkpoint-uv-worker-69dc4af, then rebased over those upstream commits. Recovery
fields/checkpoint behavior and rigged-UV rejection remain intact. Both kinds of
replacement (ordinary and recovery) invalidate obsolete UV workspaces/checks.
No agents were spawned by this work; upstream handoff agent names describe
another workspace, not verified live handles here. Preserve future remote work.

## Changes and invariants

- Profiling found full UV validation cost ~105 ms on the 19,944-triangle fixture,
  versus 0.047 ms for mesh copying and 0.128 ms for structural validation.
- Document-owned UV workspaces defer full validation to a dedicated worker. Only
  one newest pending mesh is queued; request tokens cancel obsolete checks.
  Results require workspace session, document generation and exact immutable
  mesh identity. Worker validation holds neither model nor input locks.
- Previews/gesture undo publish immediately with a checking state. Restored
  pending history snapshots revalidate. Selection/copies/topology/relax/pack
  remain synchronous; do not claim all UV work is off the interaction path.
- Apply captures an immutable target for validation and paint transfer on the
  file worker. Validation failure retains prior paint and the editable draft;
  editing/Undo are blocked while committing. The native modal remains until
  success, with retry/correction on failure. XR exposes Check & Apply while a
  check is pending. Accepted Apply still survives later focus loss/shutdown.
- Unaccepted cancellation/replacement invalidates checks; stale results cannot
  reopen a closed workspace. New document generations reject stale asset jobs.
  Public edit_uv is now asynchronous with respect to validation; use wait_uv
  when asserting validity and wait_asset for accepted transfers, including when
  the workspace was closed. Standalone UvWorkspace defaults to immediate checks.

## Additional upstream integration

Upstream db1d710 and its preceding three commits add native normal/AO/PBR
baking, filtered BVH queries, constrained adaptive triangle remeshing and guarded
Document::edit_scene/static import authoring. Their full handoff is archived in
HANDOFF-db1d710.md. Preserve the no-silent-attribute-loss import checks, retained
paint guard and source/revision/content/selection checks. Their bake/remesh
kernels still need full desktop/XR authoring integration; full glTF/VRM and the
creator are not established by these foundations. Remote handoff references to
uncommitted work/running agents are not verified process handles here.

## Validation provenance

The results below are pre-integration UV-stage evidence. Upstream independently
recorded 19 native suites, 22 GUI and 37 menu checks in its workspace. Those logs
are not present here. Fresh combined validation is recorded in VALIDATION.md;
do not relabel either earlier set as a fresh combined run.

## Build, evidence and measurements

Read AGENTS.md and TOOLKIT.md. Release build uses the locally rebuilt canonical
DomeD 742d009 toolkit in artifacts/toolkit; all 22 patch hashes and vendored
source hashes match the manifest. The clean upstream is artifacts/domed-source.
The parent DESIGN_CONVENTIONS file remains absent; current DomeD README and the
committed menu audit supply available conventions. No installed prefix changed.

All ten native suites passed (243.56 s), including 40 complete OpenXR checks.
Three targeted UV/controller/document ASan/UBSan/leak suites passed (222.75 s).
Native GUI 23/23 and universal menu 25/25 passed. Results and failed attempts
are in VALIDATION.md; inspect terminal logs and current report completion fields.

The isolated 20-sample CPU benchmark on the same 19,944-triangle fixture reports
1.077 ms mean / 2.509 ms maximum edit submission, versus the preceding ~106.3 ms
mean / 110.1 ms maximum synchronous baseline. Final validation waited 104.96 ms
in the background. Every sample was applied, preview valid, whole-gesture undo
exact. This is not accelerated validation or a physical headset/production-scale
performance claim. Reproduce with the excluded-from-all native CMake target
`dome-sculpt-uv-benchmark` and a persisted painting fixture; see UV.md.

## Resume work

1. Integrate the now-present import/retopology/rig/deformation kernels into native
   desktop/XR authoring, preserve retained data/attributes, and continue native
   remeshing/baking, masks/symmetry/local meshing and recovery. Upstream's handoff
   lists baking/remeshing/creator work; inspect current remote state before
   implementing overlapping files. Preserve suitable native upstream reuse.
2. Complete conformal unwrapping/stitching/texel-density workflows. Profile larger
   meshes and move expensive remaining draft operations off interaction where
   needed, preserving invalid previews, accepted Apply ordering and exact history.
   Distinguish edit submission from completed validation in measurements.
3. Complete stencils/projection/texture sets and humanoid rigs/automatic weights/
   skinning/morphs/full MToon/glTF/VRM plus requested plugin-equivalent workflows.
   Do not reduce the objective or substitute static GLB for avatar export.
4. Verify lost-instance/session recreation and physical controllers/stylus/
   trackball/3D mouse plus CUDA/ROCm/OneAPI. Current CPU/synthetic/software-GL
   results do not prove those hardware paths.

Rendering uses immutable snapshots; model/file/asset jobs preserve ordered saves,
source identity and revision guards. Input remains bounded at 64 samples and
reference-space/document epochs invalidate old poses. Inspect processes before
starting/restarting tests; generated builds, projects, logs and benchmark results
remain ignored. Contracts: INPUT.md, UV.md, PAINTING.md, UNIVERSAL-MENU-AUDIT.md.
