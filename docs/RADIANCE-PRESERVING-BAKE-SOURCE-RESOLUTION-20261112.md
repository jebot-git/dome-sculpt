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

Frozen source63c3199d now has a terminal0 Release closure: both complete
translation units, one complete member replacement in the193-member current4977
core,192 byte-exact untouched members,103 byte-exact public headers and exact
source/provider pins. Source27, original worker16417, public fixture producer32794
and all seven complete native inspectors pass with original deadlines; source/
provider drift is empty. Accepted/redone and original/undone endpoints are exact.
Desktop slider-roundtrip and XR defaults happen to produce identical native
endpoints for this actual fixture, rather than being assumed equal.

The adjacent Release JSON pins exact source/core/ELFs/logs and durable evidence.
The earlier8b-vs223 actual legacy1 export regression also passed32 old controls:
whole/subset/second-cycle GLBs and logs are byte-exact, preserving v1 wire/data.
Its separate legacy-v1-byte-compare-v1 receipt retains both binary/source identities;
no cross-ABI linking was performed.

This milestone validates callback27 and actual plain-source/default producer
workflows. It does not qualify full UV1 or typed-source Document Bake adoption.
Those real worker/Save/history/export cases are the next separate test child.
Actual public app/stereo/high-range final framebuffer tests and strict sanitizer
corrected-source closure remain separate. No frozen4977/5bb source/provider or
original failure artifact is changed by this documentation child.
