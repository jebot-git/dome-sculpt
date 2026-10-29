# Actual CC0 VRoid beta creator baseline

New humanoid creation selects `vroid-beta-female` by default; the catalog also
supports `vroid-beta-male` and the explicit `makehuman-hm08` fallback. These IDs
are stored in HumanoidControls schema 2 as `baseline`. Historical schema 1
records decode as MakeHuman with their exact saved field values. The baseline
factory supplies neutral VRoid shape offsets and white material tints, while
restoring historical MakeHuman defaults for the fallback. Already-anime source
models are not blindly enlarged or recolored using the earlier MH defaults.

The actual source files, author grants and exact hashes are in
[assets/humanoid/vroid](../assets/humanoid/vroid/README.md). Both are unchanged
VRM 0.0 exports from VRoidStudio-0.14.0, authored by pixiv Inc. / VRoid Project,
with embedded `licenseName: CC0`. The author's explicit older-beta base-model
CC0 grants apply; the general restrictions on current proprietary presets are
not silently substituted. No VRoid application/source/runtime is used.

The original source files retain geometry, indices, UVs, authored normals,
weights, inverse-bind matrices, facial targets (including normal/tangent source
channels), embedded PNGs, material definitions and all VRM/MToon/avatar metadata.
The native adapter produces one editable part per authored primitive/material
(19 in each base), with a `primary_part` pointing to the main body skin. It does
not flatten textured models into a few monochrome meshes. Repeated material
references share the same original asset and image cache.

Only accessor vertices unreferenced by a particular primitive are compacted.
Every resulting vertex retains its exact authored accessor index in
`CreatorAppearance::source_vertices`; no referenced triangle/UV/weight/morph is
silently dropped or remeshed. The proper 180-degree Y coordinate-basis rotation
converts VRM0's -Z-facing/right+X convention to native +Z-facing/left+X. There is
no reflection, triangle-winding flip or UV flip. Metric height and a small
floor offset are applied coherently to mesh/joint data. Neutral defaults use the
authored grounded body height, preserving all relative proportions.

The female source has 83 joints and the male 67. Their original normalized
parent-local joint frames and actual hierarchy are checked before fitting.
Every authored inverse-bind matrix is compared against native inverse binds
from that source hierarchy; a mismatch fails explicitly. The verified legacy
female Face skin's `skeleton` hint names Head although the joint array contains
the whole rig. That original advisory value remains retained with a named
legacy-compatibility diagnostic. General glTF validation stays strict; the
adapter explicitly opts into the known VRM0 advisory-hint compatibility path.
No arbitrary bind-space matrix is silently normalized or discarded.

The source humanoid assignments are validated after converting old thumb role
names to the VRM1 semantic vocabulary. Native sparse weights retain all positive
influences under explicit normalization. Female/male face targets are retained
with stable source indices and native vowel bindings; original normal/tangent
morph channels remain available in the preserved ImportedAsset and original
bytes even where native geometric preview has narrower support. This is not a
claim that all VRM runtime behaviors or packaging are complete.

## Texture and material ownership

Each part's `CreatorAppearance` shares the exact original immutable VRM byte
blob, ImportedAsset, original decoded image table, source mesh/node/primitive
and material IDs, full material JSON, classic MToon JSON and full VRM JSON.
`material` indexes ImportedAsset's material table (which has a synthetic fallback
at index0); `source_material` is the original glTF/VRM material index. The source
material/texture references are preserved, including unlit flags, alpha mode,
culling, samplers, normal/AO scales and unmapped shader fields.

PNG decoding is bounded and cached once by original image index. The source
cache retains at most two verified baseline entries. Female has 25,166,016
source texels (~96 MiB RGBA), male 26,083,520 (~99.5 MiB), across 25 PNGs each.
Non-square and 2048² textures remain at their authored dimensions. Source views
use the original sampling rules; factors remain separate and apply once.
`HumanoidPart::source_maps` is prepared for the native immutable source-map
painter rather than baking factors into resampled pixels. Additional MToon
texture slots remain in CreatorAppearance independently of preview support.
Scene integration must retain/deduplicate the source archive and image registry
on project save/reopen, and preserve the existing file/image resource budgets.

Licensed asset identity is verified by SHA256 before parsing. The native
implementation follows NIST FIPS180-4; it does not invoke a process or require a
crypto/Python runtime. The locator checks the executable-relative installed
share directory, configured development assets and configured installation
prefix, without relying on the current directory. An explicitly supplied asset
directory is authoritative for tests/tools. Wrong or missing baseline bytes
raise a concrete error; they never cause an automatic MH substitution.

## Scope and verification

Neutral baseline preservation is the first acceptance condition. Body/style
controls are bounded relative deformations of the actual source; source rig and
morph data remain the reference. Hair controls do not currently replace these
preserved hairstyles, and texture/tint editing is distinct from full hairstyle
construction. Complete VRM springs, look-at/first-person behaviors, constraints,
material expressions and exact MToon shader/export parity remain integration
work. The MH fallback remains useful for its authored anatomy, but is not
presented as the newly requested CC0 VRoid baseline.

`tests/vroid-creator.cpp` checks exact asset identity, original geometry/UV/normal
mapping, untouched neutral material factors, texture cache/sampler references,
raw shader metadata, source inverse-bind checks, distinct female/male rigs,
legacy control migration, cancellation and wrong-source rejection. Tests and
rendered evidence must distinguish original textured source fidelity from
currently supported runtime shading. Existing native GLB validation and Blender
interop fixtures provide the downstream export/visual checks.
