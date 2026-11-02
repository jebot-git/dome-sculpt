# Desktop framing and orbit

Source3c491bf adds View / Frame all (Home) and Frame selection (Shift+F) to the
existing native FLTK application. F retains Reset trackball. Successful imports
and newly loaded source models automatically fit once their current geometry is
ready and editing gestures are idle. A failed/cancelled import does not create
an authoring change. Zoom/clip limits accommodate the framed extent.

Framing reads immutable worker snapshots, visible selection and the same live
world-placement deltas used by rendering. It performs no geometry extraction,
Scene copy or GPU readback. Rotation retains the framed pivot, including after
manual panning, and native inverse-camera rays continue to hit the same model.

Renderer and framing share polygon preview/handoff selection. Bounds include
worker-built polygon geometry, live GPU move offsets, accepted handoffs awaiting
ordinary mesh publication and transformed retopology draft vertices. The
replaced cached mesh does not incorrectly determine polygon framing.

All27 native geometry/ray invariants pass, including portrait/multiselect,
hidden objects, reflected/sheared placement and draft state. The same new inline
helpers pass27 ASAN/UBSAN/leak assertions using unchanged matching04d core/support
archives; this is not a newly instrumented frontend or fresh whole-core build.

The actual native GUI passes12 checks: Home/Shift+F/F, menu routing, off-origin
geometry, orbit, real native Shift painting, ordered Save, other/hidden owner
preservation and one exact Undo/Redo. Camera actions do not create scene history.
Latest private DomeD/native Wayland universal-menu/modal behavior passes8 checks.
Actual-loader synthetic-XR selection11 and polygon41 pass separately. The first
polygon run lacked its optional helper executable; that failed report remains
preserved, followed by a successful helper build and polygon-only rerun.

Earlier383b067 native import checks (VRM11 and atlas12) retain their scope. Visual
review shows the complete avatar inside the viewport, replacing the previous
cropped lower-body view while preserving its original source orientation.
Read validation/DESKTOP-FRAMING.json for exact binaries, sources and evidence.
No Scene/public data layout, physical headset test, system installation or active
session restart is part of this checkpoint. The complete production goal remains
active; draft authoring, interchange and accelerator requirements remain required.
