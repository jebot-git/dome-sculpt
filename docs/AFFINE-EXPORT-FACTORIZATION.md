# Bounded native affine-edit export factorization

`src/affine-factor.hpp/.cpp` prepares a native local edit as two glTF-compatible
local matrices: translation*proper-rotation*signed-scale, then proper-rotation.
It does not decompose or rewrite unedited original source node chains. In
particular, inverse(T*R*S)=S^-1*R^-1*T^-1 is generally not one legal TRS node.
The existing source/export guards remain until the full placement/consumer path
uses these factors and passes independent rendered/accessor comparisons.

The native C++ adapter calls the installed LAPACKE_dgesvj_work middle-level C API
with a fixed column-major 3x3 input and six-double stack workspace. This wrapper
branch calls LAPACK directly without row-major transpose buffers. DGESVJ's
published contract limits Jacobi work to 30 sweeps. A nonzero status refuses;
there is no partial-result or approximate-single-matrix fallback. Cancellation
is observed before and after the fixed-size call. Keep this on export/preparation
workers, never per frame or per vertex.

Both orthogonal bases are independently checked, improper rotations are folded
into a signed scale, and each reconstructed column is compared relatively with
its own original magnitude (no unit-scale floor hiding tiny columns). The float
factors must each pass local-TRS checks, preserve reflection orientation, and
reconstruct within 2e-6 per column. Double reconstruction is bounded by 2e-11,
with orthogonality 2e-12. Singular/underflowed native edits, uncertain orientation,
unrepresentable factors and unverifiable results fail explicitly without
modifying the original edit. These are representation checks, not a claim that
all downstream material/frame interpolation or posed pixels have been qualified.

Fresh standalone Release and instrumented affine-library/test builds pass 4721
assertions each: analytic shear/reflection, inverse of a valid local TRS,
independent coefficient reconstruction, tiny/large diagonal components, 256 varied
compositions, seven injected driver/result failures, both cancellation sites,
and 128 warm valid calls under permanent C++ allocation denial (zero attempts).
The system LAPACK/BLAS DSOs are not sanitizer-instrumented. The native new module
and test are instrumented; this is not whole-core, GPU or GUI qualification.

The recorded host uses LP64 LAPACKE 3.12.0 and reports LAPACK 3.12.0 at runtime.
The latest 1000-call Release mean was 2.65 microseconds (SAN 10.16); this is an
observed tiny warmed benchmark, not a latency/pacing guarantee. Tests set BLAS/
OpenMP thread counts to one. Production changes no global BLAS/backend/session
configuration. No source/binary is vendored and no dependency was installed.

Primary provenance is pinned in third_party/sources.json:
[DGESVJ 3.12.0](https://github.com/Reference-LAPACK/lapack/blob/04b044e020a3560ccfa9988c8a80a1fb7083fc2e/SRC/dgesvj.f)
and its [C work interface](https://github.com/Reference-LAPACK/lapack/blob/04b044e020a3560ccfa9988c8a80a1fb7083fc2e/LAPACKE/src/lapacke_dgesvj_work.c).
The existing system dependency is BSD-3-Clause. No third-party algorithm source
was copied into the adapter.
