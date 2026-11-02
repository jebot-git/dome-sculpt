# Independent writable-paint sampling — private Scene24 work

The completed goal remains active. This checkpoint is a geometry-only foundation,
not Scene24 persistence, a repaired colour-UV workflow or current-layout texture
compilation. It follows the real partial-Bake colour/coverage failure documented
in NORMAL-COLOR-UV-COMPOSITION.md; that strict failing oracle is retained.

Scene24/material9 are reserved for independent writable-paint sampling. The Paint
root will use a conditional genuine version2 envelope (`version`, `payload`,
`domain`) with no legacy root `mesh`, so the old implicit1 reader rejects it
before bulk decoding. No existing Paint codec, Scene/Material version or public
Paint/Document/render layout has changed in this foundation.

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
The attachment is not yet connected to PaintedMesh at this checkpoint.

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

## Required next integration

SurfaceMap must own the domain and use its corner raster while retaining current
triangle/bary identity; PaintedMesh must seal the mesh/domain attachment. Preserve
all editable common/PBR/ORM/emission/mask/baked grids in that single basis. Add a
separately owned colour sampling node/current-field quality proof, independent
normal replay and conditional shared archive contexts. Audit all current versus
sampling descriptor readers, geometry identity, render attr12, material picking,
UV-canvas, native/controller authoring and actual extra TEXCOORD export. Preserve
authored UV0 and original UV1 or refuse unsupported collisions. Merely exporting
an unused authored layout does not complete compact current-layout compilation;
its measured quality route remains required. No guard is removed by this leaf.
