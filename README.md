# Dome Sculpt

Native C++20/FLTK work toward the full DomeD VR sculpting, material-authoring,
retopology and avatar-production suite described in [the requirements](docs/REQUIREMENTS.md).
This is an initial implementation, not a completed Medium/Substance replacement.
There is no Python runtime, Unity or other engine in the application.

The Blender baseline is pinned **5.4.0 Alpha**, commit
`3a4070ad41ff715f9c2e1a27f1003fbd01b80cd2`. Trackball projection is adapted from
its native viewport navigation. OpenBrush Native's controller profiles and
one/two-hand canvas navigation are compiled into the core. DomeD's BDF appearance
and universal FLTK menu helpers are the same implementation used in the Gmsh
suite. Exact origins, licenses and source checksums are in
[third_party/sources.json](third_party/sources.json). The OpenBrush XR session
transport and palette renderer are not yet connected to this editor.

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DDOME_COMPUTE_BACKEND=CPU -DFLTK_DIR=/opt/fltk-vr/toolkit/share/fltk
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
build/dome-sculpt
```

Requires a C++20 compiler, CMake 3.24+, nlohmann JSON headers, the patched DomeD
FLTK 1.4.5, OpenGL/GLU, GIO, Fontconfig, zlib, X11/Wayland development libraries,
and `wayland-scanner`. xatlas is pinned and vendored, so configuring does not
fetch its sources. `DOME_BUILD_UI=OFF` builds just the native core and tests.
The application's default FLTK backend follows its environment; tests explicitly
use X11 in a private display. No installation or running session changes are
needed to run the build.

`DOME_COMPUTE_BACKEND` selects exactly one implementation at build time:

- `CPU`: portable reference implementation, currently verified.
- `CUDA`: CUDA translation unit; requires nvcc, CUDA runtime and a device.
- `ROCM`: the shared GPU kernel compiled as HIP; requires a ROCm compiler/runtime.
- `ONEAPI`: SYCL GPU kernel; configure with `-DCMAKE_CXX_COMPILER=icpx` and install
  the matching GPU driver/runtime.

Accelerator implementations have not been compiled or run on this machine,
which currently has none of those development toolchains. Requested GPU
backends fail rather than silently changing to CPU. Passing CPU checks does
not establish accelerator correctness or performance.

## Current native workflow

The initial document contains a clay sphere with 30 mm voxel spacing. Ordinary
left drag orbits with an aspect-correct trackball, middle/right drag pans,
and the wheel zooms. Shift+left drag sculpts. `1` selects clay, `2` carve,
`3` smooth, `4` flatten; brackets change brush radius. `F` resets the view.
Ctrl+Z / Ctrl+Shift+Z undo/redo complete strokes. Lost focus or Escape cancels
an in-progress stroke. The viewport is a basic OpenGL diagnostic preview;
MToon and physically based preview shaders are still required.

The sparse signed-distance field supports swept capsule strokes, pressure,
soft unions/subtraction, smoothing and flattening. Transactions retain exact
sample values for undo/redo. The mesher welds shared intersections and discards
degenerate faces. xatlas generates seams and packed UV charts; it preserves the
mapping back to source geometry. Artist seam controls and a UV editor are not
implemented yet.

Ctrl+S saves the volume as a private, atomically replaced `.dsculpt` CBOR file.
The current version stores voxel size and field samples only. Materials,
rigs, editable charts and shapekeys will require a later document version.
Ctrl+O reopens it. Ctrl+E exports the current static mesh as `.glb` with normals,
UV coordinates and a basic glTF metallic-roughness material. This is not yet
full glTF scene support. VRM export is deliberately unavailable until the
humanoid rig, metadata, expressions, material and secondary-motion requirements
are implemented.

The spatial-input core accepts controller or stylus tip poses, pressure,
trigger and joystick transitions. Joystick press anchors a tool menu; stick
direction chooses a tool and the next press confirms. Input is disarmed across
tracking/focus loss and timestamp gaps; idle or held secondary hands cannot
steal an edit. These behaviors are tested with recorded-style samples. There
is no live controller/stylus adapter or headset sculpting path yet.

## Validation

```sh
uv run --with python-xlib --with pillow python tests/gui.py
build/dome-sculpt --export-demo artifacts/sculpt.glb
```

Python is used only for the optional isolated GUI test harness. The core tests
are native C++ and check volume edits, swept strokes, topology, UVs, persistence,
GLB structure, input ownership and OpenBrush canvas transforms. The GUI test
uses a private bus, HOME and Xvfb display to inspect actual FLTK/OpenGL pixels,
carve/undo, orbit and view reset. Its report and images are in `artifacts/gui`.
No physical headset or stylus verification has been performed.
