# Native polygon editing

This is an extension of Dome Sculpt's existing native authoring model. The
current checkpoint implements the detached N-gon kernel, immutable component
workspace and direct OpenGL preview. Native desktop/XR controls, Scene12
authority, retained-source conversion and actual interactive validation remain
required. This is an implementation checkpoint, not a completed polygon UI.

## Authority and operations

`PolygonMesh` stores ordered N-gon faces, per-corner UVs and material slots,
explicit edges/hard flags, vertices, normalized original-vertex provenance and
never-reused component IDs. Corners have independent identities, including at UV
seams. Its triangle Mesh and face/vertex/corner picking maps are derived display
data. Concave simple faces use deterministic checked ear clipping; a diagonal
never becomes the persistent authoring representation implicitly.

Cube and quad-grid primitives contain actual quadrilateral faces. Detached move,
connected-region extrusion, planar convex inset, equal-count face bridge,
seam-aware edge dissolve, vertex weld, shared edge cuts, vertex connection,
face deletion and shell flipping are implemented. Region extrusion duplicates
shared vertices once and creates sides only on selected-region boundaries.
Inset interpolates old face UVs and source provenance. Cut vertices enter both
incident face rings with independently interpolated seam-side UVs; cut and
extruded child edges retain their applicable hard-edge authority. Dissolve
refuses to discard a UV seam, protected hard edge or distinct face material.

Each successful result is a new value. Failure, cancellation, resource refusal
and allocation exceptions leave the original authority and next-ID cursor
exact. A no-op has no replacement, including sub-ULP translation. Component IDs
are retired on removal. `PolygonEdit::first_id` accepts a workspace high-water
ticket, so local Undo/branching can restore the exact old snapshot while newly
created visible components cannot acquire discarded draft IDs. The workspace
must retain this high-water ticket across Undo/Redo, preview and branching;
selection jobs also require their session/source lease.

Validation checks finite values, strict IDs, complete directed manifold edges,
connected vertex links, simple projected face boundaries, nonzero triangulation,
normalized provenance and duplicate faces. Double arithmetic relative to a face
origin avoids a fixed world-area cutoff: valid tiny and translated/large faces
remain representable. Intrinsic folded faces reject. This is not a global
intersection/Boolean repair algorithm: separate faces may intentionally overlap
in a modeling mesh. Concave/nonplanar inset, unequal face-count bridge,
nonmanifold pinches and unrepresentable zero-area results refuse explicitly.

Work and component limits are checked during preparation. Expansion checks peak
vertices/faces/corners before building its output; indexed component lookup
avoids repeated full-table scans. Logical work units count validation and
preparation visits, with bounded map lookup cost; they are not a wall-time or
GPU instruction count. Cancellation applies to validation, preparation,
triangulation and archive decoding. The strict nested authority format is
version1; it is not a premature change of the Scene archive version.

## Source and integration requirements

The kernel preserves per-corner UVs and interpolation provenance, not every
imported source attribute by implication. `from_triangles` creates a canonical
polygon value from indexed positions/UVs and leaves the caller untouched; it is
not permission to replace an original imported mesh without a source-retaining
conversion. The forthcoming Scene12 holder must preserve original paint,
sculpt layers/masks, appearance/material bindings, rig/pose/morphs and exact raw
source; unsupported attribute transfer requires explicit conversion rather than
silent loss. All retained origins must participate in aggregate quotas and the
maximum-required Scene version, including Scene10/11 source features.

The next workspace uses guarded workers and immutable previews, exact local
Undo/Redo, Cancel/Apply, one global history transition and save/reload. It must
check the central held-object editable predicate and edit-authority epoch;
Ctrl+W/Escape must route through local modal editing. Spatial pages19/22 are
reserved, preserving Holding20 and Import21. Native FLTK and real-loader XR
fixtures remain pending. No additional frontend or runtime is introduced.

## Basis

Pinned Wings 3D commit `2f6672721d83af7bc39717c86deb12a2970ade9f` informs
component allocation and editing discipline:

- `src/wings.hrl` defines `we.next_id`, winged-edge orientation and per-side
  attributes. `src/wings_we.erl` lines115–119 allocates never-reused identities.
- `src/wings_extrude_face.erl`, `inner_extrude_attrs` lines96–116 and `region`
  from line131 preserve side attributes and use selected-region outer edges.
- `src/wings_dissolve.erl`, `dissolve_1` from line51 and `simple_dissolve` from
  line88 rebuild and validate boundary topology before accepting a dissolve.

These algorithms are adapted into native ordered N-gons, indexed adjacency and
strong detached results; this is not a mechanical port of Erlang winged-edge
tables. The implementation additionally refuses UV/material/hard-edge loss.
Exact reference bytes and original copyright/license are retained under
`third_party/wings/`; hashes are in `third_party/sources.json`. No Erlang runtime
or Wings frontend is linked.

## Validation

The exact final kernel checkpoint passes52 functional checks and3,132
individually injected C++ allocation boundaries. The same complete polygon
library and fixtures pass ASan/UBSan/leaks2/2 in4.14s; optimized Release2/2 passes
in0.34s. These cover actual edit output, independent area/Euler/manifold oracles,
source/ID persistence, tiny/translated triangles, seam/hard flags, high-water ID
tickets, bounded expansion and every measured cancellation checkpoint. Each
injected allocation preserves the exact old authority and successful retry.
Logs retain initial compile errors and subsequent passing snapshots. This scope
is the polygon library and fixtures; it is not a full-core, native UI, GPU or
headset proof. Native/VR authoring integration remains in progress.

