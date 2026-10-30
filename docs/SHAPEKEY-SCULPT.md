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

## Actual desktop gestures and compact workarea

A test-only native 17x17 grid fixture supports visible edge deformation with a
real Shift/left drag. On Undo of target creation, transient selection clamps to
the remaining target; Redo does not invent the previous selection. The GUI
fixture therefore explicitly creates/selects a fresh target before testing its
stroke rather than guessing the transient index after history navigation.

Actual private-display/bus FLTK tests pass 15 normal and 17 compact assertions:
selected-key displacement, all other keys/paint exact, saved weights unchanged,
visible geometry/shading, source-aware global Undo/Redo and finishing the editing
preview. The initial central stroke genuinely changed thousands of pixels by
only 2..4 RGB levels; the final fixture makes an actual visible-edge stroke and
retains the stronger >12-level / >50-pixel acknowledgment. Failure logs remain.
Normal and compact captures were inspected.

The initial main window previously exceeded a 640x360 workarea. Startup and
minimum size now clamp to FLTK's actual workarea, preserving the ordinary
1100x800 viewport on larger screens. Compact captures now cover the complete
600x320 main window (the artifact crops the bottom 40 pixels of status).

This gate used an optimized app.cpp-only overlay against the frozen cc36463
coherent core/render/XR/other frontend libraries and unchanged headers. Its
native grid fixture links the same exact core/support. It is not a fresh whole
frontend sanitizer qualification or a real Wayland/HMD test. Standard reproduction
after a coherent build is DOME_SHAPE_STROKES=1 DOME_SCULPT_TEST_BUILD=build running
tests/shapekey-menu.py, with DOME_SHAPE_COMPACT=1 for the compact variant.

The frozen cc36463 native/core source subsequently passed a fresh whole-own-core/
support C/C++ ASan/UBSan/leak run: 11 selected suites in 83.75s, including the new
draft and controller stroke paths and the existing Scene16 persistent-denial
fixture. Configuration is UI OFF / XR ON; no instrumented frontend, renderer or
loader is built/executed by this selection. Actual persistent denial inside the
native gesture worker's diagnostic catch remains a separate agent scope. The
new test does not qualify all binary/material source allocation paths or the
separate JSON dependency fixes. See validation/SHAPEKEY-SCULPT-SAN.json.
