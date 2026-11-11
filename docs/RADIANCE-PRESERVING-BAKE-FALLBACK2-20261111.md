# Resolved FLOAT fallback metrics: source contract, qualification pending

The ef0 Release controls passed field579, EXR648 and native69 checks. The real
worker workflow then passed16403 checks, including projection-source values,
adoption/hidden owners, ordered Save, Undo/Redo, fresh Document reopen and exact
EXR, before GLB export failed `Invalid native HDR fallback metric`. Preserve
`artifacts/radiance-bake-export-proof-20261111/native-release-v1` unchanged.

The bounded production diagnostic loaded that actual accepted native file and
ran the real glTF builder/finisher. It observed Scene41, EXR interpretation2,
peaks `[52016, .0006399999838322401, 2]`, scale52016, absolute error228.74609375,
relative error.02376273274421692 and zero erased samples. The first legacy
30000 scale guard caused the refusal. Its source/ELF/commands/output remain in
`fallback-diagnostic-v1`. This is diagnosis, not a workflow pass.

This child source introduces an explicit fallback record2 for actual EXR2.
It retains the old package1–4 envelope and exact fallback1 shape, bounds and
F32 calculation. Old consumers reject the new inner record rather than
misinterpreting it. New readers preflight the actual EXR header/version and
require a genuinely decoded Scene41 and that record's actual current FLOAT
paint owner. Opaque JSON, a legacy EXR1, another hidden field, stripped version
metadata and a mere caller assertion cannot supply this authority.

Fallback2 declares a PBR or authored-MToon current view and explicit units:
its native normalization scale and absolute error use linear cd/m²; relative
error is dimensionless. The emitted glTF emissive multiplier retains the same
numeric scale. Khronos defines its `emissiveStrength` as a nonnegative numeric
multiplier without the unrelated30000 limit; native preservation remains
independent from that ordinary measured view. See the
[official schema](https://raw.githubusercontent.com/KhronosGroup/glTF/main/extensions/2.0/Khronos/KHR_materials_emissive_strength/schema/material.KHR_materials_emissive_strength.schema.json).

For each source FLOAT value A, decode the actual PNG RGB8 value into linear
binary64, multiply the actual FLOAT component factor and FLOAT scale in
binary64 to obtain R. An unlit ordinary fallback does not emit that term, so
R=0. Absolute error is `abs(double(A)-R)`. For A>0, relative error is E/A.
For A=0 and R=0, relative error is zero. Nonzero R at A=0 has no finite relative
metric and refuses; it never becomes a fake zero, NaN, Infinity or clamp.
Positive A reconstructed as zero increments the explicit erased count.

A and R are finite/nonnegative and <=F32max. E is therefore <=F32max. A positive
FLOAT source is >=2^-149, so the dimensionless ratio is <=F32max/2^-149,
exactly `(2-2^-23)*2^276`, less than2^277 and safely finite in binary64. It is
not bounded by FLOAT radiance or by the old1e6 compatibility limit. New private
metric storage is double; all TUs seeing that changed private definition must
be rebuilt. All103 public headers remain intended byte-exact8b; this does not
permit mixing old private-layout consumers.

The planned reader checks actual glTF factors/scale, captured RGBA8 PNG and EXR
extents, typed view and all old resource quotas before allocation. PNG replay
uses only the writer's bounded noninterlaced RGBA8 representation, excluding
compressed profiles/text. It recomputes quality from actual PNG/FLOAT data and
rejects rehashed false metrics. Complete native paint replay checks the FLOAT
contribution planes against their genuine current owner, independently from
the RGB view and checksums. Nothing routes native radiance through RGB8.

The new independent metric fixture covers exact zero, undefined zero division,
F32 maximum radiance, a positive minimum-subnormal denominator and the exact
IEEE-power ratio oracle. The real worker fixture gains independent PNG
reconstruction in extended precision, no-erasure checks, rehashed wrong units,
wrong quality, stripped/fake version2, foreign contributions and a real Scene41
whose named current field is absent. Actual original failures remain evidence.

These changes are source-only and unqualified while root runs quiet original
P1. No new compile/native/hash/archive/GPU jobs may start until that window
closes. Afterward the codec's complete TUs and every actual dependent private
consumer need fresh owning compilation/linking, original deadlines, before/
after source/provider pins, native controls and independently coordinated
software/hardware/application qualification.
