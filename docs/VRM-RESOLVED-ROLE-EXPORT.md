# VRM0 resolved role export checkpoint

This narrow fix follows static COLOR_0 checkpoint `d3e5e61aee5471b4c391a1cd6f804e76da2aff0f`.
It changes no public layout, native Scene codec or color preparation schema.

## Behavior

For an explicit immutable material binding, the current base and shade roles
alias when their image owner, sampler and effective typed UV identity agree.
Historical source texture indices remain provenance, not current role identity.
This matches the native renderer. Raw materials without a binding retain their
original descriptor-index alias convention. Different samplers or UV coordinates
remain distinct. This does not add role resampling or accept unsupported UV1.

An overlay whose common color and emission coverage are both exactly zero keeps
the original MToon role images, animated UV sampling and expression property
identity, with native base/emission factor overrides applied. It must still pass
all source-view, image, budget and unit-atlas validation. Nonzero fixed-UV paint
retains the existing animated/ST export guards. Zero coverage does not mean
silently freezing an animated source into an image.

## Independent graphical evidence

`tests/vrm-role-consumer.cpp` uses the actual built-in CC0 nineteen-part female
avatar and the actual Document mesh worker. It authors fixture metadata and
explicit low-level overlay maps; it is not evidence that native painting covers
every tiny UV face. The independent consumer parses exported GLB with cgltf,
decodes PNGs, evaluates position morphs and skinning with its own matrix code,
and uses its own GLSL material and transfer pipeline. It does not call native
morph, normal-frame or shader helpers. The consumer fixture has a deliberately
simple legacy MToon shadow ramp, no normal map/rim/matcap/outline, and no UV mask.
It is a material-role regression, not a claim of arbitrary MToon correctness.

The 20 assertions cover current equal roles with historical unequal indices,
protected/fractional/full overlay, morph weights 0/.35/1 and anisotropic hips,
a raw/no-binding control, and zero-overlay exported material-value expressions
at factors 0/.35/1 with moving UVs at times 0/.7. Independent parsed geometry is
compared for all nineteen parts and more than 10,000 triangle corners.

Final Release software and Intel Arc A770 runs pass all 20 assertions: maximum
8-bit channel error is 1 for the resolved overlay and 0 for raw and animated
controls. The same fixture linked to the old exporter fails its first resolved
case with error 25. Unit coverage passes 14 assertions. The changed exporter TU,
fixture and unit test also pass 20+14 assertions with ASan/UBSan/leak detection,
linked to the complete instrumented static-COLOR support build. This is not a
fresh run of every registered native test.

Qualification uses exact d3e5e61 support archives with the newly compiled VRM TU
linked first. All 52 public headers are byte-identical to that support source.
Link maps confirm that the old core VRM object is absent from patched binaries
and present in the negative control. The ordinary CMake target is registered and
configuration succeeds. The frozen validation manifest records sources,
binaries, support archives, link maps, commands, models and logs.

Five positive VRMs and the old-export negative control each have zero Khronos
validation errors and zero errors against the fifteen verified official VRM0
schemas (revision `94e82dd346fa6cf0337c4421728640e5252dd38e`). Each has the known
legacy VRM extension-name warning; the raw control additionally has nineteen
missing-tangent generation warnings. Informational unused/empty-node messages
remain. A schema-valid negative control still renders incorrectly.

## Versioned consumer provenance

The independent UV animation follows UniVRM revision
`efd20f62f1bf0ac3fcc34de1e497324c5197d146`:

* [MToonCore.cginc, lines 126–137](https://github.com/vrm-c/UniVRM/blob/efd20f62f1bf0ac3fcc34de1e497324c5197d146/Packages/VRM/MToon/Shaders/MToonCore.cginc#L126-L137),
  blob `204e6ca2484b65a340a991f5d1e45ebbe977f527`: main ST,
  mask/time scroll and counterclockwise rotation around .5.
* [MeshData.cs](https://github.com/vrm-c/UniVRM/blob/efd20f62f1bf0ac3fcc34de1e497324c5197d146/Packages/UniGLTF/Runtime/UniGLTF/IO/MeshIO/MeshData.cs),
  UV import at lines 497–502 and 620–625 uses ReverseUV.
* [UnityExtensions.cs, lines 133–135](https://github.com/vrm-c/UniVRM/blob/efd20f62f1bf0ac3fcc34de1e497324c5197d146/Packages/UniGLTF/Runtime/Extensions/UnityExtensions.cs#L133-L135)
  maps UV to `(x, 1-y)`. The consumer converts glTF UV into Unity coordinates,
  applies animation, then converts back. A first consumer implementation omitted
  this conversion and failed the .7-second case; native UV code was unchanged.
* [glTF 2.0 specification](https://github.com/KhronosGroup/glTF/blob/a55d8458c1e82720413acd3b420fd09e73b8e40b/specification/2.0/Specification.adoc)
  revision `a55d8458c1e82720413acd3b420fd09e73b8e40b`: node morph weights
  override mesh defaults. The independent evaluator observes that precedence.

## Remaining work

General colored-target bake preparation, persisted projection/quality receipts,
repeat bake and UV adoption remain separate active work. Actual COLOR_n and
TEXCOORD_n morph-channel import/export also remains open. Existing deforming
normal representation guards remain; this checkpoint neither removes them nor
claims that schema validation proves graphical equivalence.
