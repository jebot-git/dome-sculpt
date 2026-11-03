# Exact native authoring container

This private component extends466d84b/41b5864 with an additive Scene-owner
`export_glb_authoring` API and bounded `extract_native_authoring` API. It does
not append existing GLB/VRM writer options, require an imported-original graph,
or admit root's separate Source4 portable compiler. Existing ordinary writers
retain their guards. The full goal remains active.

`NativeAuthoringExport` owns a detached const Scene, explicit whole-scene or
exported-object scope, exact IDs and portable-view intent. The private capture
has a private constructor: its factory snapshots/validates the Scene and derives
original ObjectSurfaces internally. No arbitrary modified surface or equivalent
flag can manufacture this relationship. A later real compiler can consume those
sealed originals before changing UVs/materials; final normal-frame splitting is
separate from the original capture.

Whole-scene native data retains hidden/empty objects and required history/source
owners while only visible nonempty geometry is emitted. Explicit subsets require
unique visible nonempty IDs and prune outside live objects/groups, preserving
actual immutable source/before-state dependencies. These meanings are tested
independently; an original raw model may retain other original nodes as required
source provenance without importing outside live Scene objects.

The new `extras.domeSculptAuthoring` version1 contains native CBOR, scope, emitted
object remaps and complete core/BIN/resource digests. It has no fake EXR/HDR
resource table. Canonical decimal strings preserve general u64 native IDs across
JSON; remapped node/mesh/material references are checked. The portable view is
explicitly `existing-gltf-core`, `lossless:false`; this is intent and honest
scope, not a claimed quality measurement or compiler-equivalence authorization.
Root owns Source4 compiler/fallback receipts and actual file-worker/import/UI
integration. Existing Source4 and HDR compiler guards remain required until that
real composition.

Small native payloads use `scene-cbor`; large payloads use explicit
`scene-cbor-deflate` with the same proven bounded CBOR/zlib primitives as HDR.
Stored native data stays at128MiB, expanded CBOR256MiB, complete container256MiB
and JSON16MiB. JSON depth/count, descriptor/table/object counts, exact lengths,
hashes, definite-CBOR framing and actual Scene validation remain mandatory.
This is integrity binding, not an origin signature or low-RSS claim. No Scene
version maximum is invented; actual Scene decoding enforces its capabilities.
Absent markers return no native owner; malformed/present or competing authority
refuses. Existing `options.emission` cannot compete with the new owner. Valid
HDR1/2 serialized paths remain unchanged; both updated readers reject competing
markers before selecting an owner.

Extraction restores the Scene directly rather than treating the container as
new original source. Its exact layers, material/morph/rig/source records and
aliases survive save/reopen and byte-identical export cycles. Automatic native
Open/append/chooser integration is root's separate task; do not substitute
`capture_gltf_source` for native restoration or overwrite GLB/VRM with CBOR Save.

## Qualified component scope

Matched-component Release4/4 and whole-owned sanitizer4/4 pass78 checks each:
28 new authoring plus4 budget/14 codec/32 HDR regressions. One complete existing
runtime TU is replaced and one complete new TU is added over eight verified
immutable466d/41b own-support archives; four complete consumer TUs are compiled
with matching headers/profile flags. Old members are removed and link maps are
retained. This is not a fresh whole-app/core, vendor, device, GUI or full-suite
qualification; system OpenEXR/zlib libraries remain uninstrumented.

The new checks include actual imported two-instance source images/morphs and
real sparse colour paint; hidden/empty whole-owner data, subset/no-outside-live
objects, original source/image aliases, native project reopen, stable second
cycle, u64 IDs above2^53, digest/tamper/version/remap/limit negatives and actual
durable-write cancellation with original bytes/owner unchanged. Thirteen fixture
files are byte-identical between profiles; the existing three HDR GLBs remain
byte-identical to466d. Five small generated/source GLBs have0 Khronos errors,
0warnings and19infos, unfiltered.

An additional complete consumer proves the actual CC0 male nineteen-part base
with real sparse colour paint through the compressed generic envelope. Both
profiles restore exact native/source/rig/morph data and produce identical second
cycles; a smaller explicit expanded cap refuses. Native expanded size is
138,426,115 bytes, whole GLB43,109,764 bytes, with no HDR marker/table. The registered
`native-authoring-real.cpp` is byte-identical to that independently compiled
consumer. It is a matching-archive consumer proof, not an additional fresh core
build. Its two outputs also pass the pinned validator. See the machine receipt
for exact counts, source hashes and artifact scope.
