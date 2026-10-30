# Native desktop/XR retopology checkpoint — 2026-10-30

This stage extends 3aa47e5 on work/native-retopo-authoring. Main implementation:
fdb132b. Final verified application source:
251cd3de1390f6e6748c73aed4428af5f3e41ece. The full production objective remains
incomplete in REQUIREMENTS.md. No agents were spawned.

Read ../AGENTS.md, RETOPO-AUTHORING.md, VALIDATION.md and AGENT-HANDOFF.md.
Previous records are preserved in HANDOFF-3aa47e5.md, HANDOFF-33f26a2.md and
VALIDATION-33f26a2.md; keep all earlier creator/integration history.

## Implemented

- A dedicated native topology worker owns a source-constrained stable-ID patch
  workspace. Desktop Shift+click/drag and controller/stylus pressure share
  place/select/move gestures, ordered triangle/quad creation, wire edges, pins,
  seams, relax/snap and atomic incident deletion. Held gestures have exact local
  cancellation/undo; immutable views are published with source/session guards.
- Desktop Mesh → Retopology and XR page 14 expose actual commands. Joystick
  presses open the topology page while editing; Tool mode changes Place/Move,
  Snap range uses the common slider/ray controls. Desktop has a native BDF modal
  wheel/slider. Universal menu availability, radio state, modal Close/Cancel,
  draft Undo/Redo and source/save isolation are retained. Default drag stays
  trackball navigation.
- Apply creates a separate atlas-ready mesh, retaining/hiding original assets,
  in one guarded global scene transaction. Source paint, rig, morphs, layers
  and archives are preserved. Failure/cancellation retains the draft for retry.
  Reopened patches display their retained high reference only in render snapshots;
  cancellation and unchanged Apply restore the authored mesh preview.
- Scene version 7 stores RetopoPatch source IDs and the independent topology
  payload beside its triangulated painted mesh. Stable IDs/cursor, quads, wires,
  pins and seams survive native save/open and duplication. Older scene formats
  remain supported. glTF export uses the derived atlas triangles.
- Closing a workspace releases its BVH/history on the idle topology worker.
  Accepted Apply survives XR shutdown; wait_asset includes the topology-input
  barrier before waiting for ordered asset work, preventing an early final save.

## Evidence and environment

At fdb132b, all 50 Release CTests passed in 411.19s. The real-loader synthetic
OpenXR report passed all 73 checks (complete=true, failed=0); its script retains
prior XR coverage, not a new retopology-specific loader replay. Final source
251cd3d passed eight focused Release suites in 33.05s and four whole-core
ASan/UBSan/leak suites in 23.94s. The topology suite has 56 assertions including
controller-only authoring, posed/grouped projection, native/GLB persistence,
reference lifetime, exact histories and immediate accepted-Apply shutdown.
Final topology native GUI passed 12/12 and compact 2x BDF 6/6, exit 0. General
native GUI passed 53/53 at fdb132b. VALIDATION.md records exact scopes, logs,
captures and diagnostic attempts; do not merge these into a claim that the
broad run used the later source refinements.

All 30 manifest source hashes and the canonical qimgvr BDF helper match. Use the
unchanged 282467e / 22-patch DomeD toolkit in artifacts/toolkit-authoring, with
its lib directory, as described in TOOLKIT.md and AGENTS.md. Launch with
build/run-dome-sculpt or prepend that directory to LD_LIBRARY_PATH. The
build-sanitize cache instruments C and C++ across the core with UI/XR OFF.
GUI orchestration uses uv with dbus-next, python-xlib and pillow, private
HOME/bus/Xvfb, 90s cold-font startup and 30s interactions. Artifacts/build logs
are ignored local evidence and do not accompany a fresh clone.

All build/test handles were confirmed terminal; inspect actual live jobs before
starting work and never restart solely after an observation timeout. No installed
application/toolkit or active compositor/session was replaced. No fresh physical
device/backend or external Khronos validation is claimed.

## Continue the full objective

Manual topology authoring is a working increment. Continue bridging/extrusion,
automatic/stroke-assisted patches, wire-only object persistence and draft recovery,
source/paint/rig transfer and production topology/overlay latency and memory.
Current topology overlays build bounded CPU line lists per draw; efficient GPU
caching and production-size evidence remain needed. Projection uses geometric
source surfaces rather than alpha-cutout/geodesic behavior. Derived atlas UVs do
not yet honor topology seam/pin metadata as an atlas prescription. Further Apply
creates a new retained version instead of silently repainting/rebinding old data.
Reopening requires its referenced source still exist; deleting a source leaves a
historical ID and gives a clean reopen refusal.

Retain every other REQUIREMENTS.md row: production VR sculpt masks/symmetry/
resolution/local meshing/recovery; texture/stencil/projection/multiset/source-UV/
atlas/bake quality; interactive plane cuts/booleans and procedural workflows;
complete CATS/Auto-Rig Pro/Divine Cut/Retopo Flow/Random Flow basics; rig placement/
fitting/automatic bind/transfer and morph authoring; full MToon/glTF/VRM fidelity;
physical controllers/stylus/3D-mouse/headsets and real OneAPI/ROCm/CUDA execution.
Earlier live-weight fixed-pose/preview/picking contracts remain intact. Cold
Fontconfig discovery is still a canonical qimgvr follow-up.

Fetch origin and inspect current branch/status/history before overlapping work.
Repository: https://github.com/jebot-git/dome-sculpt at
/home/blux/de-xr/dome-sculpt. The parent is not a repository; preserve unrelated
de-xr projects. Do not reset or force-push newer remote work.
