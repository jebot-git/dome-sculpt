# Native triangle remeshing foundation

`remesh_triangles()` provides a native CPU reference path for isotropic or
adaptively sized triangle remeshing. It implements edge splitting, manifold-link
edge collapse, valence-improving diagonal flips and source-projected relaxation.
It does not implement quad remeshing, automatic production retopology, GPU
acceleration, global collision resolution or a complete MeshLab equivalent.
Desktop/XR controls and persistent document integration remain caller work.

## Source preservation and constraints

The function takes an immutable `Mesh`, optional per-triangle material indices,
optional per-source-vertex absolute target lengths and `RemeshOptions`. All work
occurs in owned draft data. Exceptions, cancellation and exhausted budgets return
no partial result and leave the source unchanged. Publish only after checking the
original document/source generation. Full before/after values provide the exact
undo basis; this component does not own history.

The source BVH is an immutable copy. Initial source charts connect triangles only
across indexed edges that have no explicit seam, material discontinuity, boundary
or sharp crease. Projection searches only the originating chart and requires a
compatible source geometric normal (dot product greater than .1), preventing
projection onto unrelated components, disconnected UV charts or back-facing thin
sheets. A maximum projection distance rejects excessive displacement.

Boundary, seam, material-border, crease and pinned vertices remain fixed during
collapse/relaxation. Protected edges may split, retaining their source polyline.
All attribute copies of an identical geometric boundary edge split together in
canonical endpoint order, preventing T junctions/cracks across duplicated UV
vertices. New cut-free internal edges can remesh normally. Explicit seams and
pins survive in output flags; material identities follow source charts.

Every output index has source triangle identity and barycentric coordinates in
`source_corners`. This supports transfer of additional UV sets, colors, tangents
or other imported attributes, using the original source snapshot. Existing UV0
and display normals are sampled/interpolated from that source; new protected-edge
vertices interpolate within their source boundary. Output triangles carry
material indices. This is geometric/attribute provenance, not a claim that an
existing painted texture remains exact after topology changes. Caller integration
must preserve additional attributes and explicitly rebake/transfer paint or reject
unsupported painted edits. The remesher never silently clears paint pixels.

## Implemented operations

Each bounded iteration runs the enabled stages:

1. Split geometric edge groups above `4/3` of the mean endpoint target. A batch
   changes at most one original edge of any incident face and updates inherited
   protected/seam edges. Project free midpoints to the original source chart.
2. Collapse short interior edges below `4/5` of the mean endpoint target when
   neither endpoint is protected/pinned. Require the manifold link condition,
   distinct surviving faces, nonzero area, compatible face orientation and a
   common source chart. Conflicting neighboring operations wait for another
   iteration. Tetrahedron extinction and duplicate-face singularities reject.
3. Flip eligible internal diagonals only when squared valence deviation improves
   (target 6 for free interior vertices, 4 for protected vertices), there is no
   existing opposite edge, charts/materials agree and both replacement triangles
   retain compatible orientation. The new diagonal midpoint is checked against
   the original surface within the projection bound.
4. Relax free vertices toward a uniform neighbor average, with Jacobi position
   proposals, sequential fold guards and original-chart projection. Pins and
   protected boundaries stay fixed. This is not curvature flow or a guarantee
   of optimal element angles.

Default targets are uniform. With nonzero `adaptivity`, the maximum incident
source dihedral angle generates a target multiplier
`max(.25, 1/(1 + adaptivity*angle/pi))`. It refines bent regions more densely;
it does not claim VCGlib's curvature/radius quality estimator. Explicit absolute
per-source-vertex targets override this automatic field. Split/collapse vertices
inherit averaged targets; target recomputation from the evolving geometry is not
performed.

Each iteration verifies indexed edge orientation, duplicate/degenerate faces and
connected vertex fans. Final geometric topology validation also crosses attribute
seams, verifies component count and requires a closed result for a closed input.
Open manifold surfaces are supported. Nonmanifold or inconsistently wound inputs
fail; global self-intersections are neither detected nor repaired. A complicated
folded source can still need a stronger geodesic neighborhood or exact collision
kernel; source chart/normal/distance constraints are conservative local guards,
not a global embedding proof. No strict Hausdorff error or convergence guarantee
is claimed. Fixed corners, short feature segments and finite iterations can leave
edges outside the target hysteresis band. Invalid candidates may be skipped;
projection/topology/resource failure rejects the job transactionally.

