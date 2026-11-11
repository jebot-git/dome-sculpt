# Actual alternate-coordinate emission source refusal and scoped fix

Exact current4977 core plus the test-only1dcfb042 fixture reproduced an actual
callback refusal after1 positive assertion: `Original texture coordinate set
requires explicit surface authority`. Preserve proof namespace
`radiance-bake-current-native-proof-20261111/native-release-v2` and its source,
whole-TU build/provider pins. Earlier native-release-v1 failed fixture construction
before0 assertions because UV1 initially lacked its paired image view; that
separate fixture defect and failed evidence also remain intact.

The callback already resolves the actual owned original UV array through
coordinates(), then incorrectly passes an unresolved role.texcoord1 to the ordinary
sample_source_texture(), whose default one-coordinate validation correctly refuses.
The new private implementation copies that role only after real coordinate
resolution and resets the sampler copy's texcoord0, following the existing
sample_surface_texture() pattern in source-maps.cpp. It retains actual original
UV ownership, affine transform, image and filtering. It changes no public or
private header definition, limit, guard or source data.

Typed MToon applies its slot transform explicitly through mtoon_uv(); its image
role is required to have identity affine and texcoord0 by
SourceMaterialBindings::validate()/roles_valid() (material-bindings.cpp). Retained
appearance roles are likewise constructed with identity affine. The independent
controls include genuine nonidentity MToon slot and raw PBR role transforms;
they select different original UV1/image coordinates from the separate nonuniform
writable UV0 FLOAT field. A new27th assertion rejects a forged second affine in
the typed image role, retaining the existing ownership contract.

This source child is unqualified until complete new emission-bake-source.cpp
and fixture translation units replace the matching193-member4977 core member
and fresh test ELF under exact source/header/provider pins. No frozen4977/5bb
source, provider or previous failure artifact is modified. Actual public app/
stereo/high-range final framebuffer tests remain separate.
