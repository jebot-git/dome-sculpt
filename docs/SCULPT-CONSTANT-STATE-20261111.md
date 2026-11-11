# Per-object sculpt attribute constants

The7415d581 renderer extends the per-object material-state repair by submitting
disabled vertex-array defaults once per object, rather than once per ordered
cell run. It retains original object/triangle/draw order, all VAOs and their
actual immutable geometry owners, mask-buffer bindings and scalar failure/
recovery behavior. It does not flatten CPU geometry or allocate extra GPU memory.

Only arrays disabled in every chunk VAO use the retained constants. Scalar6
can be enabled for masked chunks, so an unmasked run still explicitly restores
its -1 current value. This follows the
[Khronos draw contract](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glDrawElements.xhtml):
drawn array attributes may leave unspecified current values; unmodified
attributes retain their values. The implementation does not assume that masked
draws preserve a usable scalar default.

Six added actual-GL counter assertions retain all78 previous chunk checks,
giving84 in that consumer. The prior49465f29 renderer passes the independent
pixel and material-state oracles but fails the new constant bound:582 tangent
default submissions versus four actual object/range initialization sites.
The repaired renderer uses four in every opaque/Blend/Mask/reflection case;
material setups remain three. These are submission counts, not an FPS claim.

Both Release and strict ASan/UBSan/LSan pass1739 software graphics checks.
All94 output files perprofile are byte-identical to the original currentb8
images; all94 Release/SAN pairs are also byte-identical. Actual Arc checks pass
1739 perprofile,56 software/device pairs perprofile within the original bounds
and56 byte-identical Arc Release/SAN images. See
[the software/build receipt](validation/SCULPT-CONSTANT-STATE-20261111.json) and
[strict Arc evidence](STRICT-ARC-CONSTANT-20261111.md). The separate UHD negative
cleanup leak remains explicitly failed; positive Arc qualification is clean.

Each profile recompiles the complete changed renderer and regression-consumer
TUs and relinks seven consumers against those and the unchanged frozenb8
providers. Every source and owned input is checked. This is incremental
component qualification, not a fresh whole-main build or physical pacing proof.
The completed full original P1 run predates both renderer repairs. Optimized
full-workload measurements and further order-preserving draw batching remain
required; none of those performance targets is declared complete here.
