# Full Dome Sculpt production goal remains active

Read AGENTS.md, docs/REQUIREMENTS.md, docs/HANDOFF-466ebd1.md and the parent
DESIGN_CONVENTIONS. Native C++20/FLTK desktop/OpenXR remains the required runtime;
Python is test orchestration only. Animation and final offline rendering are
excluded. Every production requirement remains required; a milestone or a
passing build does not complete the goal.

## Current merged and verified application

Application7cc0d42 merges local avatar/weights/UV/MToon/source-morph/VRM0 work
with remote7817ca2 native desktop/XR baking controls. Both histories are retained.
The earlier88cde79 checkpoint is public on
checkpoint/native-xr-authoring-8f4d67d. Remote0141926 then added only a divergent
handoff; that documentation is now being preserved as well. Its
AGENT-HANDOFF.md describes the pre-integration branch, not live local ownership.
Never force-push or discard either history.

Exact7cc source: artifacts/verify-bake-controls-merge. All48 Release suites pass
262.50s, including the real-loader synthetic OpenXR client105.37s. Four whole-core
ASan/UBSan/leak suites pass6.71s (asset worker, bake controls, UV workflow, weights);
C Mikk and xatlas dependencies are instrumented too. Native universal menus53/53,
bake GUI19/19 and real latest DomeD Wayland/universal-menu8/8 pass. Bake dialog
capture was visually inspected. Logs use bake-controls-pinned / bake-authoring-pinned
names; see VALIDATION.md. No proof is implied for subsequent dirty features.

Canonical DomeD main was rechecked at282467e; both clean Release compositor and
22-patch FLTK1.4.5 toolkit include eaeb8e0 popup-parent behavior. Explicit runtime
library prefix: artifacts/toolkit-282467e/lib64. No system installation or active
session/display-manager restart was performed; preserve unrelated DomeD/qimgvr work.

## Physical headset test

The user tested exact8f4d67d on the real WiVRn HMD/controllers and confirmed
rudimentary functionality, then requested ending it. Only that test process was
terminated; recent native recovery files were copied into
artifacts/xr-milestone-8f4d67d. This is basic physical operation, not complete
workflow/stylus/reconnect/performance validation. SIGTERM bypassed the CLI final
save, exposing a real graceful-stop gap; root is fixing it with a lock-free stop
request and real-loader signal/save regressions. Existing shutdown semantics
preserve completed edits and cancel unfinished gestures.

## Preserved unfinished work and ownership

The stash named Preserve lazy UV source UV baking and skin authoring during7817
merge was applied cleanly after7cc was committed; stash@{0} remains a backup.
Do not drop it until its work is safely committed. Shared main has unfinished
features; do not mistake its old build archives for a coherent current ABI.
Query collaboration state for actual live ownership.

- merge_contracts: deferred UV topology initialization on the existing worker,
  bounded/cancellable loading, ready gates, allocation-barrier and GUI/XR tests.
  Isolated7cc+owned changes are in artifacts/verify-lazy-uv-7cc; new tests need
  their CMake registration and readiness fixtures committed together.
- merge_palette: repeated all-slot SourceUV baking/adoption, original layers
  and baked base preservation, immutable SourceMaterialBindings, Scene archive
  dedup/quotas, renderer and mesh-snapshot propagation. Public Scene/SurfaceRange
  layout changes are coordinated but NOT stable until the agent announces a
  barrier. Original CreatorAppearance/source archives remain unchanged.
- merge_docs: painted/source-derived VRM0 materials, forks/dedup/expression remaps,
  consuming the same explicit material-role/freeze descriptor; no implicit
  flattening of unsupported animated texture behavior or loss of source IDs.
- Root: skin bind/transfer helper and desktop/XR workflow, graceful CLI XR stop,
  backend configuration/device validation and integration/publication. New skin
  helper has9 focused native checks, not yet registered or exposed in UI.

Build new integration candidates from frozen source or private worktrees. Never
link tests against pre-header-change archives. Preserve exact cancellation,
source tokens, ordered saves, no-op weight history and paint/UV/rig/morph/source
identities. Commit only owned tested files, with publication coordinated by root.

## Full remaining scope

Follow REQUIREMENTS.md without reducing it to these avatar/bake milestones.
Continue production sculpt masks/symmetry/resolution/local meshing; painting
projection/stencils/texture sets and production baking; complete fit/bind/transfer,
rig/morph creation and editing; retopology/cuts/procedural plugin equivalents;
Unicode throughout XR; full required glTF/VRM semantics and independent consumers;
physical devices and actual accelerator workloads. Intel Arc/UHD expose OpenCL
and LevelZero, but no complete SYCL SDK is installed yet; HIP compiles but no AMD
GPU is present, and CUDA hardware/toolchain is absent. Root tightened the ONEAPI
configure check to require headers/runtime, not merely a compiler accepting
-fsycl. Compile-only or rendering checks do not prove compute-backend execution.
