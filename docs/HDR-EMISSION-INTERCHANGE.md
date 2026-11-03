# Native HDR emission interchange — first GLB checkpoint

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
