# Native pose and shapekey authoring

Scene → Rig and shapekeys opens the FLTK editor for a visible, unlocked mesh
with deformation data. Pick a joint and enter bounded local XYZ rotation
deltas (degrees) or translation deltas (metres). Rotations apply X, then Y, then
Z after the joint's existing local rotation. Reset pose clears existing local
pose overrides before applying entered deltas. Rest skeletons, geometry and
inverse-bind conventions remain intact.

The explicit linked-pose option synchronizes matching native rig parts inside
the selected creator group (or ordinary immediate group). It requires the full
rest skeleton and joint order to match; when both parts retain authored joint
maps, those maps must also match exactly. It does not infer humanoid bindings
from bone names. A locked participating part rejects the complete operation.
Hidden rig parts follow the same pose. Unrelated groups remain unchanged.

Shapekey weights address an exact target slot on the selected mesh, within
[-1,1]. Selecting a target displays its existing weight. Enable Apply selected
shapekey weight to commit it. Other targets, morph deltas, expressions, original
source bytes/images, and source joint/target identity maps remain intact.

In XR, Objects → Avatars → Rig and shapes exposes joint/target navigation,
three-axis rotation deltas, shapekey weight, grouped pose reset, undo/redo and
save/export. Large ray sliders and continuous joystick adjustments use the
existing spatial control system; vertical stick changes the selected XYZ row.
Values are staged until Done/joystick confirmation. Geometry and deformation
validation run on the guarded asset worker. These controls do not provide live
geometry previews while adjusting; accepted edits update the shared viewport.

Apply is one global transaction. Cancel, focus/session loss, and closing the
native modal discard staged values. Worker failures leave the previous scene
intact; generation/revision/object checks reject stale editors. Rest meshes and
paint stay immutable and editable after posing. This is pose/weight authoring,
not full rig placement, morph creation, or skin-weight painting.

Validation: tests/rig-edit.cpp checks independent vertex rotation, group
propagation, exact snapshot/paint retention, lock/cancellation rejection,
native persistence, worker/undo/redo/source guards, controller navigation and
staged confirmation/cancellation. DOME_MENU_RIG=1 runs the focused native FLTK
universal-menu path in tests/menu.py, using a private fixture/display/bus.
