# Independent writable-paint sampling — private Scene24 work

The complete goal remains active. The immutable geometry foundation is now
connected to an actual SurfaceMap and editable all-channel paint grids in this
private module. This is not Scene24 persistence, a repaired native colour-UV
workflow or current-layout texture compilation. It follows the real partial-Bake colour/coverage failure documented
in NORMAL-COLOR-UV-COMPOSITION.md; that strict failing oracle is retained.

Scene24/material9 are reserved for independent writable-paint sampling. The Paint
root will use a conditional genuine version2 envelope (`version`, `payload`,
`domain`) with no legacy root `mesh`, so the old implicit1 reader rejects it
before bulk decoding. The Paint object now carries an immutable sampling attachment, requiring coherent
ABI consumers. Scene/Material versions and existing Paint codec bytes are
unchanged; context-free serialization and Scene exposure deliberately refuse
the new domain until the paired owning archive closure is implemented.

## Exact geometry contract

`PaintSamplingDomain` is factory-immutable. It owns a detached immutable Mesh basis
and exact source-corner indices. Current authored geometry/UV0 remains separate
from the one editable paint-grid basis. Each source index addresses an ordered
corner of the basis triangles, preserving separate paint UVs when two current
triangles share a welded vertex. Complete source triangles may explicitly move
in the array but cannot reverse, duplicate or disappear. No nearest matching,
field averaging or material/normal authority is supplied by this leaf.

The bounded cancellable factory composes actual source-corner correspondence and
retains the original strong basis owner on repeated operations. Its temporary
raster mesh expands corners while preserving current triangle IDs and barycentric
meaning. SurfaceMap must continue storing current authored geometry and evaluate
positions/normals through those current indices, including posed/affine painting.

`PaintSamplingAttachment` defines the later ephemeral factory seal: exact strong
mesh/domain address and control-block equality. It is held by PaintedMesh outside
SurfaceMap, preventing a SurfaceMap/attachment ownership cycle. The basis itself
never owns SurfaceMap, Paint or Material. Layers, paint values, visibility and
style factors are outside this geometry certificate. Worker/decoder deep proof
must precede sealing; cheap publication must not scan geometry or replay planes.
The attachment is now sealed by successful actual SurfaceMap construction and
retained by immutable snapshots. Paint/style changes do not invalidate it.

## Finished foundation evidence

Release and O1 ASan/UBSan/leak builds each pass62 checks. Twelve real domain capture
allocation boundaries are measured; persistent denial at every prefix preserves
exact caller geometry, existing basis/seams and successful retry. The test also
covers welded discontinuities, explicit triangle permutation, repeated fractional
scale/turn, actual raster face/corner identity, changed REST/normal and evaluated
position refusal, reversed/duplicate/out-of-range mappings, retained/work/temporary
budgets and early/middle/final cancellation. Factory basis does not alias mutable
caller geometry.

These two new translation units are compiled locally against exact d56f215 headers
and linked read-only to its coherent Release or wholly instrumented core/support
archives. The leaf uses the unchanged Mesh API. This is not a new complete-app
build or a Scene24 ABI/GUI/render/export qualification; the exact source/object/
archive hashes and commands are in artifacts/paint-sampling-foundation.json.

## Actual SurfaceMap/grid stage

The constructor rasterizes a temporary corner-expanded basis while keeping its
public Mesh as current authored geometry. Current face/barycentric IDs remain
unchanged, so transformed/evaluated world positions and normals paint the same
retained seams. Every sparse common/PBR/channel/mask layer, original source-map
owner, sharp partial baked coverage, composite cache/image owner and independent
normal/colour metadata is retained exactly. UV1 and raw unprepared normal data
remain guarded pending their own role adapters. The qualified projection metric
fix samples actual evaluated positions/normals.

Fresh coherent Release core and C/C++ support pass the62 geometry checks and84
grid checks. Grid checks include32 measured real persistent allocation prefixes,
all-channel post-UV painting, transformed evaluated stamps and exact cancellation,
hidden layers, sharp coverage, snapshot ownership and10,000 allocation-free seal
checks. A fresh wholly instrumented C/C++ O1 ASan/UBSan/leak build also passes the
new two scopes plus existing texture painting:3/3 in26.17s. Matching Release
three scopes pass in2.75s. These are native module tests, not Scene24 codec,
GUI, controller, GPU or export evidence.

## Required next integration

SurfaceMap and the sealed Painter attachment now preserve all editable grids in
one basis. Add a
separately owned colour sampling node/current-field quality proof, independent
normal replay and conditional shared archive contexts. Audit all current versus
sampling descriptor readers, geometry identity, render attr12, material picking,
UV-canvas, native/controller authoring and actual extra TEXCOORD export. Preserve
authored UV0 and original UV1 or refuse unsupported collisions. Merely exporting
an unused authored layout does not complete compact current-layout compilation;
its measured quality route remains required. No guard is removed by this leaf.
