# Current authoring integration; full goal remains active

## Verified8f4d67d and incoming bake controls

Exact detached8f4d67d in artifacts/verify-authoring-merge passes47/47 Release
suites147.34s and5 whole-core ASan/UBSan/leak suites18.24s (GLB interoperability,
baked persistence, source morphs, UV workflow and weight authoring). The C Mikk
dependency and xatlas are instrumented as well. Native menus53/53, rig/weights
12/12, avatar export5/5, baked surface GUI5/5 and latest real DomeD Wayland8/8
pass. Logs use authoring-pinned names. Canonical toolkit and compositor both
come from282467e; no system install or active session restart occurred.

Origin main advanced again to7817ca2, adding desktop/XR bake controls, progress
and cancellation. Preserve and merge it before publishing main. Current clean
8f proof remains valid independently; it is not proof of the upcoming merge.
Pending lazy UV, all-slot sourceUV and new bind/transfer helper work must be
preserved separately through that merge. No full production completion claim.

Read AGENTS.md, REQUIREMENTS.md, DESIGN_CONVENTIONS (parent workspace) and
TOOLKIT.md. Preserve the original objective in HANDOFF-466ebd1.md; no partial
checkpoint completes it. Runtime remains C++20/FLTK/EGL/OpenXR, Python test-only.
Animation and final scene rendering remain excluded; interactive shading is in.
No installation or active compositor/display-manager restart has been performed.

## Current histories and pending merge

Public last local checkpoint was c95208b (application1f6698a). New local agent
milestones a5a2794/69da9df/2eb45ba/032f67a/b1d1223 add VRM0, conformal UV,
MToon/linear rendering and shared source morph transport. Root committed
native/controller weight painting and background export plus integration hooks
in f5b912f. The af492ee merge is now resolved but not yet committed.

Remote main advanced to af492ee during this work: a separate workspace merged
source-aware baking/Mikk tangents and modern archives with c95208b. Fetch is
complete. Preserve BOTH histories, including72d9127/b879ce4/af492ee. Root must
merge af492ee after committing owned changes, resolve coupled paint/render/
export/UV contracts, rebuild/test exact merged source before push; never force.
The remote handoff describes a different workspace, not live local agent state.
Historical handoffs/validation are in git history and remote archived documents.

## Root integration in this checkpoint

- Ordered async Document::export_asset captures committed Scene, returns a future,
  reports failures/status without blocking native UI. Whole-scene GLB preserves
  raw original bindings; selected-avatar VRM exports explicit group only. Native
  chooser/menu and headset Rig→Export avatar route are connected. Unsupported
  paths preserve existing outputs; VRM0 still deliberately refuses generated/
  source-less/painted/unproven metadata cases until implemented.
- Weight gestures collect bounded capsules on input, then compute current posed
  common-frame skin edits on worker. Max capsule falloff applies once to original
  weights: frame density cannot accumulate strength. Explicit locks, cap/prune,
  symmetry map/plane, visible linked parts, exact source tokens, cancel and one
  global Undo. No-op constraints create no dirty/history entry. GPU selected-joint
  heatmap and worker BVH target actual posed geometry. FLTK controls and XR page13
  expose tool/selected joint/mode/strength/value/locks/symmetry/linked parts.
- UV-only methods moved to document-uv-solve.cpp. New heavy operations run on UV
  worker, preserve exact pin/vertex/corner/skin/morph/source mappings and history.
  Accepted Apply survives implicit focus/rebase/shutdown; explicit Cancel still
  invalidates transfer. Original textured source UV modifications remain guarded
  until explicit all-slot bake/adopt is integrated. Initial topology construction
  still eager in this checkpoint; future lazy helper is separate uncommitted work.
- Shared process-relative material clock sampled once per stereo frame. XR uses
  linear RGBA8 or sRGB8 transport explicitly, renders scene/cursors/BDF palette
  into GPU linear target and resolves once. Synthetic runtime inspects released
  texture itself and classifies format-correct display colors, with both format
  fixtures. Original nonblank/menu/field/ordering assertions remain intact.

