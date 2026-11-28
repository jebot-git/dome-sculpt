# Native HDR emission interchange — first GLB checkpoint

The historical GLB-only scope below is preserved. The later qualified VRM and
file-worker continuation is in [HDR-VRM-EXPORT.md](HDR-VRM-EXPORT.md).

## Prepared-source Standard GLB

The current Standard GLB writer can combine physical paint with the
original emissive texture on an imported Core PBR mesh with a writable
atlas. For a varying background it evaluates original UVs with the paint
atlas's retained triangle samples. One GPU pass applies the original
texture transform, wrapping and level-zero filter, then resolves the
FLOAT radiance and paint coefficients. The GLB fallback samples the
resulting static atlas. Its resolution bounds the detail it can represent.
The native Scene retains the original image and sampler. Other roles keep
their own images and coordinates unless Standard GLB compiles their paint.

Package version 7 uses the `prepared-source-domains` encoding suffix and
bounded FLOAT32 scanlines. At least one field has `domain.kind` set to
`prepared-source` and `domain.version` set to `1`. Its `domain.texCoord`
identifies the writable atlas on the emitted mesh. The reader verifies
ordered triangle positions and UVs, the fallback sampler and its reported
error, then replays the exact FLOAT field from the native Scene. This
supports vertex splits introduced when exporting source tangent frames.
Original minification behavior and finer source detail stay available in
the editable project.

A prepared Core PBR Bake can use this package with version 2 FLOAT fields.
The GPU composes baked color, metallic, roughness and occlusion with
original textures and editable paint. The compiler samples the combined
source and baked normal in the rest pose and encodes it in the same tangent
frame the GLB emits. If the mesh has no tangents, the exporter generates
MikkTSpace tangents and tangent morph deltas on the writable atlas before
it emits any texture coordinates. The native project retains the original
source, bake recipe, exact FLOAT coefficients, normal layers and deformation.

Prepared Core PBR Bake can export varying MASK alpha when every triangle
has the same visibility throughout its native and emitted filtering domains.
Source alpha bounds include every source texel for wrapping or mipmapping.
Clamped non-mipmapped source roles can use tighter bounds on the actual
affine UV footprint. The compiler checks the padded PNG taps and original
vertex alpha against the cutoff. It can move the cutoff within a proven gap
to account for quantization. Cutout edges within a triangle still require a
portable representation with a continuous visibility proof.

Static imported MToon 1.0 can carry physical and material-relative emission
paint into Standard GLB. The exporter maps writable texels through the
actual original UV0 or UV1 and the typed emission transform. The selected
GPU samples the original image and resolves the premultiplied paint with
the authored emission strength. The GLB keeps the original MToon roles and
uses the writable atlas for the resulting emissive texture. The native
package retains the original sampling, all FLOAT coefficients, and the
editable layers. Dome Sculpt can display a portable re-import that uses
an additional retained UV set for the compiled emissive texture.

Prepared static MToon1 HDR Bake uses the same package with a compiled
base color, authored MToon normal, ORM and emission on the writable atlas.
It requires the original shared glTF base role. An independent shade
role keeps its original sampler. When base and shade originally share
a sampler, the selected GPU resolves a separate shade image on the writable
atlas. The image includes the original shade factor and the ordered paint
contribution, including the native rule for a zero base channel. The writer
uses a neutral shade factor and reports an export error when a shade sample
exceeds the standard texture range. The export encodes the combined normal
in the emitted tangent frame and keeps the rim sampler. The selected GPU
also composites the core material and emission textures. Projection ray
queries and normal encoding run on the host. The native package keeps the
independent original Core PBR and MToon normal frames, Bake
coverage and absolute FLOAT donor radiance. An ordinary donor keeps its
radiance regardless of the receiving material's emission gain.

When a standalone imported MToon Bake projects a typed donor, the native
project can store separate Core PBR and MToon normal directions on the same
writable atlas. The `paired-mikk-v1` Bake payload retains the two images
under the original-source material and geometry binding. Earlier single-plane
Bakes remain readable. Standard GLB emits the authored MToon plane in its
`normalTexture` and recovers both planes through the native package. The UV
workspace applies new charts over the same ordered geometry. It transports
projected normals through the old and new tangent frames, filters FLOAT
emission and layer samples, and retains the original UV mapping for each
material sampler. The selected compute GPU filters the native Bake emission
coefficients in bounded batches from the original dense or packed field.
This preserves subnormal values and supports the full finite baked radiance
range. Apply rejects a result if its nonzero energy cannot be represented
with the rounded coverage. Atlas correspondence, byte-channel and layer
transfer, tangent-frame conversion and native field storage run on the host.
Independently selectable portable material variants,
topology-changing transfer and animated UVs remain open.

## First GLB implementation

This private component extends 06816fe. It is the first plain writable-mesh GLB
adapter, not full HDR/VRM/source-coordinate or frontend admission. Root owns the
separate Source28 renderer/import integration; this tree does not contain it.

