# Private native VRM1 source/export milestone

This branch implements a first VRM1.0 source-node export route. It is based on
`eec7753284f99b65fae94a22795917efaf63f6d5`, is not yet a production-main
integration, and does not complete full VRM support or the remaining goal.
The machine receipt is [VRM1-SOURCE-EXPORT.json](validation/VRM1-SOURCE-EXPORT.json).

## Actual implementation

The writer dispatches from validated **actual** `VRMC_vrm.specVersion: "1.0"`
in immutable captured JSON. It emits VRM1, without relabeling VRM0 or inventing
skins. It retains the original source-node graph, hierarchy and child order,
authored per-skin inverse binds, genuine rigid descendants, current supported
skin/morph pose, encoded PNG/JPEG resources and raw native-project sources.
Typed remapping covers humanoid object/thumb roles, first-person mesh-node
annotations, look-at, expression morph/material/UV bindings, thumbnail images,
spring joint/collider/group/center references and roll/aim/rotation constraints.
Source morph identities and material forks remain independently witnessed.
Opaque mesh and primitive extras retain JSON values; current target names keep
their current morph identities. Unrepresented extension-reference semantics and
multiple authored scenes refuse before replacing an output.

The existing native exporter/file worker and FLTK universal-menu commands now
recognize V1 sources. `ImportedAsset::avatar_version` is an advisory derived
parser result, recomputed on replay; it is not serialized authority and never
inferred from `extensionsUsed`. The writer independently authenticates the raw
version/owner. A glTF that only declares an unused VRM tag stays non-avatar.
Malformed/unsupported actual metadata refuses atomically.

Preserving import appends exact parsed `KHR_texture_transform` data for all five
core PBR roles, retaining authored base texcoord separately from an optional
extension override, and retains source emissive strength. Existing owned source
maps carry effective affine sampling. Normal/tangent and HDR cases without an
owning authoring adapter still refuse. The previous reader demonstrably refuses
the new raw transform source on native restart; no unversioned serialized trust
bit or outer Scene schema was introduced. Public ImportedMaterial/ImportedAsset
layouts changed: every consuming core/render/UI/XR archive must be rebuilt.

For large VRM hierarchies, native placement groups can use the actual mesh-node
ancestor closure under the existing128-group cap. The complete raw graph and
all native skin joints remain retained, including joints/constraint sources
outside that placement closure. No node or quota is dropped/raised.

## Evidence

| Exact scope | Result |
| --- | --- |
| Final affected optimized Release core/support/render/app/helper builds | Pass; this is not an all-target/all-backend build |
| Final selected Release CTest |4/4,55.65s: V1 source/export and worker, existing generic VRM import and VRM0 source export |
| Final whole-owned C/C++ ASan+UBSan+leak native CTest |3/3,84.71s: V1 source/export and worker plus generic VRM import; O1/assertions enabled, no suppressions |
| V1 native rich fixture |2,606 actual PASS assertions in each profile; two genuinely weighted skins with distinct exact IBMs,130 extra original joints, a real rigid accessory, metadata/refusal/history/restart oracles |
| Actual file-worker/controller V1 cancellation |14 checks in each profile; real running fsync barrier, queued export, CancelWork, following Save, retry and one Undo/Redo |
| Official unmodified UV-animation sample, independent GLSL |16 software and16 actual A770 checks; original/current full-image max1 at times0/.37/1.1, unchanged tolerance2; actual public PBR and time sensitivity negatives |
| Retained-source replay consumer |16 Release +16 whole-owned SAN checks; genuine malformed/future raw models reject through actual CBOR/Scene decoding, unused-tag and fabricated-advisory cases remain None |
| Export → fresh native reimport → export/restart |3 Release +3 whole-owned SAN cases: multi-skin/rigid, current kinematic pose and official static source; independent corners/IBMs and exact native restart,1,486 inputs unchanged |
| Actual native FLTK universal menus |7 normal +7 compact; chooser ownership/Close, real durable-temp Cancel, following Save+Redo, exact project and parsed V1 retry |
| Pinned unfiltered Khronos validation |11 actual emitted files,0 errors/73 warnings, maxIssues0 |
| Pinned official VRM1/MToon/spring/constraint schemas |60 schema instances over those11 outputs,0 errors |

The separate replay consumer/registration was appended after the frozen native,
image and GUI runs; all runtime/header/test sources from those runs remain exact.
Replay runs reuse the same fully owned support/core with a newly compiled
consumer, not an old-core/new-header mix.

Final native proof inputs:1,490 exact before/after source/header/library/binary
hashes. Final software/GUI proof:1,478 exact inputs. Final A770 proof:1,474 exact
inputs, explicit `DRI_PRIME=pci-0000_03_00_0`, `GL_VENDOR Intel`, renderer
`Mesa Intel(R) Arc(tm) A770 Graphics (DG2)`. GPU lease was released. The image
oracle is a genuinely independent static-only consumer for the official sample;
it is not independent posed-skin N/T image coverage or physics evaluation.
Whole-owned sanitizer native scopes do not claim instrumented external GL/driver
cleanup. UI uses current coherent Release app/core/helpers, canonical bitmap/
toolkit prefixes, signed private Xvfb and private D-Bus; no installed desktop,
physical headset, system packages or sessions were touched.

