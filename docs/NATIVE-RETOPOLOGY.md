# Native retopology and plane cutting foundations

These APIs implement geometry and persistence foundations for the Retopo Flow
and Divine Cut workflow requirements. They are not complete equivalents of
those products. [RETOPO-AUTHORING.md](RETOPO-AUTHORING.md) describes the native
desktop/XR manual patch editor and scene persistence now using RetopoMesh;
interactive plane-cut controls remain unfinished. Geometry
jobs belong on workers, with document/source generation checks before publishing
immutable snapshots. Existing paint must be transferred or rebaked explicitly.

## Editable source-constrained topology

`RetopoMesh` is a value with separate vertex, edge and triangle/quad face records,
sharing a monotonically allocated 64-bit ID namespace. Copies preserve all live
IDs and the allocation cursor exactly. Successful deletion does not recycle IDs
inside that history branch. Full before/after values provide an exact undo basis;
undo/history integration is provided by the authoring workspace/document. There are no process
pointers in its persistent data.

Vertices retain position, UV and pin state; edges retain endpoint IDs and seam
state; faces retain ordered vertex IDs and material indices. Creating a face
creates missing boundary edges. Wire edges and isolated vertices support drawing
an unfinished patch. Referenced edges/vertices cannot be deleted. Faces and
incident vertex movements are validated transactionally; invalid operations keep
the previous value and ID cursor unchanged. Modeling rejects degenerate/crossing
faces, duplicate faces, inconsistent/nonmanifold edges and disconnected face
fans at a vertex. Wires may touch an existing patch without becoming faces.

`project()` snaps a selected set through immutable `SurfaceQuery` with a maximum
source distance and atomic cancellation. `relax()` computes synchronous (Jacobi)
blended uniform Laplacian updates, projects updates back to the source and keeps
boundary, wire, seam and pinned vertices fixed. Distances, strength and iteration
counts are bounded; face inversion or failed snapping rejects the whole draft.
No curvature-aware stroke inference, patch bridging, automatic quad generation
or edge collapse/extrusion is claimed. The manual editor places/selects points
and moves them through projected gestures.

`triangulate()` chooses valid interior diagonals for concave as well as convex
quads, returning vertex/face source IDs and per-triangle materials. Source UVs,
pins and seam flags survive. Display normals are area weighted. Nonplanar quads
remain editable if their projected contour and resulting triangles are valid;
this is not a guarantee of distortion-free parameterization.

`encode()`/`decode()` implement a versioned native JSON payload. Stable IDs,
allocation cursor, ordered faces, material indices, position/UV floats and flags
round-trip exactly through the encoder. Decode rejects duplicate/reused IDs,
missing references, malformed records, invalid topology and nesting beyond 32
levels before recursive JSON parsing. Input is at most 64 MiB; topology is bounded
to 100,000 vertices/faces and 300,000 edges, within the 100 metre workspace. The
format is independent of the main document; scene version 7 embeds it as versioned
persistent data alongside the triangulated display/paint mesh.

## Half-space cuts and caps

`plane_cut()` keeps `dot(normalized(normal), position) >= offset`. Thus offset
is measured in metres independently of the input normal's length. Input material
indices are optional, one per triangle. Clipped side positions, normals and UVs
are interpolated; surviving original pins/seam flags remain, while new
intersection vertices are unpinned and cut boundaries become seams.

Result triangles retain source triangle IDs and materials. Newly generated cap
triangles use `UINT32_MAX` and the requested cap material. Per-output-corner
barycentrics reconstruct side attributes from the original source triangle, so
colors, additional UV sets or other imported attributes can be transferred by the
caller. Cap corner barycentrics are zero. Cap normals face the removed
half-space; when the original mesh has UVs, cap UVs use a planar metre-scale
projection. UV pixels and arbitrary external paint are not copied implicitly.

Capping requires closed consistently oriented source shells. Cut intersections
use canonical geometric endpoint order so duplicated attribute-seam vertices
produce identical contour coordinates. Contours are checked for closed degree-2
loops and triangulated while retaining collinear boundary vertices; this prevents
T junctions at cut edges. Closed manifold validation runs before publication.
Concave contours and disconnected solid caps are supported. The bounded reference
path runs on the CPU and does not claim GPU acceleration or general boolean CSG.

