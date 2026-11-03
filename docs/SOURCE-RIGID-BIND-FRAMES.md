# Rigid source bind frames

This native continuation of `2f06246` closes its explicitly guarded rigid-mesh
joint and source-parent cases. It does not add a dummy skin, public C++ field or
archive version. VRM REST metadata export remains separate work.

A rigid mesh may also be an actual source joint, and a native source rig can be
reparented while its original immutable Scene placement groups remain unchanged.
A pose-only product of the old group path is therefore not a valid attachment
frame after REST authoring.

The attachment uses the same bind equation as each real skin:

`F = Q * P_current(node) * inverse(R_current(node)) * R_original(node)`.

`P` and `R_current` are the authenticated canonical MODEL pose and REST globals
from the native rig. `R_original` is the immutable original node global.
`Q = source_node_native_world * inverse(R_original)` retains the actual native
placement prefix through the original source-node group. Extra native subgroups
below that source node remain relative artist offsets in the actual object world.
The normalized current parent graph determines `P` and `R`, rather than the old
immutable SourceNodePlacement ancestry. Per-node neutral pose authority returns
the exact native source endpoint, avoiding a spurious numerical artist edit.

Under pure fitting `P_new = P_old * inverse(R_old) * R_new`, so `F` is invariant.
After an actual pose edit the worker persists `F_after * inverse(F_before)` on the
real rigid object's world transform. It retains real rigid morphs, paint,
metadata, source identity and independent artist offsets. There is no live
per-frame geometry dependency on another skin.

The new independent fixture adds a genuine rigid mesh to the actual weighted head
node, plus a real authored neck-child spacer used as a new head parent. It keeps
valid humanoid neck ancestry, all old weights, authored inverse binds and genuine
source node IDs. Tests cover default and reflected/sheared native roots, rotated
and scaled native child subgroups, pivot fitting, actual canonical reparenting,
posed rigid attachment and exact Scene25 restart. The expected full world affine
and all rigid corners come from an independently evaluated virtual single-joint
skin with the original bind. The earlier `2f06246` runtime refuses this same input
with its original attachment WORLD-bound diagnostic; that negative is preserved.

Final same-ABI optimized qualification passes five suites in48.96s, and the
matching asserted ASan/UBSan/leak support-overlay five suites in302.31s. New bind
coverage is34 assertions (all16 affine components plus real corners); unchanged
regressions retain1672graph,1806Scene,63actual Document/controller/history and10
independent cgltf checks. The separate immutable export normalization passes110
Release +110instrumented checks with explicit caller snapshot byte accounting.

Qualification receipts distinguish the new native bind checks from unchanged
Source Scene/Document/controller/GLB regression proofs. No new image comparison,
Arc/physical headset, GUI route change, allocation sweep or full CMake rebuild is
claimed. Current transient VRM graph and metadata work is excluded from this
native milestone. Distinct-pose reconciliation, source-authored generated/unbound
rigs and complete typed VRM REST/physics metadata remain required.
