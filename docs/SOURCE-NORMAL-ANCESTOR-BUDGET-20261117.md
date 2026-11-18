# Prospective source-normal replay budget

The latest UV receipt does not bound older before-state receipts. Before deep
replay, the source adapter now visits actual normal/material/key anchors,
rejects cycles and applies the existing native64-depth/256-binding limits. It
collects every historical UV replay cap and checks the maximum against the
caller quota. Shared completed DAG nodes are reused without a false cycle.

The UV validators recurse before allocating their current-operation scratch:

- `SourceNormalUvReprojection::validate`: before-material validation and paint
  pairing precede target-mesh construction and normal reprojection.
- `SourceNormalSampling::validate`: before-material validation and paint pairing
  precede role-coordinate scratch, target mesh, resolved planes and detail
  transfer. Role-coordinate scratch is released before target construction.
- Each operation subtracts its retained target mesh from its own receipt before
  its remaining reproject/transfer work. Ancestor deep replay has finished by
  then. The normal adapter allocates its posed capture after these validators.

Consequently UV replay caps apply sequentially, and their maximum is compared
with the separately estimated capture requirement. Cache hits do not bypass
this conservative prospective check. Original input graphs retain their own
native storage limits; this change does not relabel them as new scratch.

Tests add three actual retained UV edits, an independently decoded positive
under a sufficient caller quota, a genuine larger historical receipt under a
smaller latest receipt, an explicit cycle, and atomic native worker/ordered-Save
refusal. The actual second-Bake interior texels must also independently reject
omitted, swapped-role and wrong-domain normals beyond the unchanged normal
error tolerance. Both normal roles and material/history assertions remain.

All six complete core/test TUs and three fresh links pass at this checkpoint;
runtime qualification is pending. Strict sanitizer and new-family graphical
and public desktop/XR tests remain required. No schema, frontend or unsupported
canonical/native32/writable-paint authority is introduced.
