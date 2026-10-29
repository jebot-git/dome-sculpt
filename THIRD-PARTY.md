# Third-party sources

The combined Dome Sculpt application is GPL-3.0-or-later; see LICENSE.
The original notices and licenses on reused source files remain in force.

- Blender 5.4 Alpha source excerpts/reference files: GPL-2.0-or-later.
  Trackball sphere/hyperbola projection is adapted from `calctrackballvec`;
  surface picking adapts `isect_ray_tri_v3` with a relative determinant epsilon.
  `third_party/blender/BKE_blender_version.h` proves version and cycle for the
  pinned source commit. The reference `.cc` files are not compiled wholesale.
- OpenBrush Native: Apache-2.0. Its `navigation` and `controller-profiles` sources
  are compiled unchanged, along with the EGL graphics helper. Native session
  lifecycle, controller actions, matrices and BDF palette rendering are adapted
  in `src/xr.cpp`, `src/render.cpp` and `src/palette.cpp`. The graphics helper is
  MIT, with the upstream de-xr notice in `third_party/openbrush/LICENSE.native-graphics`.
  Apache LICENSE and NOTICE are retained beside these files.
- Shared de-xr FLTK appearance, native BDF font and universal menu helpers: MIT.
  Copied from the canonical qimgvr tree used by the Gmsh integration.
- Wings 3D AutoUV: permissive `license.terms`, retained under `third_party/wings`.
  UV-continuity chart partitioning and explicit cuts inform `src/uv.cpp`; native
  disjoint sets replace the Erlang winged-edge traversal. Reference placement
  code was inspected; it is not compiled. No Erlang runtime is linked.
- xatlas: MIT, including the original Thekla/NVIDIA notices in its source.
  Source and LICENSE are under `third_party/xatlas`.
- cgltf 1.15: MIT notice at the bottom of `third_party/cgltf/cgltf.h`.
  Used only by the independent native glTF validation test.

libpng is linked from the system for native PNG encoding; it is not vendored.

Exact repositories, commits, roles and SHA-256 hashes are recorded in
`third_party/sources.json`. Seamless3d, OpenFX and MeshLab are currently
references under evaluation, not bundled or linked implementations. Their GUI
runtimes are not dependencies of Dome Sculpt. Blender Python add-ons named in
the requirements are workflow targets, not proprietary code dependencies.
