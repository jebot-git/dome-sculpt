# Native bone-length interchange — private checkpoint

The current private child of b953e26 implements a node-local editor property:

```json
{"extras":{"domeSculptBone":{"version":1,"length":0.137,"space":"localY"}}}
```

The scalar measures the endpoint `(0,length,0)` in the actual joint's local
frame. Native controls specify world metres and derive this scalar through the
complete object/group/skin-space world matrix. It is not a child/end-site, a VRM
humanoid limit or a collider radius. Ordinary consumers may ignore this editor
property; their mesh, pose and material interpretation is unchanged. Version 1
has exactly three fields and the existing finite native bounds `(1e-6,1000]`.
Unknown versions/fields/spaces or incompatible existing extras refuse explicitly.
Unrelated node extras and unchanged records remain exact. The supported VRM
coordinate bases preserve local Y; other bases require an explicit adapter.

Source adoption resolves actual original node IDs into each native skeleton's
own slot map, never names, presumed slot positions or a first skin's value.
Export requires compatible lengths for every shared actual source joint.
Original bytes, declared joint tables, own IBMs, child order, material/sampler
identities and typed VRM references remain immutable provenance. Native Save
already persists Bone.length, so this interchange adds no Scene/public layout.

Two production dependencies were found by actual fresh native reimport:

* The source VRM emitter retained the generated native graph and appended a
  second original graph. The new emitter builds one original graph and reuses
  actual mesh/skin payloads, keeping every real original node and ordered child.
  Split primitives remain separate mesh leaves. It does not raise hierarchy
  limits, fabricate skins or insert skeletal end-sites.
* Duplicate image records exhausted the existing native project limit after
  decoding. Export now shares only byte-identical encoded image payloads with
  identical image metadata. Texture/sampler/material identities and role color
  spaces remain separate. Every unique original image and original raw bytes
  remain; no image quality is reduced and no limit is raised.

The creator VRM coordinate conversion also omitted NativeAffineEdit. The narrow
fix conjugates copied object/group edits by inverseBasis*edit*basis alongside
TRS/rest/pose/IBMs. Exact identity-basis paths preserve existing matrices and
signed-zero bits. An independent actual CC0 all-corner old control failed by
1.30209m under reflected/sheared root placement; the corrected geometry check
measures below 0.4 micrometres at the unchanged 20 micrometre bound.

A reflected source-node ancestor prefix above hips needs exact native placement
representation after fresh import. The worker adapter authenticates the source
closure, exact rest matrices/mesh space/own IBM table and every actual nonzero
influence before deriving placement-only native editor roles. Raw declared
skin.joints, original node IDs and IBM accessors stay unchanged. Positive legacy
prefix behavior is unchanged. Negative editable humanoid/deforming joints or
weighted external prefixes remain refused. Export permits an unchanged exact
placement prefix through a pose; it does not admit arbitrary negative joint
pose scales. No new trust flag or approximate TRS decomposition is introduced.

Final current-source Release and asserted ASan/UBSan/leak scopes each pass
618 checks:496 both-base source/fresh-native roundtrip,42 actual worker/controller,
14 owning-placement policy,22 conflicting-slot guard,2 creator-affine geometry,
8 generic GLB restart and34 independent image checks. The eight-state source
roundtrip passes16 actual VRM exports, own IBM bits/raw source slot order,
REST/current pose, reflected/sheared roots, native Save/restart/re-edit/re-export
and Restore. Maximum independent geometry error is below 0.4 micrometres at the
unchanged 20 micrometre bound. Native projects remain 240/248 MiB under the existing
256 MiB limit. Release roundtrip takes 269.21 s; the fully instrumented stress case
takes 1188.33 s. Its new CTest timeout is 2400 s to bound the large persistence run
with measured sanitizer margin; cases and assertions are unchanged.

All eight independent software and strict PCI03 A770 image comparisons have
maxError 0 at tolerance 2, including the retained wrong-original-IBM and visible
rigid-part sensitivity controls. Hardware handle 23081 exits 0 in 45.21 s;
1156 source/helper/core/support/object/binary hashes match before and after.
Only the new CTest timeout was changed after that graphics source snapshot;
runtime sources, graphics fixture, objects and binary remain identical.
Khronos 2.0.0-dev.3.10 reports 28 actual outputs with 0 errors and 522 unfiltered
warnings; 26 VRM0 outputs pass the pinned official schema set with 0 errors.

Earlier unchanged module prerequisites remain separately scoped: 696 local-Y
source-unit/axis/own-skin-space endpoint checks and 222 metadata helper checks
each in Release/asserted sanitizers, including 91 actual persistent allocation
denial boundaries. The final seven changed implementation TUs are linked before
immutable fully instrumented own core/support and matching source-rest overrides;
link maps exclude inherited copies. This is not a new whole-branch build or a
new GUI/headset qualification. All 80 public headers match the frozen support ABI.
See validation/BONE-LENGTH-AUTHORING.json for exact manifests and negative evidence.

Length-only Restore and Undo/Cancel remain byte-exact. After actual pose transport,
Restore may retain a redundant near-identity double affine prefix while preserving
all source/intrinsic state and reconstructed world placement. The diagnostic
negative measured2 JSON representation operations and5.55e-17m corner error;
production code does not snap artist transforms or add a motion deadzone.

This does not complete VRM1 export, arbitrary spring topology/radius/shear
transport, conflicting per-part poses, general source-rig controls or FBX
skin/deformer interchange. Source24/25/26 semantic integration remains separate.
