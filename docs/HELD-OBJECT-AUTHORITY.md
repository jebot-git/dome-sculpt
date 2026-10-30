# Held-object editing and ghost isolation

The default controller squeeze continues to move the workspace. Explicit Object
grab mode moves one visible unlocked object instead. One grip moves it; two grips
rotate/scale it. Desktop ordinary left drag picks and moves an object only in this
mode; Shift retains brush editing. Escape or focus loss cancels an unaccepted pose.
Release accepts one transform history node independently of completed brush edits.

The DomeD universal Scene menu and controller Objects / Mesh tools / Holding page
expose mode, Hold selected, Release, Ghost others and individual visibility. Hold
selected pins the target without continuous grip. Objects visibility is a native
document-owned modal with per-object checkboxes and compact appearance support.

ObjectInfo appends editable/ghost flags and an optional current world transform.
EditorState appends object_grab_mode/isolate_held/object_grabbing, held_object and
a monotonic edit_authority. AssetSource adds an optional edit_authority lease;
legacy three-field callers retain admission compatibility, while actual queued
jobs independently capture and verify current authority. SurfaceRange appends
worker-authored object_world/world_frame_valid for per-range GPU motion.
These are ephemeral publication fields; no Scene/native archive schema changes.

The central object_editable_locked(id) requires a visible unlocked object and,
while held isolation is active, the held identity. invalidate_object_authority_locked()
cancels strokes, controls, UV/retopo/cut drafts and queued authoring, then advances
the epoch. Polygon integration will add its cancel hook there and keep new motion
exclusive with changed polygon drafts; pinned held editing remains available.
Future affine object matrices must replace the same centralized world
transform path, not bake preview transforms into source mesh data.

The authority applies to synchronous/native strokes, preparation, sculpt/skin/
bake targets, transform/material mutation, UV acceptance and asset-worker
publication. Generic isolated Scene operations verify unchanged protected COW
owners, revisions and metadata, including group authority, without serializing
or hashing full meshes/images. Ghosts remain eligible read-only bake sources.
Dedicated individual visibility controls remain available. Their explicit saved
visibility edit is independent of ephemeral ghost opacity. Original materials,
alpha, locks, source appearance and exported data never receive ghost overrides.

One controller can physically hold while the other paints, sculpts or brushes
weights. Samples use the effective current transform. Weight strokes temporarily
restrict linked parts to the held target without changing the user's setting.
Accepted weight work completes before grip release commits the pose. World-space
asset operations require release first; Bake is visibly disabled during motion,
and admission rejects it instead of silently using the old committed pose.

AuthorityTransition restores prior permission binding, selection and published
state after publication allocation failure. Cancelled unaccepted work stays
cancelled; this is not a global allocation rollback guarantee for every gesture.

Held motion keeps a local preview only, with no Scene/revision/geometry changes
per hand frame. Release commits one transform history node; cancellation retains
the source. Controller default squeeze still moves the workspace. Explicit object
mode uses one/two-hand navigation on the object and preserves workspace placement.
History restoration and document replacement release obsolete held identities.
Draws combine current effective object world with inverse worker-packed world in
GPU uniforms, avoiding double transforms after accepted geometry publication.
Ghost opacity/tint is a GPU override preserving alpha-mask and culling behavior.
Object-mode picking is prepared by the worker on opt-in. Ordinary scenes do not
allocate its immutable material-aware BVH merely for this feature. Motion does
not rebuild it; new picks require a current snapshot rather than stale authority.

## Qualification

Private source is based on 76edb2d plus 47e49217 VRoid-only, using DomeD 282467e /
FLTK 1.4.5+22. No install, session restart or physical headset testing is included.

* 62 native authority checks: lazy picking, readonly mutation refusal, exact scene,
  stale leases/workers, native readonly-source bake, moving bake refusal, Undo/Redo,
  replacement, ordinary versus explicit controller grip, and 30 injected transition
  allocation failures restoring exact permissions/selection/committed scene.
* 29 actual Document/controller checks: shared registry bounds, Holding dispatch,
  physical hold with other-hand paint/sculpt/weights, accepted-weight release,
  focus cancellation and two-grip scaling. Controller poses are synthetic; these
  checks do not claim HMD usability proof.
* Native FLTK/universal-menu GUI: 11 ordinary checks, including actual mouse pick,
  motion, cancellation with exact pixels/source and accepted Undo/Redo; 9 compact
  checks cover per-object visibility and exact saved scene. Compact coverage does
  not claim a complete main-viewport mouse workflow on 640×360.
* 36 renderer assertions on software GL and Intel Arc A770: exact ordinary-pixel
  restoration, both MToon versions, linked-weight ghosts, immediate visibility,
  and committed packed-world transition. Watched production draws make zero
  glBufferData/glBufferSubData, host readbacks or mappings during ghost toggles,
  stereo reuse and 20 motion frames. The test reads pixels separately as its oracle.
* 35 existing native/transaction/UV/weight/skin/groups/frame regressions pass.
  Existing 81-check MToon rendering also passes on Arc.
* Both new authority/controller suites pass whole-core ASan/UBSan with leak
  detection enabled. The 36-check renderer also passes with the entire core and
  renderer instrumented. No sanitizer suppression is used.

Actual workflow fixtures found and fixed missing object-mode picking outside
weight view, and a spatial_action admission cap rejecting page 20 before its
dispatch table. The common registry now has 24 pages / maximum 23, shared with
the coordinated polygon/import work. Unknown pages cannot alias root actions.
The earlier VRoid test typo spatial_command is corrected to
spatial_action; newly built groups tests verify actual female/male controller
creation. The previous checkpoint's stale groups binary is not counted as proof
of those newly added assertions.

The required colored-target bake Scene10/PBR-plane/quality receipt work remains
parked, incomplete and preserved at artifacts/color-bake-be1858c with its handoff
and hashes; COLOR/UV morph channels also remain required.
