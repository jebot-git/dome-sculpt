# Full authoring goal; bake-controls merge pending

Read AGENTS.md, REQUIREMENTS.md, DESIGN_CONVENTIONS in the parent workspace,
TOOLKIT.md and VALIDATION.md. Preserve the original production objective in
HANDOFF-466ebd1.md: native C++20/FLTK desktop/OpenXR sculpting, painting, UV,
rigging, skinning and avatar authoring, full required glTF/VRM, named plugin
workflow equivalents, input devices and build-time accelerators. Animation and
final offline rendering are excluded; interactive shading is included. No
partial checkpoint, build or passing subset completes this goal.

## Current histories and merge

Local main is `88cde79`, recording exact application `8f4d67d`. The latter merged
remote `af492ee` source-aware baking/Mikk with local native/controller weights,
conformal UV, MToon/linear rendering, source morph transport and source VRM0
export. Both histories were preserved. The local verified checkpoint is pushed
on `checkpoint/native-xr-authoring-8f4d67d` at `88cde79`.

Remote main then advanced to `7817ca2`, adding native desktop/controller baking
controls. That commit is now being merged, with no combined-source verification
yet. Preserve all incoming work; never force-push over it. Incoming handoff and
validation are archived as HANDOFF-7817ca2.md / VALIDATION-7817ca2.md, and describe
a different workspace. Older exact histories remain in git and existing archives.

All unfinished next-feature changes were saved before this merge in `stash@{0}`:
`Preserve lazy UV source UV baking and skin authoring during7817 merge`. This
includes tracked lazy UV hooks/tests/docs and untracked UV initialization/loader,
all-slot sourceUV and skin-authoring helper files. Do not drop that stash. Restore
it only after the merge is committed and preserve conflicts explicitly. Query
live agent state rather than assuming historical agent names are still active.

## Verified local checkpoint8f4d67d

Exact detached source in artifacts/verify-authoring-merge passed47/47 Release
suites147.34s and five whole-core ASan/UBSan/leak suites18.24s (GLB
interoperability, baked persistence, source morphs, UV workflow and weight
authoring). The C Mikk and xatlas dependencies were instrumented too. Native
menus53/53, rig/weights12/12, avatar export5/5, baked-surface GUI5/5 and actual
latest DomeD Wayland/universal-menu8/8 pass with zero process/report failures.
Logs use authoring-pinned names; see VALIDATION.md for exact scopes.

Canonical FLTK and compositor both come from clean DomeD282467e. Toolkit is
artifacts/toolkit-282467e; compositor is
artifacts/domed-282467e-target/release/dome-compositor. Its Release build completed
in8m03s. Use the updated prefix's lib64 in LD_LIBRARY_PATH; inherited /opt paths
can override ELF RUNPATH. Preserve unrelated DomeD/qimgvr edits. No system
installation or active desktop/display-manager restart occurred.

## Implemented local authoring contracts to preserve

- Ordered asynchronous Document::export_asset captures committed Scene, returns
  a future and reports status/errors without blocking UI. GLB exports the scene;
  native/controller VRM exports the explicitly selected avatar group. Existing
  files survive unsupported export failures. Source-preserving VRM0 still refuses
  generated/source-less/painted/unproven cases until they are implemented.
- Desktop/controller weight gestures collect bounded capsules, then compute
  posed common-frame edits on a worker. Maximum capsule falloff applies once per
  original vertex, so frame density does not accumulate strength. Influence
  locks, cap/prune, explicit symmetry and linked visible parts retain exact source
  tokens and one global Undo. Constrained no-op strokes add no dirty/history
  entry. GPU heatmaps and worker BVH target actual posed geometry. FLTK and XR
  page13 expose settings/joints/modes/locks/symmetry/linked parts.
- UV solves/stitch/relax/packing run on the UV worker with complete vertex/corner,
  skin/morph/source remaps and exact pins/history. Accepted Apply survives implicit
  focus/rebase/shutdown; explicit Cancel invalidates transfer. Original textured
  source UV edits remain guarded until all-slot bake/adopt is integrated. Initial
  topology construction is still eager in the committed checkpoint; lazy hooks
  and initialization tests are preserved in the stash.
- Rendering uses canonical Mikk tangent corners for baked normals, explicit
  render-only original-image vertex remaps and complete weight attributes. Rest
  meshes/skin/morph/source records remain immutable. Protected/fractional normal
  coverage is retained through PBR/MToon composition. UV changes rotate baked
  normal detail into the new Mikk frame. Bake bytes are not counted as paint tiles.
- Shared material time is sampled once per stereo frame. XR explicitly supports
  linear RGBA8 and sRGB8 transport; scene/cursors/BDF palette compose in a GPU
  linear target and resolve once. The synthetic runtime inspects actual released
  textures and retains both format fixtures and ordering/nonblank/menu assertions.

## Incoming7817ca2 bake controls to retain

Mesh → Bake surface exposes hidden/locked source selection, logarithmic world-mm
front/back/AO bounds, integer AO samples/padding, asynchronous Bake/rebake,
progress/status and cancellation. Clear removes only the baked base beneath
editable layers. The native dialog follows Cancel-default Return,
Escape/Ctrl+W/universal/WM Close. File → Cancel background work cancels supported
scene operations. Worker progress/cancellation resources never expose mutable
widgets/model state, and cancellation is checked under the model lock before
publication. Preserve the local no-op weight transaction while merging this lease
release/cancellation path.

Objects → Mesh tools → Baking uses XR page12; weights stay on page13. Source,
bounds and quality panels stage configuration until Done, with no scene/history
mutation; Cancel/focus loss discards staged changes. Bake/Clear explicitly start
one exact global transaction. Existing Sculpt layers remains under Objects.
Incoming native controls and four synthetic XR accept/cancel/focus/undo replays
have their own prior-workspace evidence; this combined source still needs its
own Release, GUI, sanitizer and real-compositor checks before main publication.

## Resume and full remaining scope

Finish the merge with guarded asset work plus no-op weight history, both XR
transport formats and all native menu flows intact. Rebuild coherent archives
before testing; stale public-header/library mixtures produce misleading crashes.
Commit the merge, verify the exact committed source, then push without force after
rechecking remote. Restore the stash and give the agents their preserved tasks.

Continue source-UV bake/adopt with every material role/UV set, original identity,
editable layer and baked base preserved; deferred UV initialization; complete
bind/transfer and fit/rig/morph authoring UI; painted/generated VRM and VRM1;
retopology/cuts/procedural tools; sculpt masks/symmetry/resolution/local meshing;
projection/stencils/texture sets; bake recipes/cages/multiple-source masks and
production quality. Complete XR Unicode and all physical controller/headset/
stylus/3D-mouse and compute backend requirements. Actual CC0 VRoid female/male
remain the primary humanoid baseline, with MakeHuman an explicit alternative.
Keep the full REQUIREMENTS matrix active; do not redefine completion around this
checkpoint or the avatar subset.
