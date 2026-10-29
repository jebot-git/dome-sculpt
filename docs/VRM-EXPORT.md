# Source-preserving native VRM0 export

`export_vrm(objects, path, options)` writes a real binary `.vrm` with the original
VRM 0.0 extension and native edited geometry. Its return value reports mesh/joint
counts, changed morph-normal recomputation and diagnostics. It reuses the GLB
writer's exact emitted identities; no temporary glTF file or external converter is
used. Cancellation, bounded resources and validation precede the atomic write.

The first supported source version is **VRM0**, including the two verified CC0
VRoid beta baselines. This is not complete VRM1 support or a claim of complete
VRM interoperability. VRM1, node constraints, animation tracks, unknown source
extension remappers, non-MToon legacy shader families, missing source primitives
and source-less generated parts
currently reject explicitly. Ordinary GLB export remains available for those
native generated parts. No Unity, C#, Python or Qt runtime is introduced.

## Identity and authoring policy

Each creator part retains original mesh/node/primitive/material and compacted
vertex IDs. `source_joint_nodes` maps native skeleton slots to original glTF node
IDs. `source_morph_targets` maps native morph slots to original morph target IDs.
Both tables persist as bounded, little-endian uint32 native archive payloads;
unique/in-range IDs are validated. Old native drafts may omit them and still
open, but VRM export refuses missing identities. Bone names, display names and
unchanged array lengths do not reconstruct missing provenance.

Morph edit/reorder/delete/add operations must update or invalidate the mapping,
even when the target count remains unchanged. This exporter consumes that explicit
contract. It does not infer whether a caller has incorrectly retained stale
identity metadata. Future authoring commands must uphold the same invariant.

All parts must share one actual grouped rig, pose and object transform. The GLB
writer deduplicates exact compatible native rigs; the VRM path verifies that every
part references the same emitted joint nodes. Source joint parent relationships
must agree exactly with native parents. Rest inverse binds are rebuilt from the
edited rest skeleton after restoring source basis; original source matrices
remain exact in the native archive.

Source mesh splitting is explicit: an original mesh containing ten primitives
becomes ten emitted meshes. First-person mesh annotations and expression binds
expand to each emitted mesh, and every expression target is independently mapped
to its native output morph slot. Humanoid bones, first-person head, spring roots,
centers and collider nodes use exact node remaps. Collider group indices retain
their original order. Material-value expressions retain source material names,
which must resolve to actual emitted materials.

## Materials and other metadata

The export retains complete original author/license/usage permissions, thumbnail,
look-at curves, first-person offsets/flags, expressions and spring/collider
parameters. A proven uniform fit scales authored offsets and radii; it does not
redesign spring physics. Nonlinear rest fitting currently rejects pending an
explicit local metadata-refitting path.

All original PNG/JPEG image payloads are embedded byte-for-byte, including the
thumbnail and additional MToon maps. Original textures retain independent sampler
constants and source image identity. MToon0 texture properties, including shade,
rim/matcap and data masks, are remapped explicitly. Material extension paths with
unknown index semantics reject rather than guessing reference identities.

The ordinary GLB writer neutralizes baked generic paint factors. This VRM0
milestone supports source-only bindings and raw factor edits; derived painted
MToon bindings explicitly reject until exact coverage and aliased shade
reconstruction are available. Dividing by a zero baked base channel cannot
recover nonzero original shade data. A future painted path must also handle or
explicitly reject shared nonidentity MainTex ST instead of rebaking only one
role. Raw factor edits retain original typed factors when the generic fallback
is unchanged; edited nonzero-source channels use a relative ratio, and edited
zero-source channels use an explicit absolute override. Schema-unsupported HDR
base results reject rather than clamp.

Secondary UVs, colors and tangent handedness can be retained in ordinary GLB
through the original compacted vertex map. Missing requested UV sets and
unrepresentable texture shear reject. These extra attributes remain source data;
this exporter does not claim to regenerate modified authored tangent frames.

