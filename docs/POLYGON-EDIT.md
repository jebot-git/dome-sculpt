# Native polygon editing

This is an extension of Dome Sculpt's existing native authoring model. The
current checkpoint implements the detached polygon kernel; native desktop/XR
workspace, Scene12 authority, retained-source conversion, held-object guards and
actual interactive validation remain required. It does not claim a complete
polygon workflow from a command-line kernel test.

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
