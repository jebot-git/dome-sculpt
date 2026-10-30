# Import ownership, JSON lifetime and cancellable authoring

The complete production objective in REQUIREMENTS.md and HANDOFF-466ebd1.md stays
active. All additional polygon/holding/VRoid-only/FBX-OBJ requirements remain
required. This continues the existing native C++20 FLTK/OpenXR application.

## Current integration

The current checkpoint extends published 8ad6d30. It pins JSON 3.12 with the
upstream allocation-free destructor and conditional scalar-comparison exception
contracts, fixes both captured-directory ownership leaks, and adopts cooperative
atlas cancellation in Scene atlasing, retopology, both cut outputs and native
painting-mesh preparation. The latter uses transactional AssetWork state and the
existing Cancel command; cancellation preserves exact source/history, while
successful retry supports one Undo/Redo. No Scene schema or public data layout
changes. Read IMPORT-SAFETY-INTEGRATION.md and its validation receipt for exact
source/binary/log scopes, including the corrected synthetic-runtime JSON header
link. All consumers must be rebuilt together after the JSON dependency change.

Accepted snapshot exports keep their close/drain semantics. Explicit export-job
cancellation remains required; a shutdown predicate must not discard accepted
output. Import affine/source poses, UV1 sampling, native FBX deformation/materials,
COLOR/UV morphs and broader glTF/VRM remain incomplete. The whole production matrix
also retains its sculpt/paint/remesh/rig/weight/shapekey/device requirements.

## Private combined candidate

The current private work/production-union-7152 combines published7152 with the
qualified color803/950 and complete polygon e899 lineages. It is not yet
qualified or published as a union. All six code/build conflicts are resolved:
shared bitmap/toolkit and JSON interfaces, color/PA paint policy, recursive
Scene12 authority, shared menu parameters and accepted worker ordering coexist.
The polygon standalone library also inherits the pinned JSON interface.

Before publication, fix and test retained polygon color-budget accounting and
color-projection/preparation/current-mesh authority pairing. Color preparations
must not smuggle unbound PolygonAuthored originals through retained receipts.
Prepared-color topology changes remain guarded until an actual transferable
field representation exists. Rebuild every consumer coherently for the merged
public layouts, then run native/GUI/XR/graphics/serialization/fault qualification.
Individual branch receipts remain scoped to their own source and binaries.

## Active delegated work

- merge_contracts integrates full polygon Document/native/XR/Scene12 authority.
  Private default application sculpt→polygon→atlas→save/reopen→source Restore now
  passes 103 Document assertions. Latest main8ad was merged as4318dde; fresh
  current-toolkit native/runtime builds and GUI/XR/SAN qualification remain work.
  Source-COLOR topology and richer primitive/nonplanar operations remain explicit
  followups. Preserve the polygon-authority refusal in prepare_locked when merging
  the new cancellable preparation worker; old SourceAuthored guards stay strict.
- merge_palette has private8031163: main8ad + frozen8d8 colored normal/PBR/MToon
  bake + compact slider wheel fix0f681e8. Preceding coherent union59c passed
  CPU166/OneAPI167 and13 direct Arc fixtures. Current focused16, GUI9+9+7,
  Wayland8, native PCF software/Arc and exported Khronos/VRM checks pass; complete
  own-core sanitizer qualification and final receipt remain in progress.
  General curved/mixed-skin original-frame baking remains required.
- merge_docs froze JSON1651 +FD e167, now integrated here. Separate native affine
  math/source-hierarchy work is on work/import-affine-8ad6d30. It preserves exact
  local TRS chains, reflection/shear behavior and normal/cofactor rules, without
  relaxing current similarity guards or adding Scene fields. Scene13 is only a
  coordinated future feature-version reservation after polygon12. Foundation
  math alone does not complete affine editing/render/import/export.

Only the old8f4d67d rudimentary milestone has physical headset confirmation. The
user ended that test. Synthetic/runtime/GPU fixtures are separate evidence; no
new HMD launch, system installation or active-session restart occurred. Current
DomeD toolkit details remain in TOOLKIT.md, with native BDF/PCF and23 patches.

The primary old frame WIP remains stashc46d1d907a9ca0cdb56cda17e54ab20284939fc1
plus byte-verified artifacts. Keep milestone commits/pushes, preserve all private
work and rebuild coherently after public-layout/header changes. Do not mark the
full goal complete from a subset of passing tests.
