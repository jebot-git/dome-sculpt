# Owned source UV1 pipeline — private integration work

This branch extends accepted main8d46dfb with the null-shape-history fix35a3b4b.
It is incomplete production work, not admission of generic UV1 authoring import.
The native importer still refuses unresolved coordinate domains; ordinary
compositing still requires explicit coordinate preparation. Prepared normals,
shapekeys and topology transport do not acquire UV1 permission from this change.

PaintSourceMaps owns a bounded immutable TEXCOORD_1 domain paired with actual
UV1 views and validated against current vertices. Bare texture sampling still
rejects unresolved UV1. Explicit surface sampling chooses each role separately,
including the normal derivative basis. Optional versioned little-endian binary
coordinate storage participates in paint budgets. Scene18 is required whenever
current paint or recursively retained UV/material/polygon/shape history owns it;
UV0 scenes retain their historical formats. Scene17 belongs to the separately
coordinated implicit-default-material adapter.

The source UV baker now resolves each PBR role's selected coordinate domain,
checks agreement with retained MToon UV1, converts derivative normals into the
chosen destination basis, and retains original coordinates/images/paint in the
first source capsule. Explicit adoption produces ordinary editable atlas maps.
Raw worker snapshots remap coordinates through the actual packed vertex order;
geometry identity retains the coordinate owner. GPU shaders bind each PBR role's
coordinate index, including original normal fallback. Material-aware picking
uses the same UV1 alpha domain. Export emits an actual TEXCOORD_1 accessor with
retained-frame vertex remapping and independent per-role texture indices.

Current evidence includes analytic swapped-UV normal/sampling fixtures, codec
limits and downgrade rejection, actual atlas adoption and retained-source
restart, an actual native UV-editor Cancel/Apply/Save/reopen/one Undo/Redo flow,
and independent cgltf validation of the emitted accessor/material references.
Save retains the existing explicit refusal while accepted UV transfer is pending;
the workflow saves after its worker completes. The renderer's independent
transposed-image and rotated-normal witnesses pass all five PBR roles, alpha
mask/blend, picking, worker packing and invalid-domain recovery. The first Arc
run identifies the actual A770 and all five image differences are zero. A fresh
final-source Arc/sanitizer/integration receipt is still required.

Remaining work includes generic import admission, source-backed painting before
atlas preparation (especially differing UV1 under overlapping UV0), independent
exported-image/device comparisons for all normal/deformation variants, native
controller/FLTK/OpenXR workflow qualification, source-UV editing compatibility
with prepared normals and authored keys, and integration with Scene17/other agents.
The full REQUIREMENTS.md remains required. No system installation or physical
headset test was performed for these private changes.
