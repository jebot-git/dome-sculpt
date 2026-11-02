# Affine import on the current production baseline

Private merge `b943927d10e1faab8d602bec88a5ae3eb41a97bd` retains public
`6b64e22` and the qualified affine import history through `3fda297`. It does not
change the accepted XR selection or permission implementation. The product
changes relative to public main are confined to import-authoring and GLB
bind-space export: exact source-node authority is used for a graph containing
nonuniform/reflected locals, while ordinary similarity-only imports retain
their established native route. Sheared inverse bind-space is represented by
a verified TRS chain, preserving authored and implicit inverse binds.

The fresh canonical optimized FLTK/OpenXR build passes 17 selected suites in
95.85s. These include 821 independent source-chain/current-pose/GLB checks,
26 actual native/controller admission, evaluated-world pick/paint, Cancel,
one-Undo and ordered-Save checks, 13 independent reflected-frame witnesses,
GLB interoperability, actual CC0 VRM regressions and both xatlas regressions.
Two actual FLTK/DBusMenu affine import flows pass 11 checks each, covering static
and authored-skin sources, exact native restart and universal Undo/Redo.

Both finalized source-image batches pass on the actual Arc A770: 24 assertions
and 12 independent image comparisons each for baseline and reflected/sheared
imports, all maximum error zero. The unchanged image tolerance is two. The
explicit selector was `DRI_PRIME=pci-0000_03_00_0`; each log records Intel and
Mesa Intel Arc A770 (DG2). The retained old errors 54 and 18 are not relabeled.
Their independent-consumer world handedness correction and separate analytic
witnesses are described in AFFINE-IMPORT-REFLECTION.md.

Sanitizer scopes remain separate: the affine implementation at `d295bff`
passed ten fresh own-core/renderer/support ASan/UBSan/leak suites; the newly
included xatlas construction fix has its own two-suite dependency sanitizer
proof in validation/XATLAS-CONSTRUCTION.json. This integration receipt does
not claim a new whole-current-core sanitizer run. A first final relink stopped
because Make had loaded its old target list before CMake regenerated the newly
added dependency target; explicit configure and the complete final chain then
exited zero. Both logs are retained.

The production goal remains incomplete. Authenticated implicit/default-material
identity (Scene17), native UV1 import/paint admission (coordinated Scene18),
broader glTF extensions, full VRM/source-pose coverage, FBX deformation and
remaining affine metric-authoring routes still require their actual adapters
and consumer evidence. Their guards remain. This milestone performs no push,
installation, physical headset test or desktop/session restart.
