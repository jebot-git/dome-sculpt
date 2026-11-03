# Original-node reparent and rigid subgroup VRM0 export

This private continuation of `c031ba2` exercises genuine source-node reparenting
and a rigid mesh on the same node as a real weighted humanoid head joint. The
earlier native `2192365` and typed export `c031ba2` proof trees remain frozen.
There is no new public layout, Scene version or archive payload.

One actual source node is both the weighted head joint and an unskinned mesh
instance. A genuine authored neck-child spacer, included in a real source skin
table, becomes its new parent. The native authoring operation preserves each
skin's own current pose/inverse-bind products and the complete first source.
Rigid geometry follows the same original/current bind equation, without a
dummy skin. Source IDs and actual node/skin tables establish these identities;
neither names nor a first skin's pose are substituted.

The independent source fixture covers ordinary placement and an additional
reflected/sheared native root with a rotated/scaled native subgroup below the
rigid source endpoint. It reads six fitted/posed/restarted VRMs with cgltf,
checks all19 skins and4 rigid parts, every44,436 ordered corner and exact
current per-part IBM float bits. Maximum world error is recorded in the receipt
at the unchanged20 micrometre bound. The true-head camera point, collider and
center references and every recursively simulated spring child/order are checked
independently against current native/source authority.

The original exporter passed ordinary reparenting but refused the native rigid
subgroup as an unrelated placement. That exact failing binary, sources, map and
log are retained. The narrow implementation fix validates the complete original
source chain and permits native groups only after the authenticated rigid mesh
endpoint. Their complete world delta already belongs to the emitted rigid mesh
attachment; no authoring transform is flattened away. Native groups on a
skinned path, before the rigid endpoint, followed by another source placement,
or associated with a foreign model remain refused atomically with old output
and source state intact.

The actual native fitting worker applies reparent/pivot edits under the affine
root and rigid subgroup, queues immediate Save, and preserves exactly one Undo.
Actual controller Cancel stops running and queued exports at the durable
temporary-file barrier; old outputs, full source/history and following Save are
unchanged, and retry succeeds. Independent native/core-consumer images include
pose/morph, visible rigid sensitivity and a wrong-original-IBM control.

Final Release and asserted ASan/UBSan/leak scopes each pass81 source,19 worker
and18 software image checks. The four final images match exactly on software
and strict PCI03 Intel Arc A770 at unchanged tolerance2. Ten emitted VRMs pass
pinned Khronos with0errors/200warnings and official VRM0 schemas with0errors.
`validation/VRM-SOURCE-REPARENT.json` records exact source/support/binary and
evidence hashes. Qualification rebuilds the changed private graph C++ unit and
new fixtures before immutable matching own-core/support/renderer archives;
it is not a fresh whole-branch build or complete regression run. All public and
other private type headers remain byte-identical to the matched `c031ba2` base.
The three new CMake registrations configure separately.

These cases use actual female CC0 VRoid geometry/textures through the existing
deterministic material profile. Male fitting/pose exports remain qualified in
the prior component, not newly qualified for reparent by this fixture. No new
FLTK/Wayland/physical headset, allocation-sweep or performance claim is made.

General source rest/role/axis authoring,
distinct per-part pose reconciliation, wider collider/sphere/capsule and spring
reauthoring, eye/look-at/dynamic physics, generated/unbound rigs and VRM1 remain
required. Matrix and image equality do not establish physics equivalence.
The prior translation-only collider authority and explicit unsupported-case
diagnostics stay in force. Source24/25/26 semantic union remains separate.

The subsequent private bone-length child supplies explicit node-local length
interchange and actual fresh native reimport/Save/Restore. See
BONE-LENGTH-AUTHORING.md for its separately qualified scope and source graph/image
resource fixes; this checkpoint remains frozen.