The original exact-eec small V1 import reaches the old VRM0-only writer refusal.
Original real-source transform/group-cap failures, the inherited official
isBinary empty `meta.references` schema failure, preliminary implementation
failures, and a test-helper incomplete-type compile negative remain in ignored
artifacts. Their results are not relabeled by later passes. Earlier partial
software/Arc/SAN/UI cohorts are separate from the final no-loss refinement.

## Licensed official input and pinned references

Normative VRM source pin is unchanged:
[vrm-specification94e82dd](https://github.com/vrm-c/vrm-specification/tree/94e82dd346fa6cf0337c4421728640e5252dd38e).
The implementation is native project code; no repository code-license grant is
inferred, no upstream renderer was copied, and validation schemas remain private.
Private schemas also use pinned Khronos common schemas
`3cd6f59b277ed7f6540dbabdacc7841a5fdc0ded`.

The positive external input is the unmodified official
[UV Animation Test](https://github.com/vrm-c/vrm-specification/tree/94e82dd346fa6cf0337c4421728640e5252dd38e/samples/VRMC_materials_mtoon_UV_Animation_Test),
(c)2022 pixiv Inc., [VRM Public License1.0](https://vrm.dev/licenses/1.0/).
Its original metadata permits modification/redistribution without credit; the
source and README hashes/permissions are recorded in the receipt. It is used
only for private validation, not added as a creation base. Creation remains the
verified CC0 VRoid female/male bases. Seed-san (VirtualCast, credit required),
Twist Sample and isBinary sample (pixiv) are retained private counterfactuals,
with their actual raw author/license metadata and source hashes.

## Required next work and explicit guards

- Actual dynamic node constraints/springs, full source-REST changes and general
  scaled/reflected/sheared physics/collider representations still need their
  real version-specific adapters. No collider radius or solver behavior is
  inferred from matrix equality. Current changed poses with constraints and
  unsupported physics REST/placement changes refuse explicitly.
- Painted V1 MToon common colour/emission compilation, prepared colour/normal
  planes, UV-expression transport through prepared/frozen sampling, and broader
  edited expression/material semantics remain guarded. Source-only export is
  the qualified first route, not a completed painted VRM1 writer.
- Native HDR emission authoring still refuses (actual Seed-san); exact raw
  emissive fields remain preserved by the parser. Native editable initial
  out-of-unit UV sampling still fails before an authorized writable basis
  (actual Twist sample). Existing Paint24 retained-domain factories cannot
  manufacture this missing initial source authority.
- Generated normal tangents under a nonidentity sampling transform and broader
  transformed-normal frame/export cases require their owning frame adapter.
  The role DTO addition does not prove all five role/frame combinations.
- Multiple authored scenes, unsupported actual source extensions/custom
  attributes, tracks/cameras/lights and unrepresented opaque reference semantics
  must be transported or precisely refused; they are not silently stripped.
- The official isBinary sample contains an inherited schema-invalid empty
  optional references list. Native source can retain it, but a conforming new
  export refuses rather than erasing authored metadata to pass validation.
- Independent posed skin/morph normal/tangent pixels, broader fitted V1 cases,
  actual synthetic-loader V1 export and the rest of full VRM authoring remain
  required. This milestone does not authorize a physical test.

## Reproduction

Use the canonical bitmap/toolkit environment in TOOLKIT.md. Native:

```sh
cmake --build build-release --parallel 2 --target dome-sculpt dome-sculpt-vrm1-source-export-test dome-sculpt-vrm1-export-worker-test dome-sculpt-vrm1-source-export-ui-test dome-sculpt-vrm1-source-render-test dome-sculpt-export-cancel-gate dome-sculpt-vrm1-source-replay-test dome-sculpt-vrm-import-test dome-sculpt-vrm-source-export-test
ctest --test-dir build-release -R '^(vrm1-source-export|vrm1-export-worker|vrm1-source-replay|generic-vrm-import|vrm-source-export)$' --output-on-failure
```

The image target takes the private pinned official `.vrm`, an output directory,
and optional `--hardware` (requires a coordinated device lease). The GUI harness
is `tests/vrm1-source-export-menu.py`; set `DOME_SCULPT_TEST_BUILD`, the private
Xvfb PATH and `DOME_VRM_UI_SOURCE` to the generated genuine V1 fixture, plus
`DOME_SCULPT_RESOLUTION_COMPACT=1` for compact mode. Optional Python dependencies
are test orchestration only. Machine receipt records exact commands/cohorts.
