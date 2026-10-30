# Holding, shared import and current DomeD milestone

The complete production objective in REQUIREMENTS.md and HANDOFF-466ebd1.md stays
active. Additional user requirements are in ADDITIONAL-REQUIREMENTS-2026-10-30.md.
This extends the existing native C++20 FLTK/OpenXR application.

## Integrated behavior and evidence

The current candidate starts from published3a3d7a2, integrates held6207af3 and
shared import optionse86d012 with semantic mergefea6818, adds atomic admission
regression9c74319, and fixes current BDF-runtime metrics at69bd085. Read
HOLDING-IMPORT-INTEGRATION.md and validation/HOLDING-IMPORT-INTEGRATION.json for
behavior, exact tested sources/runtime/binaries/logs and remaining limits.
PRODUCTION-INTEGRATION.md retains the preceding milestone's separate evidence.

Final code69bd085 qualifies CPU151/151, Arc OneAPI152/152, selected whole-own
C/C++ sanitizer44/44, native FLTK113, current DomeD universal-menu8/8 and all
four real-loader synthetic XR suites on both builds. New exact BDF1x/2x palette
checks also pass on Arc. Eleven earlier direct Arc holding/model/consumer
fixtures are separately scoped to parent9c74319, before the font-only followup.

New behavior includes individual native visibility controls, physical holding,
transparent ghosts and exclusive held editing; shared native/XR import options;
metre conversion before local bounds; retained original FBX/OBJ source; and
transactional browser/admission with stale chooser permissions. Existing generic
transactions, seeded sculpt/bake/paint/UV workflows, projection flags, authored
binds, source receipts, VRoid-only creation and resolved VRM roles are preserved.

Physical confirmation remains limited to the old8f4d67d rudimentary milestone.
The user ended that test. Synthetic real-loader XR, native/controller fixtures
and direct graphics evidence do not imply new physical holding/import proof.
Do not automatically relaunch a physical headset test.

## Active agents and required work

- merge_contracts: Wings3D-style polygon mode. Private stable-ID kernel/workspace,
  point/line/face GPU preview and original source N-gon recovery are tested. The
  Arc point-marker discrepancy is fixed by a dedicated marker shader. Full
  Document worker, native/XR controls, accepted Apply→Save/Close ordering,
  persistence and broad workflow qualification remain ongoing. Keep actual
  polygon editing distinct from the existing retopology patch workspace.
- merge_docs: Scene12 polygon authority/origin DAG archive and compatibility.
  Ordinary legacy/source cases pass privately. Exhaustive allocation injection
  found inherited noexcept JSON comparison/destruction failures. Narrow comparison
  fixes and pinned upstream allocation-free destruction are being investigated;
  full source encode/decode fault safety remains unproven. Do not relabel a plain
  decoder-only passing scope as all-archive OOM safety.
- merge_palette: general colored-source baking. Actual Document bake→UV edits→
  save/reopen→repeat bake and independent PBR/MToon/VRM consumers are progressing
  privately, including genuine newly computed normal frames under existing
  representability guards. Native UI, sanitizer/Arc and broader curved/deforming
  cases remain required. Preserve the parked prototype and its byte-verified
  backups; do not merge an unqualified prototype.

Full affine/source-pose import, native FBX skins/blends/materials, glTF normative
missing normals and broader morph/extension interoperability remain required.
So do the unfinished painting/remeshing, humanoid/rig/weights/shapekeys,
plugin-equivalent, device and accelerator rows. CUDA/ROCm compilation is not
positive NVIDIA/AMD hardware proof. The permanent global heap blackout inside
the installed Level Zero driver remains separately documented in
PAGED-VOLUME-ALLOCATION-VALIDATION.md.

Reserved spatial pages:18 procedural,19 polygon,20 holding,21 import,22 polygon
secondary,23 import options. The shared bound is page23. Rebuild all public-header
ABI consumers coherently after future merges.

## Workspace and toolkit

Current canonical DomeD6feb62b has unchanged FLTK builder/all22 patches from282467e.
Keep the optimized artifacts/toolkit-282467e/lib64 artifact; prepend the new
artifacts/bitmap-runtime-6feb62b/lib native BDF runtime for current validation.
Both compositor/client live library maps and font policy are verified privately.
See TOOLKIT.md and the exact qualification receipt. No system installation or
active desktop/display-manager session restart occurred.

The old primary frame WIP remains preserved as
stashc46d1d907a9ca0cdb56cda17e54ab20284939fc1 and byte-verified artifacts. Preserve
all frozen branches/worktrees/backups/local commits. Milestone commits/pushes
are authorized. Keep the full goal active until every requirement has actual
implementation and appropriate evidence; this milestone does not complete it.
