# Authored bind-space authoring foundation

This extends the guarded source-retaining import stage. It does not complete
full affine scene import or FBX/OBJ options. The remaining full objective in
GLTF-AUTHORING-IMPORT.md stays required.

## Native representation and evaluation

A Skeleton may carry a bone-aligned `inverse_bind_override`. Engaged identity
matrices mean glTF's implicit inverse binds; absence keeps the legacy native
inverse(rest) convention. Original trailing accessor matrices and joint-slot
identities remain in immutable original GLB provenance. The adapter does not
prune, normalize, sort or invent weighted influences.

Optional `Bone::rest_matrix` retains exact imported local affine values. TRS
remains the editable reference: an edited local pose evaluates
`TRS(pose) * inverse(TRS(reference)) * exact_rest`. Empty or exactly unchanged
pose uses the exact rest matrix directly. Optional Skeleton `space_matrix`
premultiplies root worlds without introducing a fake joint or source-node ID.
`local_matrices` is the common authority for world evaluation and GLB export.

For currently admitted imported groups (positive similarities), the adapter
retains mesh coordinates, collects actual joint nodes and their ancestors, and
maps every native bone to an explicit original node ID. The separate space is
inverse(mesh node world), so the existing native model transform cancels while
jointWorld * authoredBind supplies the actual glTF skin deformation. This
correctly ignores the instantiating node's transform for skinned geometry.
Original static node transforms are the imported current-pose reference, even
when the native editable pose vector is empty. Native rest and original N/T
transport therefore evaluate skinning with explicit binds at empty pose too.

The frame policy remains explicitly weighted joint inverse-transpose for N and
weighted joint linear matrices for T, followed by existing versioned shader
normalization/interpolation. This change does not invent a universal different
consumer policy. Native compaction retains exact rest/root and corresponding
bind rows.

Skeleton2 stores these optional fields. Legacy Skeleton1 encoding/behavior stays
unchanged. Scene11 is selected by maximum feature requirement, including bind
metadata retained only in source origins after current rig removal. Readers
reject new bind metadata hidden under older Scene versions. Promoting a scene
also emits the complete required extended archive shape. The color preparation
actor owns Scene10; its retained original deformation/material graph must be
included when combining feature scans.

## Export and current boundaries

GLB emits the same effective local matrices, exact override rows, and a separate
root-space node. Both parsed accessor values and independently evaluated world
positions are covered. A glTF local node matrix must be TRS-decomposable. A valid
source hierarchy can produce a sheared world inverse; serializing that inverse
as one matrix node would be invalid. Standalone sheared rest/space export is
therefore explicitly refused until the complete hierarchy-preserving imported
scene export is integrated. New affine/bind VRM0 conversion also refuses rather
than silently reusing the legacy coordinate conversion.

The actual importer still has the similarity-group admission guard. Full
nonuniform/reflected scene nodes, shared joint-node pose editing across
instances and static attachments, arbitrary valid source graph transport, and
all picking/physical painting/cull/remesh consumers remain required. The
standalone native reflected/nonuniform tests do not establish full affine source
adoption. Raw `.gltf` resource bundles/default-material absence, JPEG, per-role
PBR UV1/transforms, generated flat normals, COLOR/UV morph channels and the new
FBX/OBJ options workflow remain active work.

## Evidence

Fresh coherent Release core builds pass authored-bind, actual import-skin,
import-authoring, deformation and scene-schema-compat suites. Independent
analytic matrices cover nonconventional authored and implicit-identity binds,
nonuniform/reflected joints, empty pose and source-node mapping, exact native
restart, dormant-origin version gates, and emitted joint/IBM world positions.
Additional edited-pose cases check positions, N/T matrices and resetting to the
original imported state without rewriting bind bytes.

A fresh C/C++ whole-core ASan/UBSan/leak build passes the five focused native
suites (13.03s); this is focused suite coverage, not a full native-suite claim.
Updated test-only CLI and pose cases are checked separately on that same
instrumented support. A fresh canonical FLTK/OpenXR Release build passes 24
independent original-file software EGL pixel checks with maximum error zero:
explicit/implicit binds, rotated initial joint transforms, P/N/T morphs,
Opaque/Mask/Blend, normal maps and ignored mesh-node transforms. The independent
consumer was corrected to supply identity when the original IBM accessor is
absent; its former exported-file-only fixture had assumed a non-null accessor.

Actual native FLTK Import/Save/Undo/Redo on a private bus/display passes eight
checks with a real source skin, Scene11 restart and byte-exact original input.
No physical headset, new system installation, external push or hardware GPU
execution is claimed for this checkpoint.
