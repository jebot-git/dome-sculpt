# Native bound reuse and emission lattice draft

This isolated child of the qualified geometry/GUI components is not admitted.
The private emission-entry C++ layout changes; all affected core, render, native,
XR and test consumers must be rebuilt coherently before any new-code run.

The numerical color-bound memo is fixed-capacity and scoped to one top-level
validation/encode/export. It retains exact strong COW mesh, field, topology,
projection/image and source-context owners, not a general validity proof. It
also invalidates changed image/color content and claimed scalars. Current
correspondence, input, resource and cancellation checks remain outside the
numerical result. Borrowed geometry and unsupported contexts compute normally;
full capacity falls back. Return and exception release all owners.

Old stage instrumentation proves repeated computation: the same retained64
export invokes79 bounds for3 exact contexts. A narrower timer measured193ms
inside those arithmetic calls out of244ms total (concurrent-load observation,
not a production latency guarantee). Old full export/oracle and allocation
sweeps remain preserved separately.

The physical-before-UV native path already preserves exact float owners,
normal metadata, history and native pixels. Its original finite256 output
fails the unchanged2-pixel bound by17, both before and after UV. Emission-off
reduces this to1; unlit/no-emission to0; emitted before/after pixels are exactly
identical. The independent resampling counterexample is retained.

The draft therefore aligns integer upsampling of a constant-background native
bilinear paint grid to its real texel-centre breakpoints. An emission-only
KHR_texture_transform carries the genuine inverse offset. Equal grids and
Source4 fields keep their old path. Independent source-image/filter products,
downsampling and unsupported domains do not gain a global fidelity claim.
The descriptor remains finite/approximate, with exact native data separate.

Required gates include owner/content/scalar/budget/cancel/mutation/exception
controls, persistent new-path allocation denials, exact old output/image parity,
actual physical UV pose/consumer cases, wholly owned sanitizer/native builds,
then bounded hardware and coherent integration. Typed native MToon and general
native/retained sampling domains remain separate unfinished implementation.
