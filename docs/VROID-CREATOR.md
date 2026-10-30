# VRoid-only creator update

New humanoids use only verified CC0 VRoid beta female/male bases. The desktop choice and XR avatar page no longer offer MakeHuman. Embedded MakeHuman mesh data and its converter are removed; historical license/hash records live in docs/references/makehuman. Existing saved MakeHuman geometry/paint/rig/morphs and schema1/2 recipe metadata remain readable and exportable without automatic replacement. Calling its removed generator fails before publishing any Scene change. Opening the creator from a legacy group starts a new VRoid female draft with Keep existing objects checked.

The following details document the retained VRoid implementation; mentions of MakeHuman creation describe historical compatibility only.

# Actual CC0 VRoid beta creator baseline

New humanoid creation selects `vroid-beta-female` by default; the catalog also
supports `vroid-beta-male`. The legacy `makehuman-hm08` ID remains metadata only. These IDs
are stored in HumanoidControls schema 2 as `baseline`. Historical schema 1
records decode as MakeHuman with their exact saved field values. The baseline
factory supplies neutral VRoid shape offsets and white material tints, while
retaining historical MakeHuman parameter values solely for saved-project compatibility. Already-anime source
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
morph data remain the reference. Neutral hair controls retain the original19 parts exactly. Nonzero hair
length, volume or fringe controls append an original fitted swept-lock groom;
the source hairstyle and all authored geometry remain unchanged. This is an
editable parameterized extension, not a complete VRoid hair-guide editor. Complete VRM springs, look-at/first-person behaviors, constraints,
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

## Reproducible textured native roundtrip evidence

The optional native fixture exercises the same `append_humanoid` operation used
by the guarded Document worker. It saves and reopens a real project, checks exact
scene-record equality, then exports the reopened scene with original textures:

```sh
DOME_VROID_SCENE_FIXTURE=artifacts build/dome-sculpt-vroid-creator-test
blender --background --factory-startup --python tools/render-vroid-fidelity.py -- artifacts
python3 tools/compare-vroid-fidelity.py artifacts
```

The Blender step is optional development QA; the comparison script uses Pillow.
Neither is an application runtime dependency. Native GLBs must already exist;
the render tool fails if they are absent instead of substituting source files.

Both source-authored baselines insert all19 parts while retaining28 zero-area UV
faces and36,738 linked texels as explicit source diagnostics. They render all
source geometry; linked UVs paint shared source pixels and zero-area UV faces
remain unpaintable without losing render geometry. Their original UV/index/
skin/morph/remap data survives restart exactly.

Measured on2026-10-29, female native project size was119,795,368bytes and male
124,948,360bytes, both within the256MiB whole-project limit. Native GLBs exported
from reopened projects were15,724,236 and14,977,724bytes. Khronos validator
2.0.0-dev.3.10 reported zero errors, warnings, infos or hints for both exports.

Blender5.4 source/native unlit reference renders retained the authored face,
iris, skin, clothing and scalp appearance. Mean absolute8-bit channel differences
were female face0.00635/body0.000816 and male face0.00329/body0.000992; sampling/edge
pixels account for small nonzero differences. This compares the standard glTF
unlit fallback on both sides, not full legacy MToon lighting or VRM behavior.
Native GUI/XR tests are separate from these offline rendered interop fixtures.

A performance gap was also measured: insertion took about0.35seconds, while
uncached full-resolution `Scene::surfaces` compositing took about5seconds per
avatar. Immutable composition caching and GPU source/overlay blending remain
important for interactive editing; source fidelity alone is not a performance
completion claim.

## Original native fitted groom

`src/humanoid-groom.hpp` shares the original native cap/closed swept-lock kernel
with the MH fallback. The old MH topology/positions remain byte-equivalent
(topology identity14369968804387486394). For the actual beta sources a fitted
head frame, source triangle envelope and final finite-width cross-section clearance constrain
the generated rest shape. Nonzero controls append2059 vertices and3488 triangles
with independent nondegenerate UVs. Length changes lock reach; volume changes
the cap and strand silhouette; fringe changes front tips independently. Source
images/vertices/UVs/morphs and all19 source parts remain untouched. The additional
part has its own original256×256 procedural strand image, derived palette,
strictly validated paint UVs, and exact binding to the source head joint.

The head-rigid extension follows the same source skeleton without creating a
second guessed bone map. It has no spring simulation or arbitrary-pose collision
guarantee. Mild head pose and bounded extreme rest shapes have separate ray
intersection fixtures, including exact long-render controls. Seeded real torso/
shoulder interiors and2mm inward/outward torso probes verify the ray test can
detect actual clipping. Normals are normalized before ray classification to
avoid missing small near-tangent source triangles. Offline Blender full-body
renders add independent QA.
`artifacts/vroid-groom-{medium,long,fringe,posed,male}-{face,body}.png` are the
render examples. Regenerate them with
`DOME_VROID_GROOM_FIXTURE=artifacts build/dome-sculpt-vroid-groom-test` then
`blender --background --factory-startup --python tools/render-vroid-groom.py -- artifacts`.
The hairstyle is a native stylized foundation; flyaways, guide
editing, strand physics and salon-quality detail remain future work.

Creator source identity explicitly records native joint-to-source node and
native morph-to-source target arrays, plus an optional original-to-native
quaternion (beta sources use proper Y180). Absent legacy provenance must not
authorize VRM metadata reconstruction. A basis change alone does not refit
collider radii/offsets or first-person offsets after nonrigid proportion edits;
export must reject unsupported fitted metadata until its local-space fit is
explicitly remapped. Original metadata remains available unchanged.

Offline collision QA selects the nineteen imported source mesh nodes explicitly;
Blender creates an unrelated hidden unit `Icosphere` armature widget in
`glTF_not_exported`. It must not contribute to body BVHs or camera bounds. The
initial all-data-object report (long33 first-ray/2 two-ray hits) included that
widget and was invalid; the corrected exact-long source-only report is0/0.
Native seeded interior and near-surface controls remain independent evidence.

The corrected independent Blender reports were medium/long/fringe/male0
first-ray and0 bidirectional vertices. A0.24rad posed example still has5 shoulder
penetrations around0.7–2.4mm; the0.18rad native pose fixture stays clear. This
explicitly bounds the current head-rigid fit and motivates future pose-aware
collision/spring handling rather than a claim of collision-free animation.