POSITION morphs come from native editable targets. Unchanged source morphs retain
exact NORMAL/TANGENT deltas through their source vertex map. For changed geometry
or morph positions, normals are recomputed from the native target geometry and
exported as target-minus-base deltas; the return report names this policy. Exact
original deltas remain in the self-contained native source archive. Changed
TANGENT morphs currently reject until a corresponding tangent recomputation path
is implemented.

## Validation and limits

The native test exercises strict metadata types/enums/references and exact
remapping, including independently reordered expression targets and malformed or
missing identity tables. Real fixtures create each actual CC0 avatar, save and
reopen the native Scene, and export all 19 parts with one actual skeleton.
Independent cgltf parses/validates accessors, buffers, materials and hierarchy.
Independent matrix evaluation checks actual joint/inverse-bind skinning against
native preview, including an edited head pose and actual serialized nonzero morph weights across
all parts. Female has 560
source NORMAL morph channels; male has 540. Expected counts derive from each
actual source rather than a shared hardcoded assumption.

The external Khronos glTF Validator checks the binary container/core glTF; it does
not validate the VRM extension. The pinned official VRM0 schemas are independently
checked with JSON Schema Draft4 in a temporary test-only Python environment.
Neither validation helper is a runtime dependency. Schema validation plus native
and independent accessor/matrix tests are useful evidence, but do not replace
loading in an independent VRM consumer or verifying expression, first-person and
spring behavior there. Those consumer/device checks remain pending.

Primary references are the official specification at commit
`94e82dd346fa6cf0337c4421728640e5252dd38e`:

- <https://github.com/vrm-c/vrm-specification/tree/94e82dd346fa6cf0337c4421728640e5252dd38e/specification/0.0/schema>
- <https://github.com/vrm-c/vrm-specification/tree/94e82dd346fa6cf0337c4421728640e5252dd38e/specification/VRMC_vrm-1.0>

Native validation independently implements the functional field types, enum and
index rules; schema descriptions or unlicensed schema files are not vendored.
All 15 exact inspected official schema hashes are listed below. MToon adaptation
provenance/licensing is separately recorded in MTOON.md and MTOON-LICENSE.txt.

- `vrm.blendshape.bind.schema.json` SHA-256 `94690cf6df8671a1834ab2a555a2dd24cb62213d0be5f182fc2e37ac1de75221`.

- `vrm.blendshape.group.schema.json` SHA-256 `f7c0552659123ef4be7885f493f0cf9a18c7937dd4887866b687de9ca838942b`.

- `vrm.blendshape.materialbind.schema.json` SHA-256 `dc3327f2490a6763696668cc987d62e80e05ff6a11e64d956bc86d5d589d690e`.

- `vrm.blendshape.schema.json` SHA-256 `48d86efa2a8f57b8e799ecb1d7a4ca1d4cf583c036a1307f15e19f6ade235e15`.

- `vrm.firstperson.degreemap.schema.json` SHA-256 `fb2e3b5cefcdfc9765a14eb09b4c79f5e69aa78bcc542279af0193944665061c`.

- `vrm.firstperson.meshannotation.schema.json` SHA-256 `4bd400caf1296560ea4ec10347ea8bf2a86781349450555514e8f8b5b178c11a`.

- `vrm.firstperson.schema.json` SHA-256 `64127cb50bae24f569dc1c6fc72eb588f11a3c39ed83f9f4c2dea2455b47b495`.

- `vrm.humanoid.bone.schema.json` SHA-256 `de77590a2bee2daa02bd40576e3aade4b114cfd1c8efc51e0566b8dc8d611f2b`.

- `vrm.humanoid.schema.json` SHA-256 `0d5ac42a32b5350d9dc31cddd693ddfe9724efad205f054d097fd3a3d800b33b`.

- `vrm.material.schema.json` SHA-256 `b084587aa63a9486f2cc44f2aad3f57ec9a2f6f3ff4c003f61ef2a2d335dcf50`.

- `vrm.meta.schema.json` SHA-256 `d3d5a45f86902b0d3df8ad71c7e1ee6874967579d787585eaa9e7302dc3bbc87`.

- `vrm.schema.json` SHA-256 `32ad3187458eb5f7d4a735adf11002f7d1b80c1565c2f429962fbcfb20afd54c`.

