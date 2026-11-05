# VRM1 bounded wider shade expression targets

Private runtime `86ea09ee91d1b74e67cdcacdfa862b33370cfd0c`, following `196ea93`,
preserves representable shared-shade expression targets above one. It retains
one genuine paint image, original lower materials/role samplers and bindings,
separate native colour/coverage authority, and exact native package restoration.
It introduces no public header, layout, persistent schema, command, page or UI.
The changed private compiler product requires all three direct production TUs:
`vrm1-expression-layer.cpp`, `vrm1-authoring.cpp` and `vrm1-export.cpp`.

The pinned [VRM1 material-colour binding schema](https://github.com/vrm-c/vrm-specification/blob/94e82dd346fa6cf0337c4421728640e5252dd38e/specification/VRMC_vrm-1.0/schema/VRMC_vrm.expressions.expression.materialColorBind.schema.json)
permits an unrestricted numeric float4 target. The serialized initial MToon
shade factor remains unit bounded. A primary-source audit of
[UniVRM's material accumulator](https://github.com/vrm-c/UniVRM/blob/efd20f62f1bf0ac3fcc34de1e497324c5197d146/Packages/VRM10/Runtime/Components/Expression/MaterialValueBindingMerger.cs)
and its MToon shader confirms weighted target deltas without a unit clamp on
those runtime shade values. This is a pinned source audit, not a Unity/viewer
execution claim. Exact URLs/hashes are in the receipt.

The compiler now bounds accumulated shade coefficients over the complete
preset/custom unit-weight box, including simultaneous contributions. Original
native RGB division and zero-factor fallback remain exact. The initial factor
still must fit its standard unit representation. Nonnegative finite source
limits remain enforced; these staged native limits are not advertised as the
full normative range of expression targets.

Wider coefficients amplify reconstruction error. A bounded local prefilter
compensates tent smoothing for even integer upsampling, and genuine adjacent
straight RGB fills zero-native-coverage texels. These are candidate texture
reconstructions: the continuous Bernstein check still measures against the
unchanged ORIGINAL premultiplied field. Its allowance becomes stricter with
coefficient amplification. No source field, comparison camera, material/sampler,
requested image dimensions or fixed image bound is changed. A complete sampling
forecast, including compensation, padding and exact merged centre-knot count,
refuses before dense allocations. There is no extra dense scratch image.

## Completed evidence

Counter `ad0d947` preserves the actual old public-worker refusal for a valid
source shade target .99 with native red .6, requiring an upper target about
1.65. Its previous file, complete native Scene and Save remain exact. The first
wider texture prototype still refused its stricter reconstruction check. An
intermediate full factory run also retains a metadata-only polling observer
failure; prior callback scheduling was restored before the final proofs.
Neither negative is relabeled as a positive.

Both final profiles rebuilt all three complete production TUs and whole focused
consumers over copied matching immutable `196` owned support. Release passed in
165.909 seconds and ASAN/UBSAN/LSan in 817.662 seconds, with all 1,880 captured
inputs unchanged. This is a matched component scope, not a fresh whole-app/core
build or a full registered-suite result.

Two genuine actual Document workflows each pass 49 checks per profile. They
cover a target above one and accumulated preset/custom shade deltas, real
nonuniform paint, both original skins and rigid part, 256/512/1024 policies,
queued Save, one paint Undo/Redo, package Open/second cycle, running and queued
Cancel, recovery, quotas and atomic publication. Graphics focuses one part;
the package retains all three declared source parts and dependencies.

Independent cgltf/PNG/spec-shader rendering passes 515 checks / 108 pairs per
profile: original camera and shifted eye, poses 0/2, individual and mixed
weights, all three resolutions. Every maximum is 1 at unchanged bound 4 with
coverage above 300. Wrong moving paint, frozen original sampling, tint and
missing shade stay sensitive. Second-eye/no-op uploads and exact native Scene
oracles remain. All 216 PNGs and 20 native/container/control files are exactly
equal between profiles; the latter includes inputs and previous-file sentinels.

The NEW full factory proof passes 1,874 checks per profile: all 964 persistent
C++ allocation prefixes, all 903 cancellation points, accumulated-coefficient
bounds, prospective work/pixel refusal, finite/negative source guards, missing
binding and retry. It measures RGB error .0024908, coverage .00306373, coefficient
1.58333, 2,888,768 working bytes and 9,168,976 sampling units. Previous full broad
sweeps are not repeated. Legacy controls pass 8/5/5 separately; genuine moving
upper-role coupling still refuses with 18 actual worker/history checks. Ordinary
shared/colour/common-UV writers produce all 29 prior files byte-identically in
each profile.

Strict PCI03 A770 handle `44160` passes 517 checks / 108 independent pairs,
maximum 1, in 9.351 seconds. Actual Intel/Arc/A770 GL strings, DRM descriptors
resolving to `0000:03:00.0`, mappings and driver hashes are captured. The same
binary refuses actual llvmpipe with its hardware flag. All 1,894 inputs remain
exact; the lease is released. No compute, GUI, physical HMD or external viewer
claim follows.

Eight writer outputs plus two final authored inputs and the original counter
input pass 90 pinned official schema instances. Unfiltered pinned Khronos
reports 0 errors, 47 warnings, 173 infos and 36 hints. Twenty extra read-only
checks prove unchanged ignored fourth shade components and actual requested
PNG dimensions across all eight writers. Configuration registers 656 tests,
including five new dependent coefficient tests; this is registration only.
See [the receipt](validation/VRM1-WIDE-SHADE-EXPRESSIONS.json) and
[original counter](validation/VRM1-WIDE-SHADE-COUNTER.json).

## Remaining full-goal work

General MASK/BLEND coupling, initial shared factors outside the unit texture/
factor representation, negative or otherwise native-unrepresented targets,
moving upper roles under differing/zero UV targets, prepared/normal/MatCap/rim/
absolute/plain-PBR adapters and broader real external consumers remain required.
No guard for those owners is removed. The original SAME long sanitizer sweep
`64560` remains independently unfinished; the new branch proof does not replace
it. Root normal/native/key schema composition and fresh application qualification
remain separate before main admission.
