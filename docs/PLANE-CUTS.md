# Native plane-cut authoring

Mesh → Plane cut starts a shared native desktop/XR workspace on the visible,
unlocked active object. In XR, use Objects → Mesh tools → Plane cut. While the
workspace is active, a joystick press opens its cut page directly. Ordinary
desktop dragging remains trackball navigation; cutting does not repurpose the
sculpt/paint pressure gesture.

## Plane and material controls

Offset is measured in document millimetres from the displayed source bounding
center. Direction has yaw and pitch sliders in document coordinates. Group/object
placement and the current skin/morph pose are included before choosing the plane.
The native parameter dialogs support drag, wheel and keyboard adjustment; XR
uses the common joystick/ray slider panels. Cap color has four RGBA rows.

From hand positions the plane through the confirming controller/stylus grip,
with its local +Z as the unflipped normal. It uses the device that confirmed the
menu command, including a left-hand menu while the right controller is tracked.
Flip side reverses the retained half-space. Cap toggles a closed cap or open trim.
The cyan plane grid and green retained-side indicator share the scene's canvas
transform. Trackball/hand navigation changes the view without moving the cut
relative to the source.

The viewport clips the original selected surface in the GPU, retaining its
original PBR/MToon texture bindings and shading. The worker appends actual cap
geometry for preview. Other visible objects retain their original geometry.
Settings requests coalesce, but an in-flight finite preview may finish before
the newest request to avoid continuous-control starvation. Status distinguishes
updating/accepted work; Apply requires a valid preview of the current request.
An invalid request retains the last valid preview and reports its error.

Done in a parameter dialog keeps its settings as one local draft history entry.
Cancel, Escape or universal Close restores that dialog's baseline settings.
Undo/Redo route to the plane draft while the workspace exists. Cancel cut or
Escape in the main viewport closes the workspace and restores the unclipped
snapshot. Unconfirmed source edits, saves and unrelated authoring actions are
disabled; they do not silently accept or discard the cut. Tracking/focus loss
cancels an active parameter adjustment while retaining the surrounding workspace.

## Apply and retained assets

Apply creates a new group with an editable shell and, if needed, an independent
cap object. The original object is retained and hidden with its paint layers,
UVs, rig, morphs, source archives and native topology metadata unchanged. Both
new objects are static geometry from the displayed source pose. They use fresh
512-pixel atlases; the shell receives a PBR bake and the cap its chosen material.
The cap material can be edited further using the ordinary object/material tools.

Shell paint transfer uses the cut kernel's exact source-triangle and per-corner
barycentric provenance, checked against target geometry before rasterization.
It does not choose a nearby surface by a projection ray. Base color/alpha, ORM,
emission and canonical tangent-space normals follow the established native bake
samplers, including editable paint layers. Existing occlusion is retained; new
AO rays against the discarded solid are not added. Padding stays within the
existing bake policy. Original full-resolution/source-specific data remains in
the retained source.

The whole operation is one guarded global scene transaction, including group,
visibility, geometry, material and bake changes. Failure/cancellation publishes
no partial scene and retains the draft for correction or retry. Accepted Apply
survives XR shutdown and uses the ordered asset worker. A wholly retained
half-space closes as a no-op without allocating output objects or global Undo.
Native save/open retains both outputs and hidden source through the existing
group/baked-surface codecs; glTF exports the visible shell/cap material primitives.

## Worker and lifetime contracts

Input/GUI controls stage bounded settings. A dedicated cut worker captures the
displayed immutable mesh revision, extracts the selected range in document space
and runs geometry/contour work. Source generation/revision/object and session
guards reject stale work; replacing the document cancels the unconfirmed draft.
Snapshot clipping/cap metadata never enters the authoritative Scene. Closing the
workspace restores the original snapshot immediately and releases obsolete
worker caches outside model/input locks.

The new mapped mode in BakeOptions requires one source triangle per low face
and normalized source barycentrics per low corner. It validates IDs, counts and
reconstructed geometry. Atlas generation must preserve the checked corner order;
a mismatch rejects before publication. Empty provenance keeps the existing
ray/cage baking path. Mapped projection does not consume projection rays; any
requested AO retains its separate ray budget.

## Remaining production scope

This is a manual cut of the active object, not general Boolean CSG. The separate
shell/cap outputs are individually editable. Repeated cutting of a whole generated
multi-part result still needs a combined group/material workflow: selecting its
open shell alone with Cap enabled correctly reports a non-closed source. Turning
Cap off trims only that selected shell and does not trim sibling cap objects.
Do not describe this as a complete Divine Cut equivalent.

The underlying kernel requires consistently oriented closed manifolds for caps.
Nested cap holes, touching/intersecting contours, a plane within the fixed
epsilon of source vertices, ambiguous branches and empty results reject explicitly.
Adjust the plane, or use open trim when appropriate. Solid self-intersections are
not globally repaired. See NATIVE-RETOPOLOGY.md for exact kernel provenance and
limits. The authoring extraction currently expands selected render corners;
production vertex compaction and efficient GPU preview caching remain needed.
Kernel/input/output budgets and scene object/texture limits still apply.

Durable cut recipes, editable bake resolution/quality, group-wide repeated cuts,
general booleans and stronger contour predicates remain unfinished. New shell
appearance is a static PBR bake: it does not recreate every retained MToon role,
animation/UV1 convention, arbitrary imported attribute or source extension on the
new topology. New skin/morph binding/transfer is also unfinished; those originals
remain editable in the retained source. Unconfirmed workspace recovery and
production-size latency/memory need further work. Physical controllers/stylus,
headsets and real accelerator execution remain distinct verification requirements.
The complete goal remains in REQUIREMENTS.md.
