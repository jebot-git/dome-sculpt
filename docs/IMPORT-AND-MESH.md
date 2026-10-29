# Native import and mesh processing

`import_mesh()` reads static OBJ, glTF 2.0 JSON and GLB into owned native
`ImportedAsset` data. No Blender, Gmsh, MeshLab frontend or Python runtime is
loaded. Import, cleanup, BVH construction and voxelization are worker operations;
the caller publishes the returned result only after successful completion and
checks that the original document generation still matches.

## Preserved data and integration

Each primitive retains its group/node name, material identity and source world
matrix. glTF transforms are baked into positions, inverse-transpose normals and
transformed tangents; reflection reverses winding and tangent handedness. UV0,
UV1, normalized vertex colors, tangents, core metallic/roughness PBR parameters,
alpha mode, sidedness, per-channel texture coordinate sets and samplers survive.
Images retain encoded bytes, MIME type and URI/buffer-view provenance. The caller
must decode supported image formats with explicit decoded-size limits before
creating render textures. Encoded bytes are never treated as executable content.

OBJ polygons are projected and ear-clipped after checking contour intersections.
Negative indices, normal/UV seams, smoothing groups, material libraries and
relative diffuse texture references are supported. MTL Phong values are only an
approximation to PBR and produce a warning. Mixed missing UVs are filled with
zero with a warning. Unsupported OBJ/MTL records produce warnings.

Do not silently reduce this payload to `Mesh`: a scene integration must retain
its additional vertex attributes, material mapping and image provenance, or
reject unsupported data explicitly. `cleanup_mesh()` supplies source vertex and
triangle mappings so external attributes can follow its output. Existing paint
requires explicit transfer/rebaking; replacing topology must not reuse old UV
pixels as if they still matched.

Skinning, morph targets, GPU instancing, compressed geometry, non-triangle glTF
primitives, material extensions and unknown vertex attributes fail explicitly.
Required glTF extensions fail; optional extension presence is reported. Animation
tracks produce a warning and retain only the static node transforms. This is a
static asset importer, not animation/rig import. Materials use owned indices;
there are no parser-owned pointers in the returned asset.

## Validation and bounds

Default limits are 128 MiB input, 256 MiB cumulative resource/allocation quota,
and two million output vertices/triangles. The quota includes parser allocations
and conservative output/temporary accounting; it is not an exact resident-memory
measurement. External resources are optional and confined by canonical path to
the input directory tree, including symlink resolution. Network, absolute,
encoded parent-traversal and escaping resource paths fail. Base64 decoding,
accessor offset/count/stride/component alignment and sparse indices are checked
before cgltf validation or typed access. In particular, zero-count accessors are
rejected before the known cgltf 1.15 count arithmetic edge can be reached.

Geometry uses the application's 100 metre authoring bounds. OBJ faces have at
most 4096 corners and triangulation has a finite work budget. Hierarchy depth,
resource counts, total primitives and integer conversions are bounded. Callback
cancellation throws and publishes no partial result. Callbacks should be cheap,
thread safe and free of side effects.

`inspect_mesh()` reports exact geometric adjacency across duplicated attribute
seams, degenerates, duplicates, unused vertices, boundary/nonmanifold edges,
inconsistent orientation and connected components. It does not merge nearby
positions. Cleanup preserves distinct corner attributes, UV pins and seam flags.

`SurfaceQuery` owns an immutable mesh/BVH snapshot, exposes closest triangle,
barycentrics, interpolated normal and ray distance, and supports concurrent
read-only queries. This is shared with native baking rather than copying a
second triangle query implementation.

`mesh_to_volume()` requires closed, consistently oriented triangle shells and
uses BVH closest distance with odd-even ray parity. Nested shells retain cavities.
It rejects invalid topology, bounds and grids above four million points before
publishing through `Volume::from_samples()`. The band is four voxels; interior
samples remain present. It is a bounded CPU reference path, not GPU acceleration.
Calling `surface_mesh()` on the result performs native scalar-field remeshing.
Self-intersections are not detected or repaired; overlapping shells follow
odd-even semantics. This is not a quad remesher, feature-preserving retopology,
CAD projection, or a replacement for deliberate paint/material transfer.

## Provenance

- cgltf 1.15, MIT, existing pinned header `third_party/cgltf/cgltf.h`,
  SHA-256 `efb169dee911696b5d35fc8e3f7ea0c56d679debc529eba9ca6aa6443ba9d5e9`.
  Used at runtime for glTF parsing and normalized/sparse accessor decoding.
  Upstream: <https://github.com/jkuhlmann/cgltf>.
- Blender commit `3a4070ad41ff715f9c2e1a27f1003fbd01b80cd2`,
  GPL-2.0-or-later, pinned `third_party/blender/math_geom.cc.reference`,
  SHA-256 `68114c7b9742363686ab606c54791b2d745df3cb8c339aaac057397fd3c0c419`.
  The closest-triangle region classification adapts
  `closest_on_tri_to_point_v3`, lines 1045–1178, which credits Ericson's
  *Real-Time Collision Detection*. New integration code is GPL-3.0-or-later.
- Gmsh's official closest-point API documentation was evaluated
  (<https://www.gmsh.info/dev/doc/texinfo/gmsh.html>). It projects to CAD
  entities and can return points outside a trimmed surface; that is not the
  required arbitrary triangle-shell signed-distance operation. No Gmsh or
  MeshLab implementation is linked or claimed as reused in this component.

Central integration must add `src/import.cpp` and `src/mesh-processing.cpp` to
the core target, use `third_party/cgltf` as a private include directory, and add
`tests/mesh-import.cpp` and `tests/mesh-processing.cpp` to CTest. Update the central
third-party manifest to record cgltf's runtime use and the Blender closest-point
adaptation. Only `src/import.cpp` defines `CGLTF_IMPLEMENTATION`; avoid another
implementation definition in a binary that also links that object.

## Focused validation

The tests generate real OBJ/MTL/glTF/GLB files in temporary directories. They
check concave area, material/groups, UV/normal/tangent/color preservation,
reflection and hierarchy, texture provenance, native GLB round-trip, malformed
ranges/alignment, resource escape/quota and explicit unsupported-data failures.
Processing checks use analytic solids and UV seam fixtures, source mapping,
immutable query ownership, physical ray distances, cavity parity, native
extraction, topology rejection, cancellation and grid budgets.

From the repository root after building the native core:

```sh
c++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -Iinclude -Ithird_party/cgltf tests/mesh-import.cpp src/import.cpp src/mesh-processing.cpp build/libdome-sculpt-core.a build/libxatlas.a -lpng -lz -pthread -o /tmp/dome-import-test
/tmp/dome-import-test
c++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -Iinclude tests/mesh-processing.cpp src/mesh-processing.cpp build/libdome-sculpt-core.a build/libxatlas.a -lpng -lz -pthread -o /tmp/dome-processing-test
/tmp/dome-processing-test
```

The focused milestone also runs these two new translation units and their tests
with AddressSanitizer, UndefinedBehaviorSanitizer and leak detection, linked to
the existing Release core archive. This instruments the importer/processing
component; it does not constitute sanitizer coverage of every existing core
translation unit. Logs are written to `artifacts/mesh-import*.log` and
`artifacts/mesh-processing*.log` by the milestone checks.
