# Third-party sources

The combined Dome Sculpt application is GPL-3.0-or-later; see LICENSE.
The original notices and licenses on reused source files remain in force.

- Blender 5.4 Alpha source excerpts/reference files: GPL-2.0-or-later.
  Trackball sphere/hyperbola projection is adapted from `calctrackballvec`;
  surface picking adapts `isect_ray_tri_v3` with a relative determinant epsilon.
  `third_party/blender/BKE_blender_version.h` proves version and cycle for the
  pinned source commit. The reference `.cc` files are not compiled wholesale.
  Closest-triangle region classification adapts `closest_on_tri_to_point_v3`;
  sparse locked-weight normalization adapts `BKE_defvert_normalize_ex` mass
  reservation. Exact hashes and deviations are in `docs/IMPORT-AND-MESH.md`
  and `docs/RIGGING.md`.
- OpenBrush Native: Apache-2.0. Its `navigation` and `controller-profiles` sources
  are compiled unchanged. The EGL graphics helper has local exception-safe
  acquisition and exclusive resource-ownership fixes documented in
  `docs/NATIVE-GRAPHICS-LIFETIME.md`. Native session
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
  Compiled by the native static glTF/GLB importer and used by independent export validation.

- ufbx 0.23.1: MIT (selected alternative A). The unmodified native C reader,
  header and complete upstream license notice are in `third_party/ufbx`. Source
  identity and hashes are in `third_party/sources.json`; no FBX SDK is linked.
  The same pinned upstream's validation FBX files and complete license are under
  `tests/data/ufbx-deformers`, with per-file Git blob and SHA256 provenance.
  These are test inputs, not installed assets or additional humanoid bases.

libpng and libjpeg are linked from the system for native image decoding/encoding;
they are not vendored.

Exact repositories, commits, roles and SHA-256 hashes are recorded in
`third_party/sources.json`. Seamless3d, OpenFX and MeshLab are currently
references under evaluation, not bundled or linked implementations. Their GUI
runtimes are not dependencies of Dome Sculpt. Blender Python add-ons named in
the requirements are workflow targets, not proprietary code dependencies.

VCGlib blended uniform Laplacian semantics inform native retopology relaxation;
GPL-2.0-or-later source reference, with exact commit/hash and algorithm bounds in
`docs/NATIVE-RETOPOLOGY.md`. No VCGlib container or MeshLab frontend is linked.
Wings boundary half-edge classification also informs the native topology maps.
Official VRM/glTF specifications are normative avatar/export references; no VRM
schema/code is copied or code license grant inferred. Exact hashes are recorded
in `docs/AVATAR.md` and `third_party/sources.json`.

MakeHuman default geometry/targets/rig/weight arrays and the mesh converter have
been removed. Historical CC0 grants, credits and hashes remain as references in
docs/references/makehuman for existing user projects; no MakeHuman default model
or AGPL application code/Python runtime is bundled.
VRoid Studio historical beta Base_Female/Base_Male are explicitly CC0-1.0 and
retain the original author metadata. Exact original files, SHA256 hashes and
primary-source grant evidence are in assets/humanoid/vroid/SOURCES.json. Current
restricted VRoid presets and proprietary application code are not included.

- JSON for Modern C++3.12.0: MIT, matching full/forward headers pinned under
  `third_party/nlohmann`. Upstream allocation-free destruction and conditional
  scalar comparison exception contracts are backported; unrelated parser/API
  changes are excluded. Original/later notices, exact patch series and hashes
  are retained. All consumers must be rebuilt together.

- LAPACKE/LAPACK 3.12.0: BSD-3-Clause system library, not vendored. The native
  affine export adapter calls fixed-size `LAPACKE_dgesvj_work` with caller-owned
  workspace and the documented bounded Jacobi iteration/status contract. Primary
  revision, exact references and tested packages are in `sources.json`. The BLAS/
  LAPACK implementation stays a system dependency; no backend/session setting
  is changed, and factorization is kept off per-frame rendering.

FreeSpacenav legacy Unix input protocol conventions are reviewed from pinned libspnav (BSD-3-Clause); the independent C++ client has no libspnav runtime dependency. Notice: third_party/spacenav/NOTICE. Spacenavd source is inspected for producer cadence only. Exact commits and source hashes are in third_party/sources.json.

- OpenEXR3.2.4 and Imath3.1.12: BSD-3-Clause system libraries, not vendored.
  The native emission interchange codec writes lossless ZIP FLOAT32 scanlines;
  a separate OpenEXRCore channel/chunk consumer checks the actual sample bits.
  Exact package/header/license evidence is in third_party/sources.json.

- zlib-ng2.3.3, zlib1.3.1 compatibility API: Zlib-licensed system library, not
  vendored. Conditional native HDR package2 uses bounded lossless DEFLATE;
  `third_party/sources.json` records installed package, header and library hashes.
