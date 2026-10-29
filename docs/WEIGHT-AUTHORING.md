# Desktop and XR skin-weight authoring

Scene → Skin weights selects the exact native joint, Add/Subtract/Replace/Smooth,
stroke strength, target value, influence cap/prune, local symmetry plane and
hard influence locks. Apply enables the shared weight view. Blue/cyan/green/
yellow/red represent weights from zero to one. Shift+drag paints on desktop;
tracked trigger pressure paints the hand's swept volume in XR. Rig → Weights
offers the same controls with large sliders and explicit joint selection.

Desktop picking uses the worker-built BVH of the currently displayed posed
scene. Unrelated visible surfaces occlude picking. Linked painting includes
visible, unlocked parts of the same explicit native rig group/order; it never
infers joint identities from names. Brush positions are evaluated in the
selected object's current common coordinate frame, including pose, morph and
group/object placement. Symmetry requires explicit humanoid mappings.

A gesture collects bounded capsules, then evaluates their maximum pressure/
falloff once against the original weights. Repeated frame samples do not
accumulate strength. The worker normalizes with hard locks and influence limits;
impossible assignments retain exact weights and report aggregated constraints.
One accepted gesture is one global undo transaction. Cancel/focus loss discards
unconfirmed input. A wholly constrained/no-op stroke creates neither dirty state
nor an undo entry. Rest geometry, editable paint/morphs and original source
archives remain intact. Accepted skin weights persist in native projects and
glTF; tool parameters/locks are current editor settings.

The helper still uses a volume brush and per-mesh adjacency for smoothing.
Interactive automatic binding, weight transfer and durable per-rig tool settings
remain separate production work. The full objective remains in REQUIREMENTS.md.

tests/weight-authoring.cpp independently checks sample-density invariance,
geometric coverage, transformed/hidden parts, locks/cancel, source retention,
posed BVH picking, immutable attributes, desktop/controller transactions and
exact undo/no-op behavior. DOME_MENU_RIG=1 tests/menu.py additionally drives the
actual native controls and viewport gesture through a private universal menu.
