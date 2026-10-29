# Dome Sculpt

Native C++20/FLTK work toward the full DomeD VR sculpting, material-authoring,
retopology and avatar-production suite described in [the requirements](docs/REQUIREMENTS.md).
The complete production workflow remains under development.
There is no Python runtime, Unity or other engine in the application.

The Blender baseline is pinned **5.4.0 Alpha**, commit
`3a4070ad41ff715f9c2e1a27f1003fbd01b80cd2`. Trackball projection is adapted from
its native viewport navigation; ray/triangle picking is adapted from its native
geometry math. OpenBrush Native's controller profiles and
one/two-hand canvas navigation are compiled into the core. Its direct OpenXR
session, EGL graphics and BDF spatial palette implementations provide the basis
for the connected stereo sculpting path. DomeD's BDF appearance
and universal FLTK menu helpers are the same implementation used in the Gmsh
suite. Exact origins, licenses and source checksums are in
[third_party/sources.json](third_party/sources.json).

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DDOME_COMPUTE_BACKEND=CPU -DFLTK_DIR=/opt/fltk-vr/toolkit/share/fltk
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
build/dome-sculpt
```

Requires a C++20 compiler, CMake 3.24+, nlohmann JSON headers, the patched DomeD
FLTK 1.4.5, OpenGL/GLU, EGL, OpenXR, FreeType, GIO, Fontconfig, libpng, zlib,
X11/Wayland development libraries and `wayland-scanner`. xatlas is pinned and
vendored, so configuring does not fetch its sources. Set both `DOME_BUILD_UI=OFF` and `DOME_BUILD_XR=OFF` to
build just the native core and tests. `BUILD_TESTING=OFF` excludes test harness
dependencies, including Python.
The application's default FLTK backend follows its environment; tests explicitly
use X11 in a private display. No installation or running session changes are
needed to run the build.

`DOME_COMPUTE_BACKEND` selects exactly one implementation at build time:

- `CPU`: portable reference implementation, currently verified.
- `CUDA`: CUDA translation unit; requires nvcc, CUDA runtime and a device.
- `ROCM`: the shared GPU kernel compiled as HIP; requires a ROCm compiler/runtime.
- `ONEAPI`: SYCL GPU kernel; configure with `-DCMAKE_CXX_COMPILER=icpx` and install
  the matching GPU driver/runtime.

The ONEAPI production core and brush path have now run on the Intel Arc A770,
including CPU-reference, native model/undo and missing-GPU checks. Its queue and
bounded device buffers are reused; only changed distances return to the host.
HIP has compile-only evidence on this host; CUDA and AMD hardware execution
remain unverified. Requested GPU backends fail rather than silently changing
to CPU. See [compute validation and scope](docs/COMPUTE-VALIDATION.md).

## Current native workflow

The initial document contains a clay sphere with 30 mm voxel spacing. Ordinary
left drag orbits with an aspect-correct trackball, middle/right drag pans,
and the wheel zooms. Shift+left drag sculpts. `1` selects clay, `2` carve,
`3` smooth, `4` flatten; brackets change brush radius. `F` resets the view.
Ctrl+Z / Ctrl+Shift+Z undo/redo complete strokes. Lost focus or Escape cancels
an in-progress stroke. The viewport uses a native GGX metallic-roughness shader
with editable per-object color, metalness, roughness, emission, opacity and
culling. The viewport also consumes persistent baked normal and AO maps. The View menu
switches between PBR and a simple toon preview; full
MToon properties remain to be added. Layered color, metallic, roughness, emission
and mask painting use a stable mesh and persisted UVs; see [painting](docs/PAINTING.md).

The sparse signed-distance field supports swept capsule strokes, pressure,
soft unions/subtraction, smoothing and flattening. Transactions retain exact
sample values for undo/redo. The mesher welds shared intersections and discards
degenerate faces. xatlas generates seams and packed UV charts; it preserves the
mapping back to source geometry. The native UV editor supports chart/vertex edits, pins, cuts, relaxation and
packing with paint transfer; see [UV authoring](docs/UV.md). Advanced seam-based
re-parameterization and stitching remain unfinished.

Mesh → Voxel remesh... creates a new editable sculpt from the displayed shape.
Choose world spacing in millimetres with the logarithmic slider, wheel or arrow
keys, then explicitly press Remesh. XR exposes the same operation under
Objects → Mesh tools, with ray dragging, thumbstick and fine-wheel adjustment.
The original object stays hidden with its paint, rig and layers intact; these
are not transferred into the new field. One Undo restores the source scene.
Open shells and oversized grids report an error without changing the source.
See [mesh authoring](docs/IMPORT-AND-MESH.md) for limits and remaining work.

Ctrl+S saves the scene as a private, atomically replaced `.dsculpt` CBOR file.
Version 4 stores separate sculpt objects, stable identities, names, selection,
uniform object transforms, visibility, locks, sparse fields and PBR material
values, stable painting meshes, UVs, sparse paint/sculpt layers and native
rig/skin/morph deformation state. Version 5 adds groups and retained creator/source archives; version 6 adds
persistent normal/AO/PBR baked maps beneath editable paint layers. Earlier
volume/scene versions, including the standalone baked-v5 checkpoint, remain readable.
Ctrl+O reopens it. Ctrl+E exports all visible objects as `.glb`, with separate mesh nodes,
transforms, normals, packed UVs, PBR materials and embedded painted PNG maps.
Skin/inverse-bind/all-influence and POSITION morph GLB export are implemented,
with initial untextured static authoring import. Full glTF import and extension
support remain unfinished. VRM export is deliberately unavailable until the
humanoid rig, metadata, expressions, material and secondary-motion requirements
are implemented.

The Scene menu creates empty sculpts or clay spheres, duplicates and deletes
objects, changes selection and toggles visibility or edit locks. Ctrl+D duplicates
and Ctrl+Shift+N adds a sphere. Alt+Enter opens the native FLTK object/material
editor, including position, Euler rotation, uniform scale and material channels.
Each object has an independent field; desktop and spatial brushes transform into
the selected object's local coordinates. Hidden or locked objects reject edits.
Scene changes and sculpt strokes share global undo/redo. Returning to a saved
history position clears the unsaved marker, and cancelling a transient stroke
retains the previous clean state. Desktop and XR writes share an ordered file
worker so an older queued save cannot overwrite a newer save request.

`F6` or View → Start/stop XR starts the native stereo worker while leaving the
FLTK editor available. Both frontends edit the same document and acquire
immutable mesh snapshots. Spatial sculpt input, surface extraction and XR
save/export jobs run on separate workers. Input queues are bounded; focus and
tracking loss cancels the unfinished spatial stroke and requires controls to be
released before a new stroke. Focus boundaries retain earlier queued confirmations;
shutdown drains prior events, and native session resumption reopens input. See
[ordered input](docs/INPUT.md). Runtime reference-space changes rebase the canvas at the announced time
and discard queued poses from the old coordinate space.

A standalone XR entry point is also available:

```sh
build/dome-sculpt --probe
build/dome-sculpt --xr --open model.dsculpt
build/dome-sculpt --xr --frames 120 --save model.dsculpt
```

The default is a DomeD overlay, requiring `XR_EXTX_overlay`,
`XR_KHR_opengl_enable` and `XR_MNDX_egl_enable`. `--exclusive` explicitly selects
a primary XR session. The reference space is STAGE when available, otherwise
LOCAL. Per-eye poses and fields of view come from the runtime.

In standalone XR mode, SIGINT or SIGTERM requests normal shutdown. The current
frame finishes, completed edits and accepted file work drain, and `--save`
writes the final native project. Unfinished gestures are cancelled. This also
releases the OpenXR session without stopping DomeD.

Squeeze one controller to move the canvas; squeeze both to rotate and scale it.
Trigger pressure sculpts at the tracked tip. Press the joystick to anchor the
BDF menu, tilt it to select and press again to confirm. Starting at the right
and proceeding counterclockwise, the twelve root sectors are Clay, Carve,
Smooth, Flatten, Undo, Redo, Save, Export glTF, Objects, Materials, Brush size and
Flow. Objects and Materials open submenus; their Back sector returns to the
root. The object page supports creation, duplication, selection, visibility,
locking, deletion and basic placement steps. The material page offers color
RGBA color sliders, metalness, roughness, opacity, paint tools and PBR/toon preview
selection. Value controls open large sliders: aim and hold the trigger to drag,
or tilt the stick horizontally for continuous adjustment. A scalar control's
vertical wheel is ten times finer; color controls use vertical tilt to select
an RGBA row. Press the stick or ray-click Done to accept. Ray-click Cancel or press Escape
to cancel; lost focus/tracking also restores the original values. Material/layer gestures make one undo step.
Brush size uses a logarithmic slider. A stylus can drag a panel opened by a
controller. See [XR controls](docs/DOMED-INTEGRATION.md). Save uses
the opened project path, or a private
`$XDG_STATE_HOME/dome-sculpt/Recovery-PID.dsculpt` file for untitled work. Export
uses the corresponding `.glb` path. File errors remain visible in the FLTK
status area and retain the changed document and prior saved file.

Touch, Index and Vive profiles provide controller actions. MX Ink uses its
extension-gated calibrated tip pose, middle-button force and tip force bindings;
the stylus can be used alongside a controller for joystick menus. The DomeD
appearance selection supplies the XR palette's BDF face, pixel size, light/dark
theme, accent and 25/50/75/100% chrome opacity, including live updates in standalone
XR mode. Desktop backgrounds use DomeD dark/light grey. Live physical device behavior remains unverified.
Rich spatial document dialogs and the rest of the production workflow remain
in [the tracked requirements](docs/REQUIREMENTS.md).

The universal-menu audit covers main, parameter, UV and file-dialog contexts;
see [the audit](docs/UNIVERSAL-MENU-AUDIT.md).

## Validation

```sh
uv run --with python-xlib --with pillow python tests/gui.py
uv run --with dbus-next --with python-xlib --with pillow python tests/menu.py
uv run --with dbus-next --with python-xlib --with pillow python tests/dome_menu.py
build/dome-sculpt --export-demo artifacts/sculpt.glb
```

Python is used only for test orchestration; the application has no Python
runtime. The core tests are native C++ and check volume edits, swept strokes,
topology, UVs, persistence,
GLB structure, input ownership and OpenBrush canvas transforms. The GUI test
uses a private bus, HOME and Xvfb display to inspect actual FLTK/OpenGL pixels,
carve/undo, orbit, view reset, object commands and the native material editor.
Its report and images are in `artifacts/gui`. A separate private universal-menu
host tests actual DBusMenu state, dialog shortcuts and Cancel defaults.
CTest additionally exercises the shared document workers, real EGL/OpenGL
PBR/toon/textured-surface/palette/slider rendering, and the real OpenXR loader against a native synthetic
runtime. Its checks cover stereo submission, controller/stylus edits, focus
and tracking loss, reference-space changes, object creation, material changes,
layered painting, continuous slider input, file commands and overlay selection.
The installed Monado runtime also completed a bounded 12-frame run with 11
stereo submissions, but reported a simulated HMD and no controllers. Neither
fixture establishes physical headset or stylus behavior. The user subsequently
confirmed basic operation of milestone8f4d67d on the real WiVRn HMD/controllers;
full device workflows remain unverified. Current validation and remaining work
are recorded in [the agent handoff](docs/HANDOFF.md) and
[compute evidence](docs/COMPUTE-VALIDATION.md).


Current DomeD/FLTK toolkit selection, runtime-library precedence and the generated
`build/run-dome-sculpt` launcher are documented in [docs/TOOLKIT.md](docs/TOOLKIT.md).
Read [docs/HANDOFF.md](docs/HANDOFF.md) before resuming the full production roadmap.

Native baking uses pinned MikkTSpace tangent corners, Blender-based watertight
BVH projections and source-guarded object transfer. The bake data/worker API,
UV normal reorientation and GLB normal/occlusion bindings are implemented;
Mesh → Bake surface and XR Objects → Mesh tools → Baking expose source selection,
world-mm sliders, AO/padding, progress/cancel and exact Undo. See
[bake controls](docs/BAKE-CONTROLS.md) and [baking](docs/BAKING.txt).

Reproduce the retained native workflow and open its v6 asset in a private FLTK
GUI check:

```sh
cmake --build build --target dome-sculpt-bake-workflow-test
build/dome-sculpt-bake-workflow-test --artifacts artifacts/bake-workflow
LD_LIBRARY_PATH="$PWD/artifacts/toolkit/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
  uv run --with python-xlib --with pillow python tests/baked_gui.py \
  artifacts/bake-workflow/baked-workflow.dsculpt
```
