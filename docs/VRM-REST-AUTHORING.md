# Source-node REST authoring and VRM0 export

This private component extends the qualified native Scene25/source-node REST
work in `SOURCE-REST-AUTHORING.md` and `SOURCE-RIGID-BIND-FRAMES.md`. It exports
the tested source-node fitting subset through the real native file worker.
It does not complete rig/VRM authoring or the production goal.

The exporter resolves one authenticated model/instance, current source parent
graph and shared original-node pose. Each real skin keeps its own current
inverse-bind matrices and joint-slot table; unskinned source accessories remain
unskinned. It uses the same detached normalization as native fitting. No model
is grouped by names, skeleton value equality or a first skin's pose. Distinct
current poses remain explicitly refused until their authoring policy exists.

Current source parents and local matrices replace immutable raw REST authority
only in the emitted graph. The original model, raw bytes, material/image roles,
source vertex identities, morphs and first rig-source capsules are retained.
No public header, Scene version or archive payload changes in this component.
The private graph/exporter translation units must be rebuilt together.

## Metadata coordinate and topology authority

Let `R` be the current model-space REST global and `R0` its original value.
The bind correction is `C = inverse(R) * R0`. Original source-node IDs remain
the authority for humanoid roles, first-person erasure, collider groups, centers
and spring roots; indices are remapped to the actual emitted nodes.

The pinned [UniVRM exporter](https://github.com/vrm-c/UniVRM/blob/efd20f62f1bf0ac3fcc34de1e497324c5197d146/Packages/VRM/Runtime/IO/VRMExporter.cs)
changes the glTF Z convention while leaving first-person offsets in Unity
coordinates. Offset transport therefore uses `H*C*H`, where
`H = diag(1,1,-1)`. Missing offset components are zero. The actual first-person
bone stays unchanged: [VRMFirstPerson](https://github.com/vrm-c/UniVRM/blob/efd20f62f1bf0ac3fcc34de1e497324c5197d146/Packages/VRM/Runtime/FirstPerson/VRMFirstPerson.cs)
also uses that bone as a head-weight erasure root. A helper cannot replace it.

Collider attachments admit exact translation-only bind corrections. Their
helper has identity rotation/scale and retains raw offsets and radii. General
linear, rotated, reflected or sheared collider correction remains refused:
[SphereCollider](https://github.com/vrm-c/UniVRM/blob/efd20f62f1bf0ac3fcc34de1e497324c5197d146/Packages/VRM/Runtime/SpringBone/Logic/SphereCollider.cs)
uses Unity `lossyScale`; matrix equivalence alone does not prove that runtime
radius/sign convention. Center attachments retain their complete affine
correction through verified glTF TRS factors outside simulated subtrees.

[SpringBoneSystem](https://github.com/vrm-c/UniVRM/blob/efd20f62f1bf0ac3fcc34de1e497324c5197d146/Packages/VRM/Runtime/SpringBone/Logic/SpringBoneSystem.cs)
recursively visits every child and uses the first child to construct a tail.
The emitter preserves authored child order, including nonnumeric order. It
refuses helpers, added rendered-mesh children, changed REST locals and changed
membership/parents inside those auto-recursed chains. These are required
simulation-adapter cases, not a dynamic-physics completion claim. Custom
humanoid limits also refuse changed local linear axes.

Native bone length is independent editor data without a standard VRM0 field.
An edited native-only length now produces an actionable refusal, preserving
the native project and old output, rather than silently omitting that edit.
Its eventual export/import receipt remains required.

## Qualification and limits

The final metadata fixture independently checks point/basis arithmetic, H
reflection sensitivity, partial/default offsets, exact translation-only sphere
authority, affine centers, actual emitted spring order/helper identities,
malformed IDs, cycles, unsafe spring changes and cancellation.

Actual female/male CC0 VRoid geometry and textures are exercised through the
existing deterministic MToon material-profile fixtures, with nineteen skinned
parts and three genuine rigid accessories per model. Fitted, posed and native
restart exports are read by cgltf; every ordered corner and each current IBM
float bit is checked. Independent native/core-consumer images use unchanged
tolerance2, visible rigid sensitivity and a deliberately wrong original-IBM
control. They do not qualify every raw material/lighting combination.

The real Document/controller fixture accepts fitting with one Undo, immediately
queues Save, cancels running and queued VRM exports at a durable temporary-file
barrier, preserves prior files/source/history, and retries successfully.

`validation/VRM-REST-AUTHORING.json` records exact final source/binary hashes,
Release/SAN/device scopes and validation outputs. Qualification uses rebuilt
changed C++ translation units before immutable matching fully instrumented
own-core/support/renderer archives. It is not a fresh whole-branch CMake build
or complete regression run. CMake registrations are configured separately.
There is no new FLTK/Wayland/physical headset, allocation-sweep or performance
claim. Previous component UI proofs remain separately scoped.

Open work includes wider original-node reparent/role/axis fitting, authored
native length receipts, distinct per-part pose reconciliation, general collider
and spring reauthoring, eye/look-at behavior, actual dynamic physics, generated
or unbound source rigs, nonlinear creator refitting and VRM1/constraints.
Source21/23/24/26 compatibility requires a real semantic union; a larger outer
Scene number must never imply support for an unknown before-state feature.

The later [genuine-source reparent continuation](VRM-SOURCE-REPARENT.md)
qualifies an actual new source parent, weighted-joint rigid mesh, reflected/
sheared root and native rigid subgroup without changing this checkpoint's
source, output or evidence. Its limits and independent proof are separate.
