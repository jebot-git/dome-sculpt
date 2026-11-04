# Authored rest shading at zero morph weights

All-zero morph weights previously recomputed geometric normals, replacing artist
normals even though positions were unchanged. This also made a legitimate rest
surface fail the native baker's rest-pose check. The complete rig implementation
now skips exactly zero targets while validating all weights and the complete
mesh. Empty morph sets and signed zero preserve authored rest attributes. Active
morph evaluation keeps its existing behavior.

Eight native commands pass per optimized Release and owned sanitizer profile:
rig, deformation, source morph export, retained-frame export, shapekey authoring,
Document, selection, and the new real zero-morph Bake/history/save/recovery
workflow. The strengthened final fixture verifies signed-zero persistence itself.
The old core fails the same authored-normal identity checks. Eleven output pairs
are byte-identical across profiles, including an actual VRoid native project.
Both current-layout applications relink and pass their version smoke.

This is a complete changed-TU/component qualification over owned public a2
support, not a fresh whole build or a 610-test pass. The canonical toolkit
configuration registers the new test. Six actual GLBs have zero Khronos errors,
six warnings and eight informational messages, all retained. No GUI, physical
headset, accelerator, system installation or session action is claimed.
Read [the exact receipt](validation/ZERO-MORPH-REST.json).

General curved/skinned baked-normal portability remains unfinished: the separate
private tangent experiment exposes a real image mismatch (13 against the fixed
bound of 4). Its implementation is excluded from this milestone. Every other
unfinished original and additional requirement, including the later controller
tray phase, remains required.
