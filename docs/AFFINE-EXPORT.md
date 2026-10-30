# Affine hierarchy GLB emission

Native affine placement prefixes now emit two independently verified TRS-factor
nodes before the existing native similarity node. Original source locals emit as
separate exact float matrices, preserving the original hierarchy instead of
flattening a sheared world or inverse transform into an invalid single node.
Native groups remain shared; skin joints inherit complete placement and skinned
surfaces remain identity scene roots. Rig sharing includes native affine edit
identity so distinct placed rigs cannot accidentally reuse one joint hierarchy.

The independent cgltf parser/accessor fixture passes 61 assertions and all 16
measured cancellation boundaries: actual Scene surfaces, reflected parent,
rotated child, native shear edits, exact original local matrices, world-space
vertices and an authored inverse-bind/current-pose skin consumer. The same
fixture against the unchanged ad9 exporter fails with the former legacy-Canvas
refusal. Existing GLB interoperability, source morph and VRM suites also pass.
The new exporter and fixture were rebuilt with ASan/UBSan/leak detection against
ad9's fully instrumented own core/support; the same 61/16 pass. Public headers
were byte-compared, replacement objects precede immutable archives and link maps
prove the old export.cpp object was not extracted. System LAPACK remains outside
instrumentation. Full consumer rebuild remains an integration gate.

This checkpoint does not enable general affine application import, rendering or
interaction yet. Sheared effective joint rest/bind-space matrices retain their
explicit export refusal until their original-chain representation is connected.
Actual affine VRM consumer pixels, retained original source identity across new
frontend workflows, source-local normal/TBN and reflection handling still need
the complete application migration. Existing VRM regression passing describes
legacy supported inputs only. Preserve published export cancellation callbacks
when merging this narrow exporter change into main.