## Worker-ready component workspace checkpoint

`PolygonWorkspace` now provides immutable topology/render/selection snapshots,
actual BVH ray selection of original N-gon faces or nearby visible vertices and
edges, additive/toggle/all selection, bounded local Undo/Redo, exact Cancel,
no-op gestures and a high-water identity ticket across history branches. A
complete topology command is prepared before its local history node is accepted.
Continuous Move publishes only total displacement and the worker-prepared vertex
mask; CPU triangles, normals, UVs and BVH keep the same owners during motion.
Accepted End validates and reconstructs final authoritative geometry once.
An invalid/rejected End retains explicit Cancel ownership. The direct GPU
renderer consumes these immutable snapshots; frontend routing remains pending.

The workspace owns no Document/model authority and does not perform Scene
publication on its own. The central held-object API from `9f188b5` is integrated
on private merge `aa5b026` (with VRoid baseline `47e49217`); a fresh core and the
polygon/holding native tests pass3/3 in0.37s. Existing stored lock/material flags
remain independent of held isolation. Future Document integration must route
component admission and publication through the central editable predicate and
edit-authority epoch and cancel on authority changes. Simultaneous physical
holding/editing additionally requires object-local rays, the GPU effective world
transform and a proved same-object transform rebase. Pinned-only editing must
remain an explicit limitation until that integration is implemented.

The exact workspace implementation passes29 native checks and3,229 individually
injected allocation boundaries (1,925 topology-command;1,304 accepted Move-End),
optimized2/2 in0.73s. The same2 suites pass with complete own C/C++ core support
instrumented by ASan/UBSan/leaks in12.40s. Fault cases retain the exact visible
snapshot, topology/render/selection owners, revision, identity cursor, prior
Undo/Redo and actual successful retry. Initial native tests independently prove
actual BVH selection, immutable preview owners, one local gesture transition,
source persistence, canceled and rejected gestures, empty-mesh Undo and ID
retirement across branching. No actual FLTK, XR, GPU or Scene12 source-conversion
completion is claimed by these workspace tests.

## Direct GPU preview checkpoint

`PolygonRenderer` keeps immutable geometry and selection stores in VRAM. Each
continuous move changes total displacement uniforms only: no geometry/scalar
upload, mapping or readback occurs in either eye. Worker-prepared coefficients
update each N-gon area normal analytically as
`N(delta) = N(0) + cross(delta, coefficient)`. Accepted End reconstructs CPU
authority once; Cancel and local Undo retain the exact old immutable owners.
Geometry and selection buffer replacements are staged, and an actual injected
GL allocation error leaves the old stores drawable and retryable.

Triangles/wires and vertex markers use separate shader programs. A shared
fragment shader branching on both triangle lighting and `gl_PointCoord` lost
all selected markers on the tested Intel Arc path, while software rendering
passed. Dedicated marker shading passes the original strict actual-pixel
oracle without buffer reads, synchronization barriers, flushes or special
hardware tolerances. This identifies a reliable rendering arrangement; it does
not claim a diagnosed upstream driver defect.

The fresh `1f47279`-based coherent Release core passes all five polygon suites:
52 kernel checks, 3,132 kernel allocation boundaries, 29 workspace checks,
3,326 workspace allocation boundaries (1,984 command; 1,342 accepted End), and
25 actual GPU checks. The graphical suite passes on software OpenGL and Intel
Arc A770. It checks independent cube coverage, stereo reuse, continuous movement,
Cancel/Undo, real GL failure/retry, invalid attributes, actual red vertex pixels,
partial-vertex lighting and exact preview-to-accepted pixels, linear output,
alpha and surrounding GL state. Final screenshots are retained in
`artifacts/polygon-gpu-captures-{software,arc}` and were visually inspected.
The same five suites pass with complete own C/C++ core, support libraries and
renderer instrumented by ASan/UBSan and leak detection in14.12s, with no reports.
These are direct renderer/model proofs, not FLTK/XR workflow proofs.

## Verified imported N-gon preparation

`prepare_polygon_topology` recovers source face loops from the native adapter's
version1 `dome_source_polygons` witness. It proves ordered native-to-canonical
triangle correspondence, disjoint/exhaustive face ranges, exact original-corner
attributes and directed triangle-boundary agreement. Shared original control
points must have identical rest positions; different face corners retain their
own UVs and normal-seam hard edges. Legitimate identical native accessor splits
are accepted, incompatible splits refuse. The result retains a packed-corner to
original native vertex map. Missing face metadata produces explicit triangles;
it never infers quads by merging coplanar triangles.

Metadata is read through a bounded native SAX reader, including duplicate-key,
integer/version, depth, array-domain and aggregate work limits. Fault injection
found that a temporary DOM's `nlohmann::json_value::destroy` can allocate inside
its noexcept cleanup and terminate on failure. The new reader builds no DOM and
requires no allocation during cleanup. No allocation sites are filtered out.

This detached helper passes30 functional checks, all565 measured cancellation
checkpoints and all671 measured allocation failures with exact original
positions/normals/UV/indices, source bytes and successful retry. Fresh complete
own C/C++ core/support ASan/UBSan/leak builds pass the same two fixtures in0.70s;
Release passes in0.05s. These are synthetic exact-witness consumer tests; the
combined current native-adapter path, Scene12 retained-origin codec, paint/rig
conversion, FLTK controls and actual XR workflow still require integration and
validation. The helper alone does not authorize loss of source attributes.
