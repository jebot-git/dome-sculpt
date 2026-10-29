# Scene model and current file format

A scene contains ordered sculpt objects with stable positive IDs. Each object
owns a sparse signed-distance volume, a name, visibility and brush-lock flags,
a translation, unit quaternion, positive uniform scale and PBR material values.
These objects are independently editable clay parts; sculpt modifier/layer
stacks and mesh-authored objects remain to be implemented.

FLTK and OpenXR use the same selected object. Desktop rays, surface normals and
spatial brush poses are transformed into its local coordinate system. Scene
navigation operates on the enclosing canvas. Meshing bakes object transforms
into an immutable combined preview, retaining object/material draw ranges.
File export instead retains each object's local mesh and writes its transform
on a separate glTF node.

Scene metadata commands and completed sculpt strokes share one ordered history.
Metadata history retains volume ownership, while volume transactions retain
exact changed samples. Worker snapshots detach geometry and omit history so
rendering and file jobs cannot observe later edits. Selection alone does not
make a document dirty; it is included when a project is saved. Dirty state tracks
the committed history position and pending stroke changes, so cancellation and
undo back to the saved state clear the unsaved marker.

`.dsculpt` version 2 is native CBOR with a `dome-sculpt` format marker, object
array, next ID and active ID. Objects contain the fields above and their volume
samples. Version 1 volume files upgrade to a single object on read. Undo history,
canvas pose and transient input are excluded. Malformed identities, transforms,
material ranges and volume samples are rejected before replacing a document.
Current persistence limits are 256 objects, four million total samples and a
256 MiB input file. Production-size budgets, local snapshot/meshing caches,
crash recovery and compressed/chunked storage need further work.

Desktop saves and spatial saves/exports share one ordered worker. A desktop save
waits for its queued job; XR commands return immediately to input processing.
Successful saves mark the captured history position clean. Failed writes retain
the old file and changed document, report an error and permit retry. Projects
are atomically replaced using private temporary files.

GLB export currently includes all visible nonempty objects, separate mesh nodes,
uniform transforms, normals, packed UVs and uniform PBR material channels.
Locks do not exclude objects from export. Texture painting, editable persisted
UV charts, full MToon, skeletons, morph targets and VRM serialization remain
tracked in [the full requirements](REQUIREMENTS.md).
