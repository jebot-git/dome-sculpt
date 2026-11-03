# Native key / retained UV commutation — private test checkpoint

Test source `347800e0258e2c719be07ceda616595295aaabf5` is based on private combined source `c79a140`.
The runtime correction is the separately owned `38cced9` component; it is **not
merged into this test branch**. This checkpoint does not change product APIs or
archive formats and is not a main/publication claim.

The two native routes are intrinsic Create + nontrivial Replace followed by
Keep paint at .7 scale / 90-degree rotation, and that same retained UV operation
followed by Create + Replace. The first route supplies the expected role arrays
without running the retained-key adapter. Ordered source identities, base N/T/W,
all target direction deltas and declared role policies agree exactly. Both routes
retain their writable color/scalar/coverage bytes and independent normal detail.

Nine simultaneous-morph / mixed anisotropic-pose rows have native image maximum
error 0; eighteen independently parsed, cgltf-validated and GLSL-rendered current
GLBs also have maximum error 0. Bounds remain 2 for native order comparisons and
3 for external consumer comparisons. Role-component maximum error is 0 with a
2e-6 bound. Actual same-world projector strokes then preserve the common paint
bytes and each route's current normal-event identity.

A separate real native shared-edge UV split expands a four-vertex prepared quad
to six current vertices. Consistent target deltas succeed. Contradictory deltas
for two current vertices sharing one authenticated original vertex refuse with
`This key separates retained UV seam vertices`, preserving exact Scene/history.
A linker wrapper pauses only after the genuine detached key factory completes;
Cancel then preserves the prior native Scene, accepted Save file and Undo/Redo.
The same input retries successfully. This gate is publication-cancellation
coverage, not an internal-kernel cancellation or allocation-fault claim.

## Preserved negative and fixed result

* Original c79 archives: handle 40706, terminal exit 1 in 24.217 seconds. The
  intrinsic-key-first route passes, while UV-first Create fails with
  `Prepared corner topology differs from its exact current authority`.
  The detached rewind paired before-paint/materials with the current polygon.
* Frozen 38cced9 replacement: handle 26885, terminal exit 0, 101 checks in
  88.510 seconds. All 1,447 recorded source/support/object/binary fingerprints
  are identical before and after. Original negative sources and evidence remain.

## Exact proof scope

Only the new test TU was compiled, with the canonical native c79 flags. It links
matching immutable c79 core/render/affine/polygon/xatlas/Mikk/ufbx support; the
fixed core changes exactly one `shapekey-normal-preparation.cpp.o` member.
That member and its source hash were verified against 38cced9, and all shared
public/private headers match c79. Other support archives are byte-identical.
This is a matched archive-consumer overlay, **not a fresh whole-branch build**.

Renderer: `llvmpipe (LLVM 22.1.8, 256 bits)`. No GPU lease was used. The frozen
test has an optional `--hardware` mode requiring Intel/Arc/A770 renderer tokens;
it has not been executed. No new sanitizer, physical-device, Khronos or VRM
schema qualification is claimed here. Root owns coherent integration and wider
qualification.

The machine receipt in `validation/KEY-UV-COMMUTATION.json` records source,
object, archive, command, binary and evidence hashes, all nine image rows, and
the complete 1,447-entry final before/after manifest. Local proof artifacts are
under `artifacts/commutation-cpu` (negative) and `artifacts/commutation-fixed`
(positive) in this worktree. No new run was needed for this documentation.
