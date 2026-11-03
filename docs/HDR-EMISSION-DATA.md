# Private HDR paint data checkpoint

This is the first data stage on `13f7e57`, not an HDR import, renderer, native
Scene27 or frontend admission milestone. Scene27 is reserved for the completed
owning implementation; its scene/material/bake closure is not yet implemented.
Existing import, composite, UV transfer and Scene guards remain explicit.

Sparse physical emission now owns seven linear floats per pixel: historical
material-relative RGB, absolute physical RGB in cd/m², and actual float coverage.
The existing14-byte paint pixel, coverage/mask bytes and ordinary unit writes
are preserved. Physical coverage cannot first be rounded to UNORM8: a nearly
covered10000-unit source can retain visible light after byte coverage rounds to1.
Physical strokes therefore retain their own coverage and a validated
matching legacy fallback. Material-relative strokes keep their old path unless
an actual physical tile already exists. Physical data remains immutable/COW
through snapshot, stroke Undo/Redo, Cancel and evaluated-space stamping.

Only genuine physical tiles select Paint3, with a bounded non-nested payload and
nullable actual paint-domain reference. Hidden, erased and authored-zero leaves
are retained. The new six-argument decode capability permits its structural
decode; every existing overload refuses3 before geometry, even when empty.
Physical/legacy byte inconsistencies, empty authority and nested payloads refuse.
Standalone physical CBOR restarts exactly. Scene ownership is still refused
until complete before-state/minimum-version/budget and consumer adapters exist.

Fresh own core/support builds pass optimized Release and assertion-enabled
ASan/UBSan/leak profiles, with1482 source inputs unchanged. Selected3/3 pass in
both:74 independent emission algebra checks,50 real paint-data checks, and the
existing native texture-painting suite. Data tests exercise22 measured cancel
polls (including the final check), all10 measured persistent host-allocation
prefixes and same-source retries. All1492 source/archive/binary inputs remain
exact per run. This is headless core/support qualification, not a fresh app,
renderer, vendor-backend or graphical qualification.

A separate old/new-header-and-core process bridge authors identical real legacy
color/emission/mask strokes: both write the same57706-byte CBOR,
SHA256 `264111d5bae32b82ae55f1a97c8f8cddee75ffb541442a28b97d97b0d31345d7`.
Both old and new legacy-capability readers refuse actual physical Paint3. No
headers were mixed with another class layout. See the validation receipt.

Next required work is actual Scene27/material11/bake closure, GPU float sampling
and physical/legacy blending, source HDR admission, real FLTK/OpenXR controls,
and native worker/history/orderedSave plus independent image/export proofs.
Source28 independent original sampling remains root-owned and must compose with
physical capability explicitly. Ordinary PNG8/16 normalization cannot represent
all physical fields faithfully: the preserved analytic10000-strength/.005 case
loses15 preview levels even with PNG16. Exact native data and representable
standard exports are required; permanent blanket guards are not completion.
Current primary KTX HDR container support does not by itself extend ratified
glTF `KHR_texture_basisu` to HDR; no draft extension is silently advertised.

No GPU, user desktop, physical HMD, system installation or session action was
used for this data checkpoint. The separate qualified core-unlit repair is
`b35b517`; it has not been mixed into these frozen data binaries.