Cases that need a stronger polygon/solid kernel reject explicitly: plane touching
an existing vertex within epsilon, intersecting/touching contours, nested cap
holes, degenerate loops, nonmanifold inputs or failed final manifold verification.
Source self-intersections are not detected globally; callers must supply valid
non-self-intersecting outward-oriented solids. Plane offset/epsilon adjustment or
a future exact-predicate polygon-with-holes kernel is needed for rejected cases.
An uncapped trim may accept an open source surface. A cut that removes the entire
surface reports an error; a fully retained surface returns exact source mesh
attributes. Output is limited to two million vertices/triangles, contour size to
4096, triangulation work to 32 million tests and voxel-free cancellation points.

## Inspected sources and actual reuse

- Wings 3D commit `2f6672721d83af7bc39717c86deb12a2970ade9f`,
  `src/wings_face.erl`, `outer_edges/2` and `outer_edges_1/2`, lines 305–315.
  Copyright (c) 2001–2011 Bjorn Gustavsson; permissive notice already retained in
  `third_party/wings/license.terms`. Inspected source SHA-256:
  `626d4566ffad4bbd0bd683f0a41d01b572a5153687e126123bcf169b0787f8da`.
  Its paired-edge cancellation informs the explicit face-edge incidence rule
  used to identify fixed patch boundaries; the native implementation also
  handles wires, orientation and nonmanifold diagnostics. No Erlang is linked.
  Source: <https://github.com/dgud/wings/blob/2f6672721d83af7bc39717c86deb12a2970ade9f/src/wings_face.erl>.
- VCGlib (the geometry library underlying MeshLab) commit
  `5cae2abc2f9056785b0537dcbe156c40da2aea20`,
  `vcg/complex/algorithms/smooth.h`, `VertexCoordLaplacianBlend`, lines 407–428.
  Copyright (C) 2004–2016 Visual Computing Lab, ISTI-CNR; GPL-2.0-or-later.
  Inspected source SHA-256:
  `07e5d12924fa7f33eac1b21699c81f01c93472007229cd0a7761b24a25da382f`.
  The blended uniform Laplacian update informs the native relaxation formula;
  our maps, Jacobi snapshots, fixed boundaries, source projection and transaction
  validation replace its mesh container/temporary-data machinery. No MeshLab Qt
  frontend or VCGlib runtime is linked. Source:
  <https://github.com/cnr-isti-vclab/vcglib/blob/5cae2abc2f9056785b0537dcbe156c40da2aea20/vcg/complex/algorithms/smooth.h>.
- `SurfaceQuery` reuses the existing native BVH and Blender-derived closest
  triangle routine documented in `IMPORT-AND-MESH.md`. The plane clipping,
  geometric contour assembly, ear clipping and stable-ID storage are new native
  integration code under GPL-3.0-or-later. No OpenFX or Seamless3d code is included
  in this component; their separate evaluation remains broader project work.

The central third-party manifest records the inspected commit/hash and actual
algorithm use above, without claiming full upstream ports. `src/retopo.cpp` and
`tests/retopo.cpp` are integrated into the core and CTest; the separate authoring
workspace/document suite covers native persistence and shared desktop/XR input.

## Focused verification

Native tests exercise exact snapshots/ID persistence, atomic invalid edits,
orientation and vertex-fan validation, bounded projection, boundary/pin/seam
relax behavior, cancellation, concave quad triangulation, caps on cubes,
disconnected solids and concave prisms, original UV/material/provenance transfer,
resource limits and explicit holes/ambiguity rejection. They do not constitute
XR interaction testing.

```sh
ctest --test-dir build --output-on-failure -R '^(retopo|retopology-authoring)$'
```

Historical kernel-only milestone validation passed 45 assertions in the optimized Release test and in
AddressSanitizer/UndefinedBehaviorSanitizer with leak detection. The sanitizer
build instruments `retopo.cpp`, `mesh-processing.cpp` and this suite, linked to
the existing Release core archive; it does not instrument every core unit.
Logs: `artifacts/retopo.log` and `artifacts/retopo-sanitizers.log`. Current
whole-core and frontend validation is recorded separately in VALIDATION.md.
