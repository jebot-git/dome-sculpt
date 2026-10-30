# Corner RGBA and original source capture foundation

This isolated native C++ foundation is based on private production union84d9fe6.
It does not remove any existing source-color or prepared-color conversion guard,
add persistent Scene fields, or claim a complete editor/export workflow.

`edit_polygon_with_corner_sources` is an opt-in operation alongside the unchanged
three-argument `edit_polygon` ABI. It records convex transfers from result CORNER
IDs to input CORNER IDs inside the actual geometry operations. Topological vertex
weights alone cannot represent face-side color seams. Extrusion copies the selected
region's boundary sides; inset uses its actual containing-triangle barycentrics;
bridge copies the two endpoint sides; cuts use the geometry's edge parameter;
connect records duplicated endpoints; weld averages only consecutive corners
collapsed on the same face. Move/flip preserve identity, and deletion exposes no
stale current colors. Transfers flatten intermediate corners made during the same
operation. Default uncolored topology arithmetic and its measured3132 allocation
boundaries remain unchanged.

`PolygonColorField` owns immutable initial linear RGBA corner samples and sorted,
convex current-corner provenance. RGB and alpha interpolate independently, in
linear space with straight alpha. Zero-alpha RGB is retained; there is no gamma
conversion, premultiplication, division by color/alpha, position-based seam merge,
texture readback or mutation of the original source. It packs the current field
in the exact triangulated derivative corner order. Both contributor count and
aggregate provenance entries are bounded; cancellation/resource/allocation failures
leave original topology, IDs and field samples exact. A no-op has no replacement.
Dissolve currently accepts only constant affected fields; nonconstant/seamed
regions refuse until field-equivalence analysis can justify retessellation.

The separate detached `prepare_polygon_source_colors` captures an actual imported
GLB COLOR_0 accessor using the existing native source-vertex/corner witness. It
retains detached original paint/appearance metadata and the immutable original raw
asset bytes, and records each topology corner's native and raw accessor indices.
Repeated native-adapter logical corners must have identical color values. Matching
source and derivative triangle bases prove exact original piecewise interpolation;
a globally constant field is independently safe across a changed diagonal.
Nonconstant N-gons with a changed source diagonal remain explicitly refused.
Missing raw accessors, invalid correspondence and already prepared independent
PBR/common planes are not relabelled as original/identity color authority.

## Current scoped evidence

Fresh optimized regular-user CPU Release passes the five focused foundation suites in1.53 seconds:

- 95 analytical RGBA/operation checks and every1340 measured cancellation points;
- 9 actual GLB accessor/corner/basis/source-retention checks and471 cancellation
  points, including transparent uniform N-gons and a real non-affine diagonal
  refusal;
- 4600 measured persistent owned-allocation failures across extrusion, inset,
  edge cuts and weld, preserving source/IDs/RGBA and successful exact retry;
- The original52 polygon kernel checks and3132 allocation boundaries.

See `artifacts/tests-color-foundation-release.log` and CTest's LastTest.log.
A fresh complete-own-core/support C and C++ ASan/UBSan build with leak checking
passes these five plus existing color-preparation and Scene12 archive regressions:
7/7 in23.91 seconds, without reports. See `artifacts/tests-color-sanitize.log`.
The same seven-suite Release scope is recorded separately in
`artifacts/tests-color-foundation-release7.log`. Original paint/source records
remain unchanged after actual accessor capture and detached edits.

## Required continuation

This field is not yet part of PolygonAuthority, Scene persistence, renderer,
exporter, native FLTK or controller state. The proposed durable authority is
geometry-owned, with explicit source-corner provenance and a shared packed-color
view for existing GPU/export consumers. It must retain the strict PA REST/index/UV
witness and original raw normal/frame evidence. Material holders or invented
CreatorAppearance assets cannot replace that authority. Prepared color requires a
separate authenticated current-field descriptor while preserving both color
planes, original paint and immutable initial receipt; the current refusal stays.

Before removing the source COLOR_0 conversion guard, independent actual rendered
and exported field tests, save/reopen, exact Undo/Redo, cancellation/source leases,
layer/material/rig/morph preservation and schema/version admission are required.
Broader N-gon retessellation quality remains active work. Scene13 is reserved for
affine source placement; any new required persisted color fields must use an
explicit coordinated version, with older readers refusing rather than ignoring.
