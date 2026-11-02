# Native image projection and stencils

This component extends the existing painting tools in the native FLTK/OpenXR
application. The production objective remains active. It introduces no Scene
schema, import format or additional frontend.

## Painting

Prepare or select a painted mesh. **Paint → Image projection → Load projection
image…** selects a PNG, then the bounded file worker decodes it. The canonical
FLTK chooser has its synchronous image preview disabled and fits the current
screen. **Projection from view** fixes the camera to the current desktop view;
**Image projection options…** controls position and projection settings. Its
scrolling body and fixed Cancel/Apply footer support enlarged bitmap fonts.
The image name, loading/error state and enabled tool are visible in the native
status and menu. Shift-drag uses the normal painting gesture.

In XR, open **Materials → Paint tools → Controls → Image projection**. Browse
images, select a folder or PNG, then Open/load. The same page provides image
activation, Color/Stencil, parameter selection and decrement/increment. The
existing color presets remain available from this page. While image or folder
work is pending, Place becomes **Cancel image load / Cancel browsing**. The
existing shared Cancel work command also cancels an image task.

**Place projector** owns a separate controller gesture. Release the trigger,
hold it to position/orient the projector, then release to accept. Focus/tracking
loss or the joystick cancels placement. A held trigger cannot turn a dismissed
menu into a paint stroke: a fresh neutral/release is required. Loading a new
image similarly fences paint admission until it finishes or is cancelled.
Physical held-object painting uses the captured reference geometry and each
sample's current held transform. Projector placement waits until object motion
has finished; painting an already held object remains supported.

The settings include orthographic/perspective mode, dimensions, field of view,
aspect, depth limits, opacity, facing threshold, visible-scene/target-only
occlusion, preview distance, scalar channel, sRGB/linear interpretation,
alpha-as-coverage/channel data, brush tint, UV scale/rotation/offset and flips.
Stencil mode uses the selected scalar as brush coverage. Color mode writes the
chosen color, emission or scalar paint channel through the existing layers.

A translucent image plane and frustum are a placement guide, not a procedural
material layer. Preview can be disabled. Image pixels are uploaded once per
immutable image owner; camera, UV and interpretation changes use uniforms.
Filtering decodes sRGB per texel and handles premultiplied alpha before linear
interpolation. This does not extract or read back scene geometry.

## Persistence and visibility

Image selection, projector settings and placement are session tools. Saving
stores the actual editable paint pixels, not an external-file dependency or a
serialized projector. A completed gesture is one exact global Undo/Redo; Cancel
retains the original scene, source mesh/UVs, rig/morphs and placement. A following
Save or Close uses the existing accepted native-gesture endpoint.

Projection samples use evaluated world-space positions and normals, including
nested groups, pose/skin/morph and reflected/sheared placements. The original
paint UV domain remains unchanged. Image opacity, alpha and scalar coverage are
applied once by the existing painter. No retained normal preparation/token is
rewritten by color projection.

Target self-occlusion uses the native material-aware query. Visible-scene mode
also tests other ordinary visible objects with their actual native alpha/cull,
source sampling and animation-time policy. Hidden objects and editor ghosts are
excluded by editing/display policy; ghost preview opacity is never substituted
for material alpha. Target-only and explicit through-surface choices are
separate controls. Transparent target texels remain paintable.

## Bounds and ownership

PNG input is limited to 64 MiB encoded, 16,777,216 decoded pixels and 8192 pixels on
an axis. Reads check cancellation between 64 KiB chunks and around bounded PNG
decoding. XR directory scans are nonrecursive and bound visited/retained entries
and UTF-8 filenames. Pixel decoding, evaluated surface preparation and visibility
queries run on workers. The painter remains the existing bounded CPU tile
sampler; this component does not claim a GPU painting engine or incremental
geometry processing.

Detached image tasks do not claim an accepted Scene result or make ordinary
Save unavailable. Admission preserves the previous image/request on allocation
failure. Image tasks capture generation/revision/object/edit authority; stale
choosers/options, replacement and superseding requests cannot publish onto a new
target. Explicit Cancel reaches the worker even if the immediate tool-view
update cannot allocate; the error/publication latch recovers once memory returns.

## Qualification

The machine-readable source/binary/evidence manifest is
[validation/IMAGE-PROJECTION.json](validation/IMAGE-PROJECTION.json). It separates
whole-owned-core/support/renderer sanitizers, actual native FLTK, private latest
DomeD Wayland, real-loader synthetic OpenXR and actual A770 image-guide evidence.

The source-space proof includes an independent moving held-object gradient:
expected and actual original-UV center bytes are 93/242; the wrong frozen-reference
answer 188/188 is rejected. Other fixtures distinguish REST/evaluated footprints,
opaque occluders, hidden/ghost objects and actual material/texture alpha holes.
The GPU fixture checks independent sRGB/linear and transparent-fringe oracles,
with zero second-eye or pose-only buffer/texture uploads.

No physical headset/controller/stylus test or production-size performance claim
is added. Existing inherited shared-backend Wayland EGL and standalone-XR
lifetime reports from the 3D-mouse/lifecycle component remain open; no suppression
or canonical-toolkit fork is introduced. The forthcoming separate authored/PBR
preview mode must capture its immutable range policy in native stroke target and
scene visibility queries during semantic integration. UDIMs, multiple texture
sets, broader painting acceleration and the rest of REQUIREMENTS remain required.
