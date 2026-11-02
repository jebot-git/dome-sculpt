# Original-node REST authoring continuation

This private continuation starts at `bbb1792`. The original production goal
remains open. The existing native/grouped Scene25 milestone and its receipts
remain frozen; this component adds the actual original-node source adapter.
No public C++ layout or archive version is added here.

## Coordinate and ownership contract

The immutable `CreatorSourceModel` and exact source-instance identity authenticate
membership. Every primitive retains its original mesh node, original skin and
source-joint IDs. Names, equal skeleton values and a first selected skin never
reconstruct ownership. Native subgroups below the original mesh node are allowed
without losing that source identity. A complete captured target lease includes
all real skins and rigid parts; outside instances remain outside the operation.
Locked members refuse the complete transaction. Hidden members still follow it.
The existing Document held/selection authority is checked before worker admission.

`SourceRestGraph` normalizes MODEL-node local REST and current-pose witnesses once
on the detached worker. It retains every part's actual mesh, appearance,
deformation, world matrix and skin bind space strongly. Original joint slots
remain unchanged. Reparenting may append real original ancestor nodes as
nondeforming helpers; it neither fabricates skins nor shifts weighted slot IDs.
Unrepresented original nodes retain their original REST globals through derived
local compensation beneath a moved/reparented joint.

The initial admitted shared route requires exact current-node pose agreement.
A uniquely changed view and conflicting views are diagnosed separately and never
substituted for an untouched part. Every part uses its own original IBM and pose:
`Bnew = inverse(Rnew) * Rold * Bold` and
`Pnew = Pold * inverse(Rold) * Rnew`.
WORLD reconstruction and old weighted-slot skin products are checked against the
explicit default 20 micrometre bound. Exact absolute local matrices retain the
original editable TRS references. Original bytes, meshes, paint, material roles,
weights, morph channels and native artist transforms are retained.

## Native operation

`author_rig_rest` dispatches authenticated original-node rigs to the normalized
adapter. It captures complete first `rig_source` capsules on every real member,
including unskinned rigid parts, before replacing any skin state. Repeated edits
retain those same first capsules. Proposed data is validated in a detached Scene;
cancellation, allocation failures and validation errors have no partial publish.

Fitted source poses use actual local `T(parent delta) * M * R(local delta)`.
Only the edited original-node ID is remapped into each skin that contains it;
other local references and each skin's own bind table remain intact. Source-node
rigid attachment frames are resolved once per operation. Their exact world delta
is persisted on the actual rigid object, retaining its independent artist offset
and avoiding any render-time dependency on another live mesh.

Restore uses each member's complete first source. It retains live identity,
name, group and artist placement, while applying the authenticated before/after
attachment-frame delta to rigid parts. It does not silently rewire current scene
parents or invent dummy skins for rigid geometry.

The existing shared native/FLTK/XR fitting page27 and command IDs are reused.
Document captures source/generation/edit authority and the complete linked target
set; its generic ordered operation supplies one Undo and Save ordering.

## Validation state and remaining work

The final matching-header optimized overlays pass five suites: 1672 graph,
184 plan, 1806 actual Scene/native dispatch, 63 native Document/controller/history,
and 10 independent generic glTF consumer assertions. The same five pass with newly
instrumented changed/replacement C++ objects before immutable fully instrumented
own C/C++ core/support archives from `bbb1792`, asserted ASan/UBSan/leaks enabled.
All public headers are byte-identical to that frozen support; link maps exclude
both old overridden rig-edit/rig-rest-scene archive objects. This is an explicitly
qualified same-ABI overlay, not a fresh whole-branch CMake build or full regression.

The real 19-part CC0 avatar plus three rigid accessories cover current pose/morph,
first sources, repeated fitting, Restore and restart. A genuinely distinct-skin
GLB variant adds one real original auxiliary joint to one actual skin, preserving
original geometry, old weights and authored IBM arrays. It proves cross-skin
ancestor closure, own skin products and exact Scene25 restart.

The actual Document/controller path covers accepted worker fitting, immediate
ordered Save/reopen, one Undo/Redo, complete Restore, page27/fresh-trigger placement,
held edit authority and running capture Cancel. A Save depending on canceled
accepted work propagates that original cancellation and preserves its prior file;
a fresh explicit Save/retry then succeeds. It never silently saves a stale Scene.

Actual native FLTK passes 9 normal +9 compact640x360/font32 assertions; native
canonical d271 DomeD/Wayland passes 9. These execute a real relinked frontend with
explicit replacement modules and unchanged matching `bbb1792` frontend/support
objects. No physical headset or controller is claimed. Independent cgltf consumes
all 44,430 ordered source corners in each of three fitted/posed/restored GLBs,
including authored morphs, 19 actual skins and 3 real unskinned rigid instances.
Maximum WORLD error is below 0.31 micrometres. Pinned Khronos validates all 3 GLBs
with 0 errors and 19 warnings each (57 total). No new graphical image oracle, actual
Arc execution, allocation-denial sweep or VRM metadata/export proof is claimed.

A real neutral Restore defect added a residual approximately 1e-8 rigid affine edit.
The fix requires exact neutral local-pose authority at both endpoints and identical
rigid MODEL globals, validates the frame reconstruction bound, and retains exact
artist placement. A real intentional1micrometre nonneutral pose still moves its
rigid accessories; this is not a general small-motion deadzone.

Retained negatives include a snapshot pointer-identity assumption, an incorrectly
selected rigid fixture active object, three earlier fixtures that did not actually
produce distinct skin closures, a GUI fixture that inspected the unrelated initial empty object rather than all 22 source objects, the neutral Restore defect, and the initial mistaken dependent-Save success expectation. None of
these logs is replaced by a later pass.

The full required workflow still includes distinct per-part pose resolution,
source-authored unbound/generated-skin ownership, the complete source-node parent
picker, bone-role/rotation controls and independently rendered/GUI/device proofs.
Original-space VRM export still refuses changed REST/parents: first-person,
look-at, humanoid limit and spring/collider metadata need genuine bone-local
transport. Nonuniform/skewed sphere or capsule radii must not be guessed. These
remaining routes are required work, not completion by permanent guards.


The subsequent [rigid bind-frame continuation](SOURCE-RIGID-BIND-FRAMES.md)
implements true rigid meshes on mapped joints and canonical reparent following,
with a reflected native root and native child-subgroup coverage. It retains the
same Scene25 representation. The original checkpoint receipts above remain
unchanged; the continuation has its own exact qualification scope.
