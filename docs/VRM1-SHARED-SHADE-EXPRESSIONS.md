# VRM1 shared base/shade painted expressions

Private runtime `f8f295e`, following `5310acb`, transports genuine colour paint
when the typed MToon1 base and shade roles share the same immutable image,
sampler and effective UV transform. The ordinary source material stays on the
lower primitive. One actual paint image is reused for the upper base and shade;
there are no dummy images, nodes, vertex-colour authority or tessellation.

The native shade sample is `S*(1-C) + P/nativeRGB`, with the existing zero-native
factor fallback. The upper material therefore uses paint `Q=P/C` and shade
factor `originalShade/nativeRGB`. Real shade expression targets undergo the same
linear division; their ignored fourth component remains unchanged. Base-colour
expressions still affect only the original lower material. This retains separate
paint contribution and original image/sampler/expression authority.

The pinned [MToon specification](https://github.com/vrm-c/vrm-specification/blob/94e82dd346fa6cf0337c4421728640e5252dd38e/specification/VRMC_materials_mtoon-1.0/README.md)
defines separate multiplied base and shade terms. The [expression rules](https://github.com/vrm-c/vrm-specification/blob/94e82dd346fa6cf0337c4421728640e5252dd38e/specification/VRMC_vrm-1.0/expressions.md)
accumulate differences from each initial material value. The quotient therefore
commutes with those expression sums. The serialized shade-factor schema has a
unit bound; expression targetValue float4 itself does not. This first adapter
also guards mapped targets outside the unit range to preserve its current
continuous image-error proof. That is a staged adapter limit, not a claim that
all wider standard VRM1 expression targets are unrepresentable.

## Evidence

`42eea6a` preserves the old worker's real shared-role refusal after two genuine
nonuniform colour/coverage strokes. All 14 counterfactual checks retain the prior
file and exact owning native Scene. The new authored INPUT retains original BIN
bytes and all other roles/metadata; pinned official schemas and Khronos report
zero errors.

Both profiles rebuilt all three direct production translation units for the
changed private layer struct. Public headers, layouts and persistent schemas
are unchanged. Copied support is the exact matching owned `5310` cohort, not
root35/anatomical headers or a fresh whole application. Composed proof includes:

- Actual worker 48: all 256/512/1024 policies, genuine paint Undo/Redo,
  ordered Save, package Open/second cycle, running and queued Cancel,
  source/file rollback, quota and final atomic publication controls.
- New-domain factory 1,567: all 912 persistent C++ allocation-denial prefixes,
  all 647 cancellation points, four previous target/authority controls,
  two current coefficient-adapter guards, native zero-factor fallback and retry.
  Each full sweep ran once per profile; previous colour/UV sweeps were not repeated.
- Independently parsed shared-role rendering 115 / 36 image pairs per profile:
  three resolutions, poses 0/2, weights 0/.35/1 and the two original views.
  Every pair has maximum difference 1 at unchanged bound 4. Wrong moving paint,
  tint and missing shared shade give differences 128, 84 and 16 respectively.
- Current unshared colour-only and UV workers/rendering plus existing
  static-expression 81, painted-authoring 762 and painted-image 22 controls pass.
  Ordinary colour/UV native/container outputs remain byte-identical to `5310`.
- Strict PCI03 A770 rendering 116 / 36 pairs passes, actual llvmpipe with the
  same hardware flag refuses. Process maps, library and complete input hashes
  are captured; handle 14205 is terminal and its lease released.

All 12 actual writer containers pass 108 official schema instances and
unfiltered Khronos validation: zero errors, 48 warnings, 208 infos, zero hints.
All 216 PNG and 29 native/container file pairs match across Release and sanitizer
profiles; the latter cohort explicitly includes authored INPUTs and three prior
queued-file sentinels. Configuration registers 637 tests, including the genuine
three-test shared-shade graph; this is not a full CTest or application result.

The initial composed wrappers remain exit 1: the wrong-UV sensitivity control
assumed TEXCOORD>=2, while the actual available carrier was channel 0. Worker,
factory and first five image pairs had already passed. Only the consumer was
corrected to identify the real upper primitive. The final shade sensitivity
selector also requires full image/sampler/UV identity, rather than shared image
pointer alone. Original negative logs and all immutable source/archive evidence
remain recorded; the original wrappers are not relabelled successful.

See [the exact receipt](validation/VRM1-SHARED-SHADE-EXPRESSIONS.json) and
[old-worker counterexample](validation/VRM1-SHARED-SHADE-COUNTER.json).

## Remaining work

MASK/BLEND, differing/zero UV targets, wider coefficient factorization, normal,
MatCap/rim/shift/UV-mask, prepared/key/native/physical, plain CorePBR and external
VRM application domains remain required owning adapters. In general native paint
can reduce replacement alpha; adding ordinary positive source-over primitives
cannot reproduce that while preserving the original lower alpha. Per-material
UV expression coupling also prevents treating a moving shade/emission role as
fixed paint. Those are actual design constraints, not permission to drop edits
or silently flatten original sampling. No physics/constraint simulation or
physical HMD/compute support is claimed.

Root35 genuine owner guards must survive semantic integration, and the private
struct change requires all three direct TUs. Full current application/UI/main
admission remains separate. SAME original unoptimized sanitizer sweep 64560 is
still live and unfinished; none of these proofs replace or sample it. Every
other unfinished original-goal requirement remains active.
