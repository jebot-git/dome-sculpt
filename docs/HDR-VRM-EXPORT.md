# Native HDR emission in VRM and export workers

This private checkpoint extends `ee57697` (the isolated cache fix) and `ccf096e`
(the first GLB/OpenEXR package). It connects exact native HDR ownership to VRM0,
VRM1 and the existing GLB/Avatar file worker. It does not incorporate root's
Source28 import/export/renderer branch or claim full HDR authoring UI admission.

The implemented route uses actual writable UV0 mesh owners and an explicitly
accepted approximate static standard view. VRM export evaluates the authored
MToon emission image, sampler, transform and strength, including strength zero.
Relative paint is multiplied by authored strength; absolute physical paint and
an explicit physical background remain independent. VRM0 writes its real
`_EmissionMap`/`_EmissionColor`; VRM1 uses real core emission fields. Per-part
material variants include the actual emission field identity, so shared source
materials cannot broadcast one part's paint to another. Emission/UV expressions,
animated UVs, nonidentity VRM0 shared ST, retained paint domains and unsupported
alternate coordinates retain precise guards pending their actual adapters.

The exact package retains the original Scene, sparse contributions, original
source bytes/images, rig, morphs, metadata and before-state/history capsules.
Every selected object is authenticated against the immutable owner before
material compilation. Whole-scene and avatar-subset modes preserve their
separate contracts; an avatar package excludes the unrelated default SDF while
its following project Save retains the full Scene. Original source dependencies
inside the selected authoring closure remain intact. Extraction returns that
Scene directly, preventing recursive exported-container source growth.

Core unlit ignores emission. Its recorded standard-view error therefore counts
all ignored radiance, even when VRM0's typed MToon vector can carry HDR. No invalid
unlit/emissive-strength combination is invented. The ordinary PNG view is
measured approximation; named FLOAT32 ZIP EXR fields and native coefficients
retain exact HDR values. The first GLB package's within-channel quantization
negative remains relevant. This is not a claim that all HDR paint can be encoded
losslessly in PNG or that EXR is a ratified core glTF image format.

## Conditional bounded native payload

Package1 remains the raw definite-CBOR format for native payloads at most128MiB.
A valid full male CC0 avatar exceeds that stored-payload ceiling: its selected
native CBOR is138,713,063 bytes, dominated by genuine decoded image owners.
Package2 explicitly declares `scene-cbor-deflate+openexr-float32-zip` and its
expanded byte length. It stores29,632,484 native bytes in a57,628,568-byte VRM.
The female package remains package1 (133,881,799 native bytes).

Stored native bytes remain capped at128MiB, total GLB/VRM at256MiB and aggregate
EXR at128MiB. Expanded CBOR has a separate256MiB ceiling. This is not a low-RSS
claim: Scene DOMs, retained images and sanitizer overhead remain additional
allocations. Bounded CBOR output checks growth before allocating encoded bytes;
64KiB zlib-wrapped DEFLATE (RFC1950/RFC1951) encode/decode loops check output quotas, lengths, progress and
cancellation. Declared expanded limits are checked before decompression; exact
size, no trailing stream, CBOR framing and Scene validation remain mandatory.
The public read-limit structure appends `max_decoded_native_bytes`; coherent
header/library consumers are required. No Scene/archive version changes.

OpenEXR/Imath remain the qualified system libraries. Native compression directly
links the existing system zlib-compatible library (zlib-ng2.3.3, zlib1.3.1 API),
with package/header/library provenance in `third_party/sources.json`.

## Validation and limits

Fresh owned optimized and ASan/UBSan app/core/support/renderer builds were
followed by coherent changed-source/header/test rebuilds. Final selected
Release12/12 and whole-owned sanitizer12/12 pass; these are selected checks,
not an all-target regression, external-library instrumentation or GPU claim.
The final source/dependency/artifact/runner manifests each have1546 unchanged
inputs. All37 generated GLB/VRM/EXR/project files are byte-identical across
profiles. See `validation/HDR-VRM-EXPORT.json` for commands, hashes and stages.

The124 typed checks cover both real nineteen-part CC0 VRoid bases and independent
versioned V1 fixtures; independent cgltf reads world matrices, ordered geometry,
normals, indices, skin IBMs/joints and nonzero morph weights. Metadata, thumbnail
payload/sampler meaning, source/image aliases and repeated native cycles remain
exact. A separate OpenEXRCore C channel/chunk consumer reads every actual embedded
VRM FLOAT32 field. Libpng plus typed material factors checks standard radiance;
a half-tile KHR transform and strengths0/5 have independent analytic oracles.
Actual chunk cancellation, rebound expansion/truncation/downgrade negatives and
the previous reader's package1 acceptance/package2 refusal are preserved.

The20 worker checks use actual GLB and VRM0/VRM1 Avatar FileJobs, controller
shared Cancel, queued/running atomic-write barriers, following ordered Save,
retry, native restart and ordinary Undo/Redo. No new physical headset, GUI,
external viewer or device execution is claimed here. Existing worker semantics
and accepted-export drain are retained.

Pinned Khronos2.0.0-dev.3.10 validates23 actual files with0errors,
183unfilteredwarnings and652infos (19exports plus4synthetic source inputs).
Official pinned VRM schemas validate19VRMs/35instances with0errors. Both actual
CC0 baselines remain licensed original creation bases; generated V1 inputs are
test fixtures. Recompiling the exact old real-avatar refusal probe now succeeds;
its ordinary LDR export remains byte-exact at25,140,720 bytes.

Preserved failures include the first numeric thumbnail-index assertion, real
male raw-CBOR limit, omitted test-target setup, default-SDF/null-paint fixture
selection, incorrect full-Scene/subset assertion, a fixture return-type typo,
and inherited empty leaf arrays in generated source inputs. Source leaf spelling
was corrected without changing nodes or ordered nonempty children. Their old
negative logs/cohorts remain separate. Full Source28/typed alternate or retained
sampling, dynamic expressions, broad HDR controls and further production
qualification remain required. The full goal stays active.