## Limits and cancellation

Defaults cap input/output draft storage at 200,000 vertices and 400,000 triangle
records, 100,000 operations, 20 million edge visits and 20 million cumulative source
triangle predicate tests. Triangle-record quotas conservatively include dead
collapse records until the job ends. Public options permit at most 2 million
vertices/triangle records/operations, 200 million visits/projection tests and 50
iterations; positions use the finite 100 metre workspace. Targets are finite and
between 10 micrometres and 100 metres. These are deterministic work/storage bounds,
not an exact resident-memory byte accounting or a wall-clock promise.

Cancellation is checked through adjacency construction, candidate batches,
validation, projection traversal, output transfer and immediately before return.
Source-query filters must be cheap, thread safe and nonblocking. The new
backward-compatible `SurfaceQuery::closest(Vec3, SurfaceQueryOptions)` overload
supports an optional triangle predicate, cancellation and a per-query triangle
budget; it throws if no eligible source triangle exists. The original
`closest(Vec3)` and raycast interfaces remain available unchanged.

## Actual inspected source reuse

- VCGlib commit `5cae2abc2f9056785b0537dcbe156c40da2aea20`,
  `vcg/complex/algorithms/isotropic_remeshing.h`, SHA-256
  `66c632d3018a6855acbeed0c4102c69b8608dadb2214f4503a51a6de19c3792c`.
  Copyright(C)2004–2017 Visual Computing Lab, ISTI-CNR; GPL-2.0-or-later.
  The inspected `Params::SetTargetLen`(105–108), iteration schedule(290–333),
  `ImproveValence`(679–724), split predicates/stage(728–795) and
  `CollapseShortEdges` link-condition gate(977–1018) inform the native schedule,
  hysteresis, midpoint splitting, link-check collapse and valence swaps.
  Our owned arrays/maps, transactional results, original chart filters, seam
  grouping, provenance and cancellation replace its containers/refinement and
  optimization infrastructure. No VCGlib header/library or MeshLab Qt frontend
  is linked. Source:
  <https://github.com/cnr-isti-vclab/vcglib/blob/5cae2abc2f9056785b0537dcbe156c40da2aea20/vcg/complex/algorithms/isotropic_remeshing.h>.
- Source-projected neighbor relaxation follows the native blended Laplacian
  foundation documented in `NATIVE-RETOPOLOGY.md`; immutable closest queries reuse
  the Blender-derived triangle routine/BVH documented in `IMPORT-AND-MESH.md`.
  New integration and filtered-query code is GPL-3.0-or-later. No new Python,
  Erlang, Qt or GTK runtime is introduced.

Central integration should add `src/remesh.cpp` to the core, `tests/remesh.cpp` to
CTest and update the third-party manifest with the exact inspected commit/hash
and actual algorithm use above. Source identity and external attribute/paint
transfer must be wired before exposing destructive authoring operations.

## Validation

Native fixtures exercise each of split/collapse/flip/relax independently, source
immutability, barycentric/UV reconstruction, fixed corners/pins, sharp closed
solids, duplicated discontinuous UV/material charts, thin parallel disconnected
sheets, extreme-aspect triangle strips, tetrahedron collapse rejection, curvature
and supplied target fields, malformed topology, quotas and early/late cancellation.
Filtered-query tests separately check restricted nearest surfaces, empty filters,
query work budgets and cancellation. These are native synthetic geometry tests,
not XR interaction or production artwork validation.

```sh
c++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -Iinclude tests/remesh.cpp src/remesh.cpp src/mesh-processing.cpp build/libdome-sculpt-core.a build/libxatlas.a -lpng -lz -pthread -o /tmp/dome-remesh-test
/tmp/dome-remesh-test
```

The focused milestone passes 29 assertions in Release and in
AddressSanitizer/UndefinedBehaviorSanitizer with leak detection. The sanitizer
build instruments this new kernel, its suite and `mesh-processing.cpp`, linked to
the existing Release core archive; it does not instrument every core translation
unit. Logs: `artifacts/remesh.log` and `artifacts/remesh-sanitizers.log`.
