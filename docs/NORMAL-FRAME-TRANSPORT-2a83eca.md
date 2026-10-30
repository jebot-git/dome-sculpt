# Normal frame transport contracts under development

Changing a texture's sampling atlas must not silently change the frame in which
its normal vectors are encoded. The current `SourceMaterialNormalFrame::UvDerivative`
describes the interim SourceUV implementation. It is not a portable declaration
of glTF or MToon tangent semantics. Derived normal roles remain guarded for VRM
export until original-frame transport and consumer fixtures establish equivalence
through morphing and skinning. The immutable origin retains the source needed
for that work. A rest-pose image comparison does not establish this equivalence.

## Primary contracts

The glTF references below are pinned to
`a55d8458c1e82720413acd3b420fd09e73b8e40b`.

* [Meshes, lines 1626–1633](https://github.com/KhronosGroup/glTF/blob/a55d8458c1e82720413acd3b420fd09e73b8e40b/specification/2.0/Specification.adoc#L1626-L1633)
  recommends MikkTSpace when a normal texture has no supplied tangents, using the
  normal texture's associated coordinates. The bitangent uses the cross product
  of normal and tangent multiplied by tangent W; inconsistent W within a
  triangle has undefined results.
* [Morph targets, lines 1777–1788](https://github.com/KhronosGroup/glTF/blob/a55d8458c1e82720413acd3b420fd09e73b8e40b/specification/2.0/Specification.adoc#L1777-L1788)
  applies morph displacements before skinning and node transforms. Morph tangent
  accessors have three components and do not displace handedness. If base
  tangents are generated, target tangents must also be generated with the same
  algorithm and target positions, normals and coordinates; supplied tangent
  displacements are ignored in that case. Supplied base tangents with no target
  tangent displacement stay unchanged by that target.
* [Skinning, lines 2005–2011](https://github.com/KhronosGroup/glTF/blob/a55d8458c1e82720413acd3b420fd09e73b8e40b/specification/2.0/Specification.adoc#L2005-L2011)
  defines the weighted joint-world and inverse-bind matrix transform. Tangent
  transport must distinguish direction vectors from positions and retain the
  normal/vector transform conventions used by the target consumer.
* [KHR_texture_transform, lines 23–50](https://github.com/KhronosGroup/glTF/blob/a55d8458c1e82720413acd3b420fd09e73b8e40b/extensions/2.0/Khronos/KHR_texture_transform/README.md#L23-L50)
  defines the affine sampling transform and coordinate-set override. That text
  does not explicitly specify how an authored tangent frame changes with this
  transform. The core fallback recommendation and this sampling rule alone do
  not establish a universal authored-TANGENT transformation policy.

This distinguishes a recommendation to generate missing base tangents from the
required target-generation behavior once that generation path is selected.
Generating only a base accessor can alter a morphing model: it changes which
target tangent behavior the consumer selects.

## Versioned consumer evidence

Khronos Sample Renderer is an implementation reference, not an additional
normative clause. At `6e8a3d1aaa6f07e9f1310a488453e21325677786`:

* [renderer.js](https://github.com/KhronosGroup/glTF-Sample-Renderer/blob/6e8a3d1aaa6f07e9f1310a488453e21325677786/source/Renderer/renderer.js#L1764-L1771)
  enables `HAS_VERT_NORMAL_UV_TRANSFORM` for a normal texture transform.
  [material.js](https://github.com/KhronosGroup/glTF-Sample-Renderer/blob/6e8a3d1aaa6f07e9f1310a488453e21325677786/source/gltf/material.js#L127-L162)
  supplies its affine matrix.
* [primitive.vert](https://github.com/KhronosGroup/glTF-Sample-Renderer/blob/6e8a3d1aaa6f07e9f1310a488453e21325677786/source/Renderer/shaders/primitive.vert#L142-L159)
  multiplies the world tangent and bitangent by that matrix when authored
  tangents exist.
* [material_info.glsl](https://github.com/KhronosGroup/glTF-Sample-Renderer/blob/6e8a3d1aaa6f07e9f1310a488453e21325677786/source/Renderer/shaders/material_info.glsl#L125-L162)
  uses transformed normal sampling coordinates for its derivative fallback.

Those two paths must be tested separately. The authored path's direct use of a
full affine UV matrix on world vectors must not be generalized into a portable
file-format contract without independent confirmation.

The same consumer's [animation.glsl](https://github.com/KhronosGroup/glTF-Sample-Renderer/blob/6e8a3d1aaa6f07e9f1310a488453e21325677786/source/Renderer/shaders/animation.glsl#L73-L95)
blends per-joint normal matrices, constructed by inverse transpose in
[skin.js](https://github.com/KhronosGroup/glTF-Sample-Renderer/blob/6e8a3d1aaa6f07e9f1310a488453e21325677786/source/gltf/skin.js#L95-L107). Its
[primitive.vert direction functions](https://github.com/KhronosGroup/glTF-Sample-Renderer/blob/6e8a3d1aaa6f07e9f1310a488453e21325677786/source/Renderer/shaders/primitive.vert#L83-L115)
use that blend for normals and the weighted linear skin matrix for tangents,
after local morph addition. This differs mathematically from inverting and
transposing the already blended skin matrix under mixed anisotropy. The helper
must preserve raw local normal and tangent displacements until a versioned
consumer convention is selected. Legacy Unity's corresponding weighted normal
policy has not been established here; it must not inherit a generic skin label
merely because its rest-pose result matches.

Legacy MToon0 has different concrete evidence. In UniVRM
`efd20f62f1bf0ac3fcc34de1e497324c5197d146`,
[MToonCore.cginc](https://github.com/vrm-c/UniVRM/blob/efd20f62f1bf0ac3fcc34de1e497324c5197d146/Packages/VRM/MToon/Shaders/MToonCore.cginc#L70-L76)
constructs the world frame from vertex normal/tangent and object reflection
sign. [Sampling and normal evaluation](https://github.com/vrm-c/UniVRM/blob/efd20f62f1bf0ac3fcc34de1e497324c5197d146/Packages/VRM/MToon/Shaders/MToonCore.cginc#L126-L161)
apply `_MainTex_ST` to the sampled coordinates while leaving that frame intact.
This establishes this consumer's legacy convention; it does not establish the
same rule for every glTF material or KHR transform.

MToon1 must be considered separately. The
[VRMC_materials_mtoon 1.0 specification](https://github.com/vrm-c/vrm-specification/blob/94e82dd346fa6cf0337c4421728640e5252dd38e/specification/VRMC_materials_mtoon-1.0/README.md#surface-normal)
uses the core glTF normal texture. Its
[transform-order section](https://github.com/vrm-c/vrm-specification/blob/94e82dd346fa6cf0337c4421728640e5252dd38e/specification/VRMC_materials_mtoon-1.0/README.md#compatibility-with-khr_texture_transform)
specifies animation before KHR sampling transformation. The same pinned
UniVRM's [MToon1 UV helper](https://github.com/vrm-c/UniVRM/blob/efd20f62f1bf0ac3fcc34de1e497324c5197d146/Packages/VRM10/MToon10/Shaders/vrmc_materials_mtoon_geometry_uv.hlsl)
instead applies Unity ST before animation, while its
[normal helper](https://github.com/vrm-c/UniVRM/blob/efd20f62f1bf0ac3fcc34de1e497324c5197d146/Packages/VRM10/MToon10/Shaders/vrmc_materials_mtoon_geometry_normal.hlsl)
uses the interpolated vertex frame. A Unity material convention cannot be
substituted for the MToon1 file-format contract.

## Required transport and verification

A completed contract needs explicit source format/version and frame provenance,
separate sampling coordinates, original normal-frame coordinates, authored or
generated base tangents and handedness, target tangent displacements, and the
applicable skin/vector transforms. Unknown provenance must reject atomically.
Original raw appearance archives must remain unchanged.

Independent fixtures must cover supplied and missing tangents, UV0/UV1 selection,
rotated and reflected sampling transforms, mirrored islands, object reflection,
nonzero position/normal/tangent morphs, and anisotropic or skewed weighted skin
transforms. They must compare world normals from original and derived files in
an independent consumer through multiple poses, including the combined morph
and skin case. A static rotation between two rest tangent frames is insufficient:
its relation can change under anisotropic deformation. Schema and accessor
validation alone do not prove those shading results.

## Preparation helper under integration

`src/source-tangent-frames.hpp` prepares original local frames independently of
the sampling atlas. Authored base tangents and their supplied target deltas are
retained. Missing base tangents select MikkTSpace for both base and every target;
target positions and supplied normal displacements participate, while absent
normal displacements retain the base normal. Supplied tangent deltas are ignored
only on that generated path. Results retain triangle-corner identity, normal
displacements and original target order. Export/renderer integration must split
and remap all attributes at incompatible corners rather than average them.

The local evaluator applies explicitly source-indexed morph weights before
skinning, retaining tangent W and untransformed normal/tangent vectors. It does
not choose a KHR versus Unity convention or perform skinning. Work, result memory
and cancellation are bounded; unsupported handedness changes reject.

The fixture `tests/source-tangent-frames.cpp` compares a rotated-UV triangle to
an independent analytic derivative, checks supplied tangent preservation and
mirrored UVs, and demonstrates that a static rest-frame normal conversion changes
under anisotropic scaling. It parses an actual emitted GLB with cgltf and compares
its accessor-derived normals at nonzero morph weights under anisotropic and
reflected transforms. That is an independent parser plus analytic CPU consumer;
it is not yet an independent graphical-consumer, weighted-joint skinning or
headset proof. The shared original-frame runtime representation and those broader
fixtures remain required before removing the derived-normal guard.
