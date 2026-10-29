# Desktop and XR skin-weight authoring

Scene > Skin weights selects the exact native joint, Add/Subtract/Replace/Smooth,
stroke strength, target value, influence cap/prune, local symmetry plane and
hard influence locks. Apply enables the shared weight view. Blue/cyan/green/
yellow/red represent weights from zero to one. Shift+drag paints on desktop;
tracked trigger pressure paints the hand's swept volume in XR. Rig > Weights
(page13) offers controller sliders and explicit joint selection. Bitmap hints
are measured before layout; the scroll body and fixed Cancel/Apply footer work
at the tested640x360 screen with2x Unifont BDF.

## Live gesture and publication

A gesture queues bounded immutable capsules. The mesh worker coalesces pending
requests and computes real candidate skin weights from the original scene. Its
immutable heatmap can appear while pressure is still held; the authoritative
Scene, source archives and global history remain unchanged. The posed geometry
stays fixed for the gesture, so capsule coordinates continue to address the
surface being displayed. Release submits the complete gesture to the ordered
asset worker; accepted influences then update the actual deformed mesh.

Maximum pressure/falloff applies once against original weights. Repeated frame
samples do not accumulate strength. Hard locks/influence limits are retained;
impossible assignments remain exact and report constraints. One accepted
gesture is one global Undo transaction. Escape/focus loss or Undo cancels an
unconfirmed gesture and restores its original immutable surface immediately.
Cancellation epochs prevent late previews from reappearing. Replacement documents
reject old-generation results. A failed/no-op final computation restores the
original view without creating history; explicit background cancellation keeps
its existing guarded scene-worker contract.

More samples do not repeatedly cancel an in-flight preview: a finite earlier
batch can finish before the coalesced latest request. Source revision/generation
changes and gesture cancellation still invalidate it. Input queues bounded sample
copies and copy-on-write scene snapshots; geometry, weight calculation, tangent
expansion and BVH/visibility preparation remain on workers. This is responsive
architecture, not a claim of production VR timing at every scene size.

## Posed picking and material visibility

Desktop picking uses the worker-built BVH of the displayed posed geometry.
Triangle winding, authored culling, Opaque/Mask/Blend behavior and fully
transparent texels participate in selection. Unrelated visible surfaces occlude
the selected rig. Filtered traversal finds the next visible face without stepping
past nearby geometry by an epsilon. Picking normals also use the posed snapshot.

Raw and derived PBR alpha views retain wrap/filter/affine UV semantics. V0/V1
MToon picking shares the bounded retained-material parser, source-coordinate and
factor helpers with GPU binding. Authored V1 UV1, source animation/masks, native
opacity overrides and painted coverage are retained. V0 uses its representable
UV0 model. The native canvas supplies its last drawn material-frame time and
rejects a stale displayed geometry revision before sampling, avoiding a pick
against a newer animation phase than the image on screen. The heatmap renderer
binds each material independently, retains alpha/culling/blend state and omits
outlines from weighted surfaces while using the unlit weight-color ramp.

Linked painting includes visible, unlocked parts of the same explicit native
rig group/order; it never infers joints from names. Positions include the current
morph, pose and object/group placement in the selected object's common frame.
Symmetry requires explicit humanoid mapping. Rest geometry, editable paint/morphs
and original source archives remain intact. Accepted skin weights persist in
native projects and glTF; tool parameters/locks are current editor settings.

## Limits and evidence

The brush remains a volume brush with per-mesh smoothing adjacency. It does not
clip the entire swept volume by occlusion or texture alpha, and linked parts are
selected by object visibility/lock. Picking samples base-level nearest/bilinear
source views; distant mip/anisotropic coverage is not a pixel-exact picking
claim. Weight-dependent deformation refreshes after release. Incremental stroke
acceleration, large-asset memory/frame-time validation, automatic binding/transfer
and durable per-rig tool settings remain production work. The full objective is
in REQUIREMENTS.md; no physical-device/backend completion is implied.

Native weight-authoring fixtures cover live-before-release feedback, immutable
source/baseline surfaces, sample-density invariance, rapid cancellation, exact
Undo/no-op/failure restoration, accepted deformation, rotated normals and actual
CC0-avatar generation changes. material-visibility adds22 native checks; the
production MToon/linear GPU suite now has75 checks. DOME_MENU_RIG=1 tests/menu.py
checks actual held viewport painting, Escape and exact Undo (14 checks).
DOME_MENU_RIG_COMPACT=1 checks2x-BDF scrolling/footer/cancellation (4 checks).
See VALIDATION.md for exact source scopes and logs.