- `vrm.secondaryanimation.collidergroup.schema.json` SHA-256 `60bc92e5237eb6eb3b85d44941722814f6adb4e9e3c127ef28f19aec93210f45`.

- `vrm.secondaryanimation.schema.json` SHA-256 `a76718238953979f42cd232cb76f48c2ec4508a1c65ddb66833e273b2f9ef046`.

- `vrm.secondaryanimation.spring.schema.json` SHA-256 `21b98512fe905d8e363eac64d683a936a451fd71e0a066362482e14b816bc636`.

Native alpha/cull changes that diverge from the retained MToon0 render state
currently reject until typed state editing is integrated. Cutoff and raw
base/emission factor edits are mapped directly. Edited authored tangent frames
also reject rather than silently exporting a stale basis.

## Coordinate basis and fitted physics

`source_basis_quaternion` explicitly records original→native coordinates; it is
optional so legacy absence remains unknown. Native archives preserve the exact
four unit-quaternion components. Export refuses missing, inconsistent, nonfinite
or nonunit provenance. The actual beta adapter records its 180° Y conversion.
The exporter inversely transforms mesh positions/normals, morph vectors, rest and
pose TRS, and group/object transforms before glTF generation. This restores the
original VRM0 coordinate frame rather than rotating only rendered geometry while
leaving physics/look-at references in a different frame.

Source-local first-person and spring/collider data require more than basis
conversion. This milestone proves a single uniform scale/translation against all
original source vertices and native rest-joint world positions. It also verifies
that restored rest axes/scales match the original node frames. First-person and
collider offsets, collider radii and spring hit radii scale by that proven factor.
Source normals and morph-normal deltas remain exact under this positive uniform
fit. Nonlinear body/head proportion fits or edited rest axes/joint positions
currently reject until an explicit local metadata-refitting path is implemented.
Pose and position-morph edits remain supported; changed morph normals are
recomputed and reported. Nonuniform joint-scale conjugation that requires shear
also rejects.

Independent fixtures compare exported skin/pose results to native results after
the inverse basis, check all 19 normal accessor directions, measure physics scale
from native/original bone distances, and verify unknown legacy basis cannot
replace valid output. Schema success alone does not establish coordinate fidelity.

Derived painted MToon export is currently rejected until the compositor supplies
exact color coverage and aliased shade reconstruction. In particular, a zero
base-color channel can retain a nonzero authored shade channel; division by a
baked factor cannot recover that source information. Raw source previews remain
exact, including explicit zero-channel color edits. The first native export
fixtures intentionally use `Scene::preview_surfaces()` for those unpainted source
bindings. Full painted export is the next integration step, not implied by a
source-preserving avatar export.

## Validated milestone evidence

The final source-only fixture passes **138 native assertions** in optimized
Release and **138 assertions with ASan/UBSan/leak detection**. The new exporter,
VRM module and test are instrumented; the core archive dependency in that focused
sanitizer run is the coherent Release build. Separate provenance/archive tests
pass 53 assertions in both Release and ASan/UBSan/leak detection, including both
actual original source blobs.

All eight emitted avatars (baseline, posed/nonzero morph, raw and zero-channel
tint for each gender) independently pass the pinned official VRM0 schemas with
zero errors and Khronos glTF Validator `2.0.0-dev.3.10` with zero errors. Each has
20 warnings: the legacy `VRM` extension name and 19 runtime-generated tangent
frames. The validator also reports the unsupported legacy extension and source
textures/empty container nodes as informational entries. Those warnings are
retained, not suppressed. Source assets do not supply base tangents; portable
explicit tangent generation remains further interoperability work.

Logs: `artifacts/vrm-native-basis-export.log`,
`artifacts/vrm-native-basis-sanitizers.log`,
`artifacts/creator-archive-basis.log`,
`artifacts/creator-provenance-sanitizers.log`,
`artifacts/vrm-official-schema.log` and
`artifacts/vrm-khronos-validation.log`. Generated VRMs and individual validator
reports are under `artifacts/vrm-export-{female,male}/`. None of the generated
binaries, downloaded schemas, temporary Python environment or external validator
packages are runtime dependencies or vendored source.
