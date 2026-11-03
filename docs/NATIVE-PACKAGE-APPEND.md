# Native authoring package Import / append

Private checkpoint based on `5d155052201a4212e6362a35367dc4b450b7a94b`.
This adds native project append to the existing worker-backed Import mesh action.
It does not publish the branch or complete the wider production goal.

`prepare_authoring_import` probes a bounded binary GLB header/JSON before ordinary
source capture. A present `domeSculptAuthoring` or `domeSculptEmission` marker goes
through its existing strict extractor. Corrupt, unknown or competing authority
refuses; a JSON glTF wrapper carrying a reserved marker also refuses. A recovered
Scene is appended directly, never recaptured as another original raw GLB.

The new standalone `append_native_scene(destination, package, cancelled)` returns
a detached Scene and all appended live object IDs, including hidden and empty
objects. Both inputs remain unchanged on failure. Mutable volumes/paint receive
normal Scene snapshots; immutable raw models, images, material/normal/paint,
skin/morph and before-state leaves retain their owners and internal aliases.
No existing data layout or persisted format changes. Scene grants this one
transaction friend access to reserve checked counters without fake live objects.

## Identity and placement

Live object/group IDs are rebased through the destination allocators. Deleted
retopology references reserve nonlive IDs rather than pointing into the existing
destination. IDs above 2^53 remain integer values throughout.

Linked original rigs use exact immutable model ownership plus source instance.
The transaction namespaces current and retained placement nodes/capsules through
the complete polygon/shape/rig origin DAG. Memoization preserves shared first
sources. A genuine source root maps to its actual new live/native path root,
including consistent capsule IDs/parents. The proof uses exact model/appearance/
source-node owners; model-only paths additionally verify every original parent
against the immutable parsed node graph. Numeric coincidence, names and equal
skeleton values do not establish membership. Historical-only paths reserve their
own IDs. Original source node/skin/material indices, raw bytes, matrices and
`PolygonOrigin.source_object` provenance are unchanged.

## Bounds and native worker behavior

Programmatic calls preserve requested file/resource caps: native containers use
`min(max_file_bytes, max_resource_bytes, 256 MiB)`. The default file cap remains
128 MiB. The actual Document import worker explicitly requests256 MiB. Ordinary
raw capture still clamps to64 MiB after native detection. Native stored/expanded
limits additionally honor lower resource budgets. Canonical Scene/archive quotas
remain independent; editable mesh limits are checked after extraction, before
append publication (not a claim of custom vertex-cap preflight before decoding).

File capture uses a regular, confined descriptor, chunk cancellation and matching
before/after file identity/size/timestamps. Descriptor guards are noncopyable.
NUL paths and escaping symlinks refuse. Units/axes/material/normal/hierarchy
reinterpretation choices refuse native projects. Atlas/resolution choices do not
resample stored paint; the result explains that stored choices were retained.

Deep validation and a counting CBOR stream reject combined projects exceeding the
existing256 MiB Save cap before publication. The sink allocates no encoded CBOR
buffer. Scene.encode still performs its normal bounded JSON/archive staging;
this is not a zero-allocation operation or a claim of bounded total RSS by256 MiB.
The existing128-group/256-object ceilings remain unchanged.

## Qualification

Fresh owned CPU Release and C/C++ ASan/UBSan/leaks core/support/render/native-app
builds were followed by coherent complete-TU/header continuations. External
canonical SDK/OpenEXR/zlib libraries are not claimed instrumented. Six affected
native consumer commands pass in each profile, with2,091 input hashes unchanged:

- append56: exact history/source/Source4 ownership, wide IDs, tombstones, model-only
  paths, both markers, corrupt inputs, resource caps,128 actual persistent ordinary
  allocation-denial prefixes, early/mid/late detached cancellation and actual
  read-gated worker Cancel/retry/queued Save/one Undo;
- real102: both authentic CC0 VRoid bases, whole/subset/repeated import, source/
  REST/rig/morph/first-source preservation, actual worker256 MiB request and Save;
  actual two-full-base save overflow refuses without Scene/history publication;
- V1 source12,108: two skins and genuine rigid child, identical-model-owner
  isolation, pose/REST/Restore, original and fitted bind authority, independent
  cgltf world corners (maximum6.664e-8 m), fresh native reimport and restart;
- existing authoring28, import37 and scene-file16.

Afterward only generated empty leaf `children` properties were omitted in the new
V1 test fixture; no node/edge/reference was removed. That final whole consumer TU
was rebuilt and passed12,108 again in both profiles with2,086 hashes unchanged.
Other binaries/archives retain their qualified bytes. Thirty-nine declared
GLB/VRM/native-save artifacts are byte-identical across profiles. Pinned Khronos
2.0.0-dev.3.10 (`maxIssues:0`, unfiltered) reports26 files /0 errors /126 warnings /
185 infos, including three deliberate private-auth negative containers that are
still standard-glTF data. Twelve V1 files pass64 pinned official1.0 schema
instances with0 errors. Those validators do not authenticate private payloads.

Preserved negatives include the first incorrect instance allocator (original V1
export passes; appended source ancestry refuses), accepted42-object two-base Scene
that cannot Save, original5d stale normalized-UV1 assertion, unreachable fixed128
allocation-count assumption, fitted-IBM original-bits assertion, generated-input
empty-children errors and harness setup/exit-code errors. See the JSON receipt.

V1 posed constraints and spring/constraint REST export still require their real
runtime adapters. Tests retain those precise refusals and native data; positive
posed/fitted interchange uses a distinct explicitly unconstrained V1 fixture.
No new GUI/Wayland/controller/device/physical-HMD or broad regression claim is
made. The Import action uses the existing shared Document worker; frontend union
qualification remains with the parent. No GPU, installation or session action
occurred. Future Scene29/key integration requires the parent's coherent semantic
union; this branch does not numerically admit unknown native feature versions.
