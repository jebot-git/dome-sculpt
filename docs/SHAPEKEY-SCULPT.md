# Selected-key sculpting checkpoint in progress

This extends the source-retaining authoring/tools checkpoint fb29fde. The full
production objective remains active. It is not yet a published or fully qualified
freehand workflow.

ShapeSculptDraft owns a detached single-gesture target workspace. Clay, Carve,
Smooth and Flatten address the selected key at full weight in its current posed
surface. Saved mix weights, REST/UV topology, skin, pose and other targets stay
unchanged. World-space radius and displacement use the actual affine transform;
skinned displacements invert the weighted POSITION matrix, never a blend of
inverse joint matrices. Singular transforms refuse. Fixed gesture-source
neighbors and maximum per-vertex coverage avoid inflation from repeated identical
samples. Pressure changes strength, not radius. Front-facing restriction, bounded
work and cancellation apply before committing detached arrays. Mask and mirror
brush options remain explicitly refused; the separate mirrored-target operation
is already implemented.

The existing ordered native gesture worker now handles selected-key strokes.
Desktop Shift/left drag and native controller aim/pressure enqueue bounded ray
metadata. The worker performs alpha/material-aware picking, builds frame-aware
immutable previews and calls the source-retaining Replace operation for one
accepted history endpoint. Cancel/focus loss restores the earlier snapshot.
Accepted Save captures that exact endpoint through the existing ordered file
path. The viewport uses a temporary one-hot selected-key weight vector; native
projects and exported scenes keep their authored weights. A joystick press during
a target stroke accepts it and opens the shape tools after worker commit.

The algorithm accepts full affine matrices and has an independent mixed-skin,
anisotropic/sheared-world displacement oracle. The Document adapter currently
uses its existing world-transform authority; it does not enable the separate
guarded general affine import work. No additional CPU/GPU readback is introduced.
Physical device, sustained performance and new GPU upload proof remain required.

First fresh whole-core Release evidence: 15 draft assertions, 29 actual
Document/controller stroke assertions (after correcting fixture rays to the
observed XR canvas anchor), and the existing 46 shape-tool assertions pass.
These cover real selected-key mutation, original paint/weights, exact Undo/Redo,
Cancel, immediate Save, focus loss and joystick menu handoff. Logs are
artifacts/tests-shapekey-sculpt-first.log and
artifacts/tests-shapekey-strokes-anchored.log in the primary repository.
After isolating target-only strokes from unrelated volume storage budgets, all
four selected suites pass in 2.39s, also including the original 23 kernel checks.
The coherent Release FLTK/OpenXR frontend builds with the new controls. Actual
FLTK workflows, graphical consumers, OpenXR loader, allocation-denial
and whole-source sanitizer qualification of this new branch remain pending.
The preceding fb29fde sanitizer9/9 result does not qualify these changes.
