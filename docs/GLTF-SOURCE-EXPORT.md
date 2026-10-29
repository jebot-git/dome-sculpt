# Source-aware glTF export

The native GLB writer preserves independent original normal/AO images, strengths,
samplers and UV transforms for derived painted surfaces. Source-only texture
snapshots additionally export raw base/metallic-roughness/emission images with
material factors applied exactly once. Derived paint composites retain neutral
factors because their texels already contain those factors.

PNG images are shared by immutable image identity independently of sampler
identity. Sampler constants and default omissions survive; a single image can
have different wrap/filter bindings. Affine source UV transforms are decomposed
into KHR_texture_transform rotation/scale/offset. Unrepresentable shear and absent
UV sets reject transactionally before the atomic output write.

Creator primitives sharing the same group, exact object transform,
skeleton, humanoid mapping and pose share one emitted bone hierarchy and skin.
Distinct groups, skeletons, poses or object transforms remain independent.
Materials and morph-bearing meshes retain individual identities. This is actual
shared glTF skinning, not merely application metadata connecting duplicated rigs.

Independent cgltf fixtures validate raw and derived maps, sampler/image sharing,
normal/AO strengths, UV transforms, neutral composite factors, shear rejection,
shared rigs and separation of differently transformed parts. Full VRM metadata
and source morph-normal preservation are separate work; this milestone does not
claim VRM export.