The actual exporter writes standard PNG plus KHR_materials_emissive_strength
with independent channel factors. It records measured maximum absolute and
relative radiance error and the number of erased nonzero channel samples. The
fallback is intentionally an approximate static view: it cannot represent all
arbitrary HDR fields exactly or reproduce all original filter/animation cases.

Exact output is lossless ZIP OpenEXR FLOAT32 with resolved linear R/G/B plus
paint.legacy.R/G/B, paint.physical.R/G/B and paint.coverage. Legacy and physical
planes are premultiplied and remain separate. Native Scene CBOR independently
retains actual sparse layers, hidden/zero data, material/source owners and
first rig/shape history. OpenEXR is a named binary bufferView, never a core image
or a claim of ratified KHR_texture_basisu HDR support.

Root extras.domeSculptEmission version1 binds both native/EXR payloads and all
emitted core JSON/BIN resources with SHA256. Extraction checks container bounds,
marker/descriptor semantics, byte digests, JSON/BIN binding, EXR profile and
native definite-CBOR framing/depth/count before Scene reconstruction. This is
integrity binding, not an origin/authenticity signature. A changed standard
fallback refuses exact restoration. Missing metadata returns no native owner.

GltfExportOptions and VrmExportOptions append an optional immutable native-owner
option; the VRM option is reserved and explicitly refuses instead of silently ignoring an owner. The caller
must capture Scene::snapshot() before publishing the const owner. Whole-scene
mode requires every visible nonempty object; subset mode prunes unrelated
objects/groups while keeping actual authoring/source dependencies. The exact
extraction API returns the Scene directly, avoiding recursive GLB-as-new-source
wrapping. A second actual GLB cycle is byte-identical.

Limits keep the existing256MiB GLB ceiling; native CBOR and aggregate EXR are
limited to128MiB each, document JSON16MiB,256 images and65,536 bufferViews.
Dense export fields are forecast together before allocation and limited128MiB.
The codec separately bounds dimensions/pixels/encoded/working bytes and polls
cancellation through validation, header IO and compressed scanlines. Large
fields beyond this first bounded extraction tier require streaming; they are
not silently clipped or permanently declared unsupported.

The first real fixture includes physical and historical relative strokes,
fractional coverage, hidden zero-opacity paint, absolute background and shared
first rig/shape sources. Native restoration is exact; independent cgltf/libpng
checks the real standard fallback and POSITION accessor. Separate OpenEXRCore C
channel/chunk decoding proves each float bit, including .005 beside10000.
This is a separate decoder API, not a third-party viewer/GPU HDR proof.

Final affected Release9/9 (CTest9.20s; wrapper9.3271s) and whole-owned
ASan/UBSan/leaks9/9 (CTest83.55s; wrapper85.5250s) pass. The new export test
passes28 controls and codec14 controls. Actual3GLBs per profile are byte-exact
across profiles and pass the pinned Khronos validator with0errors/0warnings
and9infos per cohort, unfiltered. Independent fallback green is.00500198498
(error1.985e-6); the standard view still records3480 erased nonzero channels
and maximum absolute radiance error38.4169922 in this genuine mixed fixture.
The initial fresh Release and sanitizer app/core/support/render/dependent builds
pass (342.20s and556.52s). Private runtime/test continuations are incremental within those wholly
owned ABI-matched trees, not reused old cores. External OpenEXR/Imath system
libraries are not sanitizer-instrumented. Exact final results are in the receipt.
Preserved negatives include the legal NORMAL-first attribute-order test mistake
and the later incorrect white-green assumption after a real relative stroke
raised its channel maximum. The corrected consumer evaluates decoded PNG times
actual factors/strength against the .005 radiance oracle, without changing the
runtime to fit the fixture. Original logs/binaries/source are retained. A real mixed-scene old-core control
also found temporary HDR preparation erasing an ordinary visible emitter; the
fix restricts sanitization to actual HDR owners, and preserves that other
object's emission map byte-for-byte. An artifact-only matched-codec probe passes
all313 measured encoder cancellation boundaries in both Release and sanitizer.

Still required: native FileJob/menu admission and cancellation of this actual
new HDR path, typed MToon/VRM0/VRM1 emission export, alternate/retained/source28 UV
sampling, imported HDR source producers, independent GPU/external-viewer use,
larger-field streaming and exhaustive allocation-failure coverage. Existing
format/coordinate guards remain explicit. No physical device, system install,
user desktop or session action occurred.

Primary references: [OpenEXR Technical Introduction](https://openexr.com/en/latest/TechnicalIntroduction.html),
[KHR_materials_emissive_strength](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_materials_emissive_strength/README.md),
[KHR_texture_basisu](https://github.com/KhronosGroup/glTF/blob/main/extensions/2.0/Khronos/KHR_texture_basisu/README.md).
System OpenEXR3.2.4/Imath3.1.12 package/header/license evidence is in
third_party/sources.json. No dependency code is vendored.
