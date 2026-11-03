# Native surface-constrained retopology

The manual patch workflow uses the existing stable-ID RetopoMesh kernel through
a shared C++ worker workspace. It is available from Mesh → Retopology on desktop
and Objects → Mesh tools → Retopology in the joystick menu. Source geometry may
be a sculpt or a prepared mesh; skinned/morphed sources use their displayed pose.
The workflow is incremental, not a complete Retopo Flow equivalent.

## Editing

Start patch on a visible, unlocked source. On desktop, ordinary dragging remains
trackball navigation; Shift+click places a point or toggles an existing nearby
point in the ordered selection. In XR, touch the reference surface with the
controller/stylus and press/release its pressure trigger. Joystick presses open
the topology menu. Its Tool mode switches Place/Move; opening the menu in an existing workspace offers
Resume patch, which closes the menu without resetting the draft. Starting,
resuming or changing the tool closes the menu so pressure reaches the surface.
Snap range adjusts
projection distance. Distance is measured in document world millimetres and uses
a native wheel/slider dialog or the common XR joystick/ray slider panel.

Select three or four corners in boundary order and choose Make face. Winding
follows the reference surface normal, and a successful face clears the selection.
Connect edge joins two selected endpoints. Seam edge toggles an existing edge's
seam flag; Pin points toggles projection/relaxation constraints on the selection.
Tool mode switches the XR tool; desktop exposes separate radio commands.
Move grabs an existing unpinned point on press and projects its subsequent drag
to the source. Relax smooths the patch while preserving boundaries, seams and
pins. Desktop also offers explicit selected-point Snap and Clear selection.
Delete points removes their incident faces and edges as one validated edit.

Yellow marks ordered selections, green marks ordinary topology, cyan marks
seams, and magenta marks unselected pins. Geometry overlays have a small depth
bias and remain subject to surface occlusion. Projection uses the reference's
geometric surface; it is not an alpha-cutout-aware or geodesic editing brush.

Undo/Redo route to draft history while editing. A held move/placement gesture
forms one draft history entry. Escape/focus loss cancels the held desktop
gesture; Escape with no held gesture cancels the workspace. XR tracking/focus
loss cancels the held gesture while retaining the workspace. Cancel patch
discards the whole unconfirmed workspace. Apply or Cancel must precede source
changes or saves; those commands do not silently accept provisional topology.

## Apply, native persistence and source retention

Apply requires at least one face. It triangulates and generates a paint-ready
512-pixel atlas on the ordered asset worker, then publishes a separate mesh and
hides the previous active object. Existing source paint, rig, morphs, layers and
archives remain retained. Reopening an existing topology object starts from its
stable editable records and its referenced source, rather than reverse-engineering
the generated triangles. Each further Apply creates another retained version;
it does not transfer the previous target's paint or rig onto changed topology.
Use the existing explicit bake/authoring workflows where transfer is required.

Scene version 8 stores each patch's source object ID and independent versioned
RetopoMesh payload alongside the derived painted mesh. Vertex/edge/face IDs,
allocation cursor, wires, unfinished vertices, pins and seams survive save/open.
The atlas is a separate derived UV layout; retopology seam/pin records are retained
for topology editing and are not yet an atlas seam/pin prescription. glTF exports
the triangulated mesh with its atlas. Native quad/edit records remain in the
native scene. Complete legacy retopology-only version-7 scenes remain readable;
version 8 also preserves immutable source materials and dormant deformation.
A patch may retain a historical source ID after source deletion;
reopening requires that source still exist. Duplication retains the records.

Apply is exactly one global scene transaction. An asset failure/cancellation
leaves the original scene unchanged and keeps the draft available for correction
or retry. Generation, revision, active object and workspace-session guards reject
stale work. Replacing the document invalidates unconfirmed workspace jobs.
Accepted Apply survives XR focus/session shutdown. Standalone shutdown cancels
a remaining unconfirmed draft before its requested final save. The asset wait includes the
topology-input barrier before the ordered asset job, so a final committed save
cannot race that accepted publication. Closing a workspace releases its BVH and
draft history on the idle topology worker.

## Worker boundaries and limits

Input queues bounded source-local samples; topology copying, projection, graph
validation, relaxation and undo execute on a dedicated worker. Move samples
coalesce without replacing the initial press or crossing a gesture boundary.
Views and original reference queries are immutable. Ordered asset publication
retains existing cancellation and save/history contracts. Input does not call
the topology kernels or create GL objects.

Kernel bounds remain 100,000 vertices/faces and 300,000 edges per patch. Native
scenes cap aggregate topology records at one million and decoded payloads at
64 MiB. Draft undo retains at most 128 entries and one million topology records.
Projection distance is 1–200 mm; nearby-point selection uses at most 10 mm, scaled
with that distance. Scene object and texture budgets still apply. No automatic
quad generation, edge collapse, multi-point brush motion,
production-scale latency guarantee or physical device validation is implied.

Read NATIVE-RETOPOLOGY.md for the kernel's original provenance, geometric
validation and plane-cut limitations. Plane cuts remain a separate kernel with
their own unfinished interactive workflow. REQUIREMENTS.md preserves the full
production objective; a working manual patch editor does not complete it.

## Boundary bridge and extrusion extension

Select four endpoints as two existing disjoint wire/boundary edges, then use
**Bridge edges** in the universal menu. The XR Connect cell becomes Bridge
when four endpoints are selected. The worker chooses a noncrossing outward
quad; existing vertex IDs, pins, UVs and seams remain intact. The new face
inherits the first selected edge's incident face material (zero for a wire).
Interior edges and invalid/out-of-bound candidates refuse without publication.

Select an edge's two endpoints and choose **Extrude edge**. On the controller
page the mode cell cycles Place, Move, Extrude, then Place when a boundary pair
is selected; otherwise it skips Extrude. Touch the selected edge, then hold
and drag to grow a projected strip. Only two new points and one quad are added
per gesture. Continuing the gesture updates those same new IDs; release selects
the outer edge for the next strip. A click without movement is a no-op.
Returning to the anchor restores the original snapshot; Cancel restores the
complete original gesture. A wire's new face follows the source normal in
either drag direction; adjacent-face boundaries retain consistent winding.

Each bridge or completed strip is one draft Undo. Apply uses the existing
ordered atlas/publication transaction, preserving editable records in native
Save/Open and triangulating for GLB. It retains the existing explicit paint/rig
transfer boundaries. Projection checks endpoints and face center/orientation;
it does not certify every point inside a curved quad.

Qualified source b001ac7 has 5/5 Release and 5/5 selected owned sanitizer suites,
130 native/core/controller workflow checks, 2,978 persistent allocation-denial
boundaries in each build, 15 real FLTK universal-menu assertions and 6 compact
bitmap-font checks. Four actual GLBs (two geometries across both builds) pass
Khronos with zero errors or warnings. Initial wire/control-fixture failures are
retained in the receipt. See validation/RETOPO-EDGES.json for exact scopes.
No new physical-headset, loader, GPU, installation or full-production claim is
made. Production retopology quality/performance and the remaining matrix stay
required.
