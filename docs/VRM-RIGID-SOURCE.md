# Mixed skinned and rigid source VRM0 authoring

This private component extends the qualified original-space exporter in
VRM-SOURCE-GRAPH-EXPORT.md. It does not complete the full production objective.
There is no new public field, archive version, dependency or frontend.

Imported rigid parts retain their real source nodes and mesh identities. A plain
rigid part has no deformation; a rigid morph part has morph authority without a
skeleton or skin. Export never fabricates joints, weights or dummy skins for
either. At least one authenticated complete humanoid skin is still required by
the current VRM0 avatar route. VRM1 writing and static-only avatars remain open.

## Durable attachment and authoring behavior

`source-rigid-pose.hpp/.cpp` reads typed immutable model/node metadata and native
joint matrices on the authoring worker. It does not parse raw JSON or run on a
per-vertex, UI-lock or per-frame rendering path. Linked imported pose edits match
the model, original byte owner and source-instance root, then remap original
joint IDs across distinct skin bind spaces. Conventional creator groups retain
their existing exact-rig policy. Bone names never establish correspondence.

Every shared source-joint view must agree before applying the edit. A single
unsynchronised authored pose and conflicting authored poses have distinct
diagnostics. Different rest matrices/TRS references or missing identity tables
refuse. Locked affected skins or rigid descendants reject the whole operation.
The source-rest/skeleton-parent migration is not admitted by this component.

For each rigid descendant, the worker evaluates its source attachment frame
before and after the linked pose. If those frames are `F0` and `F1`, its desired
world becomes `F1 * inverse(F0) * previousWorld`. The result is stored in its
existing exact `NativeAffineEdit` in the same detached Scene transaction as the
skin poses. No rendered frame borrows another live skin's pose. The attachment
therefore survives Save/reopen, immutable origin capture and deletion of a skin
owner; unrelated rigid roots retain their original placement. Undo/Cancel use
the existing source-preserving transaction boundaries.

The ordinary native world-matrix consumers then handle picking, inverse-transpose
normals, spherical world-metric paint and physical holding without new transform
shortcuts. Held movement requires the existing Object Grab mode and retains its
normal isolation/edit-authority checks.

## Export and source identities

Each rigid surface is attached below its original emitted source node. Its local
residual is `inverse(emittedSourceNodeWorld) * desiredNativeWorld`. Existing
validated affine factorization represents shear/reflection as a valid node chain;
the mesh node's former native TRS is removed to avoid double application. This
also preserves independent rigid object placement/holding edits. Skinned parts
continue to use the common source joint graph and their distinct current native
inverse-bind accessors.

Completeness is checked by original node/mesh/primitive instance, including
multiple nodes using the same source mesh. The check uses bounded count and
membership against the already emitted instance set, with cancellation per
node/primitive. It never builds an expanded potentially huge original-instance
set. Missing even one shared-mesh instance rejects before atomic replacement.
First-person mesh annotations expand to the exact emitted mesh identities;
author/license/usage metadata remains intact.

This follows glTF's distinct node-transform rule for geometry attached to a joint
without skinning; skinned geometry instead uses its joint transforms. Primary
reference: [glTF2 skins](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#skins),
inspected revision `8e798b02d254cea97659a333cfcb20875b62bdd4`.

## Actual qualification

Both actual CC0 female/male nineteen-part avatars are extended by three explicit
source fixtures: a head-attached rigid badge with a morph, a plain independent
rigid badge, and another node instancing the same plain mesh. These are authored
external test assets, not runtime substitutions. Twenty-two source parts retain
their original ownership through rest, linked pose, current morph and independent
reflected/sheared rigid placement.

The native oracle independently parses original/emitted accessors, checks every
original inverse-bind float bit and every ordered current world corner at2um,
and verifies metadata, first-person references, exact restart, locked/conflicting
state refusal, measured cancellation points, retained placement and missing
instance atomic refusal. The controller/file-worker fixture applies a real
joystick head pose, running/queued export Cancel, ordered Save and retry, then
checks separate import/pose history. The real Document authoring fixture verifies
pick/normal, paint Cancel/retry at independently interpolated UVs, immutable
source owners, immediate Save, held Cancel/retry and separate exact Undo/Redo.

The five final Release suites and five final own-core/support/renderer
ASan/UBSan/leak suites pass; the receipt records exact times and hashes. A native
FLTK chooser/universal-menu run passes seven checks with an active rigid part and
all twenty-two exported parts. This is Xvfb, not a new physical headset or
compositor session test.

Independent emitted-file rendering passes49 checks/twelve images across both
real geometries and ordinary/reflected-mesh/sheared-source cases. The material
fixture deliberately selects the independently implemented main/shade subset.
Pixel tolerance remains2: software maximum1; explicit PCI03 Arc A770 maximum0
in every image. All eighteen source/output graphic file hashes agree across the
two devices and the final guard refinement. Eighteen emitted files pass pinned
Khronos validation with0 errors/360 warnings and official VRM0 schemas with0
errors. Raw legacy source advisory errors remain separate unsuppressed evidence.

## Remaining full-goal work

Original source-rest axes/parents, reauthored rigs, source-less bones, divergent
skinned per-part placements and richer metadata/physics policies still need
authenticated native graph transport. They are not made valid by adding dummy
skins or by applying a mesh-node matrix to a skinned mesh. Static-only avatars,
VRM1 writing, full expressions/constraints/physics and broader rich-import/export
persistent allocation-denial proof remain required. Root must integrate this
component with the newer Scene19/Scene20 consumers; it introduces no schema claim
for them. No installation, display/session restart or physical test occurred.
