# Exact color-bound channel reuse — 2026-11-26

The isolated per-cell optimization passes 576 differential controls in both
Release and strict ASan/UBSan builds. Tested source is `1b954003`; clean standalone
commit `eccddcd4` contains byte-identical algorithm and differential fixture files,
without the separate grouped-workflow fixture. This is algorithm qualification;
owning optimized core/application integration remains a separate build/runtime.

The original algorithm computes the same edge polynomial repeatedly when RGBA
channels have equal values. The change checks the actual three vertex values and
four neighboring texel values in each current cell. Only equivalent operands
share their local cell maximum. It preserves every tested cell, clipping result,
edge-polynomial decision, callback position, work budget, independent channel
maximum and outward-rounded receipt. No equivalence state survives a cancellation
callback. Different neighboring fields retain their general calculation. All
existing input, ownership, finite-value, quota and fidelity guards remain.

The frozen reference is the exact `725ea100` function with only its namespace
renamed, verified before compilation. Both profiles use complete new fixture
translation units and links; support archives from 725 supply unchanged Mesh/Image
validation. These fixtures directly execute the changed inline algorithm. They
do not claim that owning core consumers have already been rebuilt with it.

On the authentic 2048 native endpoint, the fixture extracts the actual mesh,
explicit Identity color field, 16-MiB projection and original receipt. Both
algorithms visit exactly 5,934,012 tested and 2,975,612 intersecting cells, with 5,808
identical callbacks. All four double maxima are bit-identical, and the complete
original native receipt CBOR remains byte-identical. Reversing evaluation order
preserves the result. A single green-channel texel change at a real triangle
interior takes the distinct general path and correctly invalidates the original
tight receipt while retaining the other channel maxima.

Additional cases cover 48 deterministic random/equivalent/paired/nonuniform fields,
in-cell legal callback mutation, cancellation, exact and one-cell-tight budgets,
ill-conditioned and degenerate charts, out-of-atlas UV and nonfinite inputs.
Rejected input comparisons use exact float bits so NaN rejection is tested fairly.
The original rejection messages and callback schedules remain exact.

Release invocation `374a3f4c02104b33a5eb829c506e3945` passes in 8.780s with peak
owned RSS 376,049,664 bytes. Strict invocation
`2f870a49d9424edea18c6149a5fe1062` passes in 158.434s with peak 1,128,923,136 bytes.
Both use 900-second limits, a separate 1.5-GiB math-case RSS bound and no virtual-AS
cap. Strict actual executable/maps attest live ASan/UBSan and leak detection with
no suppressions or diagnostics. Drift and owned-survivor lists are empty. These
confirmed CPU-only cases use no graphics lease or DRI/Mesa driver. The live older
product 725 grouped strict workflow remains untouched and has its own evidence.

Within the same actual input and process, Release paired orders measure old/new
1.612/.941s and 1.701/.952s; strict measures 34.902/15.206s and 32.784/15.044s. The
changed-neighbor strict pair is 31.567/13.775s. These elapsed observations identify
an improvement in this numerical kernel; they are not isolated hardware benchmarks
or measured whole-application speedups. The actual full owning 2048 workflow and
export evidence must still be repeated on the integrated product.

The paired compiler audit checks 758 actual dependency entries, original archive
payload digests and both ELF links. The successful short Release transient unit
is already collected before property capture; its actual not-found observation,
status, live executable/maps, journal and invocation remain. The strict unit is
held exited without processes until terminal capture, then stopped. No missing
terminal properties or physical/public display qualification are implied.

Receipt and compressed proof: `docs/validation/hdr-channel-differential-20261126.json`
and matching `.tar.xz`. The original 122,856,849-byte native input is identified by
SHA256 `4721e99dc724cfdb71cc26106c646a532bebf53d12a060ec8bc65184fd54a1e4`.
