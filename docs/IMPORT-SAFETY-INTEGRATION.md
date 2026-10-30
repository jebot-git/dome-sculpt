# Import ownership and authoring cancellation integration

This extends published 8ad6d30 in the existing native FLTK/OpenXR application. The
full original production objective and all additional requirements remain active.
No Scene version or public data layout changes in this integration.

## Changes

- Vendored pinned JSON3.12 uses the upstream allocation-free link-reversal
  destructor plus all16 conditional scalar comparison exception specifications.
  Full/forward headers retain API/namespace/layout compatibility and are rebuilt
  coherently. Typed-null children from3.12 constructor failure unwind safely.
- Source capture transfers an already-owned directory only after allocating its
  State. The older-kernel ENOSYS path owns its component-walk descriptor across
  filesystem-path/iterator allocation. Parent/outside paths and fallback symlinks
  remain confined. Both previously reproduced descriptor leaks are fixed.
- Existing cancellation predicates now reach real xatlas chart/pack work in
  Scene surfaces, retopology and cut shell/cap publication. Painting-mesh
  preparation uses transactional AssetWork state and the existing Cancel action.
  Failure/cancellation preserves source/history; successful retry is one Undo.
- Synthetic XR runtime links the same JSON interface target. The full actual
  dependency audit finds225 compiled JSON consumers and no system-header mix.
  Both upstream MIT notices accompany staged application artifacts.

Read JSON-MEMORY.md, IMPORT-MEMORY.md, IMPORT-FD-OWNERSHIP.md and
ATLAS-CANCELLATION.md for implementation, original negative evidence and limits.
Exact sources/binaries/logs are in validation/IMPORT-SAFETY-INTEGRATION.json.

## Qualification scopes

The root integrated JSON/descriptor source 32157ba passes all169 CPU Release
suites in 370.43s and23 selected whole-own-core/support ASan/UBSan/leak suites in
179.76s. Each direct-import six-case run injects13883 persistent C++ allocation
failures and396 cancellation sites, checking actual open descriptor counts,
exact original Scene/paint/COW owners/resource bytes and deterministic retry.
The real native external missing-normal glTF chooser/options/Save/Undo/Redo flow
passes13 assertions. Failure injection covers C++ allocations, not arbitrary
libc/kernel or GPU allocation failures.

The later cancellable-authoring/pinned-runtime source c6f0858 rebuilds all targets.
Six affected Release suites pass in 12.56s; eight selected fresh instrumented
core/support suites pass in 116.63s. These include18 actual authoring cancellation
assertions, the existing13 atlas cancellation and17 robustness checks, cut,
retopology, shared Document, snapshot export and native persistent-allocation
regressions. The new authoring fixture fails against321's old call sites; that
negative result is preserved. These are distinct source scopes, not a claim
that every final-source test was rerun under every backend/sanitizer.

All four final matching-header real-loader synthetic XR suites pass in 189.76s,
with actual stereo GL/controller/stylus/session lifecycle and authoring checks.
The runtime/header source, outputs and binaries are recorded in the receipt. Initial partial-build invocations lacked native fixture helper
executables, failed explicitly and were retained; the final invocation uses the
complete build. No physical headset evidence is implied.

DomeD main was rechecked at3024fb6, unchanged from the current canonical helper,
23-patch FLTK and native BDF/PCF runtime qualification. Staged Release startup and
exact MIT notice contents are verified. No system installation, desktop restart
or HMD launch occurs. Positive NVIDIA/AMD hardware proof remains unavailable.

## Required continuation

Accepted snapshot exports keep their ordered close/drain behavior. Explicit
export-job cancellation remains required; shutdown must not discard already
accepted output. xatlas cancellation remains cooperative between vendor
iterations, without a hard latency guarantee.

Full affine/source-pose and UV1 sampling, native FBX deformation/materials,
COLOR/UV morphs and broader glTF/VRM remain incomplete. Polygon mode and general
colored/deforming baking are still private agent work. Production sculpt/paint/
remesh, humanoid/rig/weight/shapekey tools, full device workflows and every other
unfinished REQUIREMENTS row remain required. This checkpoint does not complete
the production goal.