## Evidence before the pending remote merge

All44 registered suites have passing evidence across42/44 broad run and corrected
individual export/XR runs; do not label this one44/44 run. Failures were test
assumptions: neutralized GLB factors carry baked red in PNG (now independently
decoded), and RGBA8 XR now contains correct linear accent (now format-aware).
- tests-authoring-full.log:42pass; tests-document-export.log:pass6.11s;
  tests-xr-linear-transport.log:full real-loader suite pass68.06s including new
  sRGB plus linear swapchains. Project dirs/reports retained; don't trust stale.
- Native rig+weights GUI12/12: menu-weight-authoring-choice.log, captures in
  artifacts/menu-rig; actual Shift+drag, exact Undo, view settings don't dirty.
- Native avatar chooser/export5/5: menu-avatar-export.log, actual19mesh/1skin VRM.
- UV agent:79existing regressions,43workflow/43instrumented +12realGUI checks.
- MToon agent:54hardware/software/instrumented GPU cases, actual both19part
  sources zero CPU composites/reuploads; full coherent spatial fixture Arc A770.
- Previous exact1f6698a:37Release,4whole-core sanitizer,53general menus,8rig,
 8real DomeD checks. These prove that historical source, not pending merge.

Canonical DomeD remote advanced to282467e during merge validation; eaeb8e0
updates the popup patch. The new22-patch Release toolkit has been built into
artifacts/toolkit-282467e and the client rebuilt against it. A clean latest
Release compositor build is running separately; see TOOLKIT.md. Preserve
unrelated DomeD/qimgvr source edits.
Physical headset/controllers/stylus/3D mouse and accelerator backends remain
unverified; no full production or all-requirement completion claim.

## Live local ownership and next work

Use collaboration.list_agents for live state; names are local to this thread.
- finish_ab3d1: committed MToon; NEW source-uv-bake helper/tests/docs remain dirty,
  not central layout changes. Preserve all slots/material/normal-frame/layers,
  explicit animation freeze/refusal and whole Scene/image/file budgets. Coordinate
  remote canonical baked surfaces/Mikk, avoid duplicate incompatible schemas.
- sculpt_mesh: committed shared morph export; next painted VRM material identity/
  coverage, generated groom/VRM1. Hold overlapping edits during merge; delegate
  export conflict preserving source/morph and baked normal/AO guarantees.
- sculpt_rig: committed integrated UV; NEW lazy-initialization constructor/tests
  plus harmless guards remain dirty. Old Document Begin hook unchanged. Preserve
  these through merge; do not stage accidentally. Async initialization next,
  then source bake/adopt. UV graphics still lacks full Unicode; track requirement.

After merge/pinned verification, continue real weight transfer/auto-bind and rig/
morph creation UI, source UV bake, bake controls, retopology/cuts/procedural tools,
production sculpt masks/symmetry/local meshing and all device/backend rows. Keep
full REQUIREMENTS objective active; do not reduce it to avatars or this milestone.


## Remote source-aware bake merge now in progress

Origin af492ee is being merged preserving both histories. See HANDOFF-af492ee.md
for the other workspace evidence; it is not fresh validation of this merged tree.
Scene-v6 baked persistence, original-aware normal/AO projection, canonical Mikk
tangents and GPU/export coverage must coexist with local MToon/linear targets,
UV/weight/controller authoring and source morph/VRM guards. All conflicts are
resolved, and the full merged Release build succeeds. New regression checks
cover canonical tangent corners plus per-vertex weights, original source UV
identity expansion, baked UV normal rotation and bake memory accounting.
All47 updated-toolkit Release suites pass in147.90s, including the real-loader
synthetic XR client87.87s (artifacts/tests-authoring-merged.log). General native
universal menus53/53 and baked-surface GUI5/5 pass; the blue baked capture was
visually inspected. Native linkage resolves the updated toolkit lib64 files.
These working-tree results include the unstaged additive lazy-UV helper, so an
exact committed-source build and sanitizer checks are still required before push.
