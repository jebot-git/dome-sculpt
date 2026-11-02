# Generic native VRM authoring import

This checkpoint extends the existing file worker and shared FLTK/controller
import workflow. It does not replace the avatar creator or require a bundled
model hash. It does not complete the full VRM objective in REQUIREMENTS.md.

VRM0 sources use the exact `VRM` 0.0 metadata and original material properties.
`VRM/MToon` is adopted as a typed MToon0 binding, with independent PBR and MToon
normal-frame authority and original sampled images/samplers. `VRM_USE_GI` keeps
its explicit glTF fallback. Other legacy shader names refuse with an adapter
diagnostic. VRM1 sources use exact `VRMC_vrm` 1.0 metadata and typed
`VRMC_materials_mtoon` 1.0 materials. VRM1 metadata remains in the immutable raw
asset; the VRM0-only `CreatorAppearance::vrm_json` field is not repurposed.

The authoring adapter keeps every selected source-node instance and local chain,
original mesh/accessor/material/node IDs, authored inverse binds, current source
pose, morph channels, raw expressions and physics metadata. Complete humanoid
and skin ancestor closure is available on each skinned part. Used native mesh
vertices are compacted in ordered-corner order; the complete original accessor
arrays remain in the shared immutable source, and every editable corner retains
its exact original accessor index. The actual female/male models contain 19
parts each, with 9,851/9,878 used native vertices respectively.

The approved additive `ImportLimits::allow_vrm_metadata` flag defaults false.
Only preserving import can use it. Required `VRMC_vrm` and
`VRMC_materials_mtoon` tags are admitted after authenticating the captured
version and typed material coverage. Unknown required tags still refuse. The
retained-source archive reparser follows the same authenticated path, so native
Save/reopen preserves these assets. No Scene or material archive version changes
are needed for this adapter.

Linear vowel bindings use exact mesh/target identities for VRM0 and exact
node/target identities for VRM1. Binary bindings remain raw rather than being
silently reinterpreted as linear. All raw expression definitions remain in the
original source. Full expression evaluation, binary and material animation,
first-person/look-at behavior, spring-bone simulation, constraints and associated
authoring remain required. The VRM1 reference checker validates the implemented
meta/humanoid, first-person and expression-reference structures; it is not a
complete runtime implementation of every official schema feature.

Remaining import guards include unsupported legacy shaders, the native
humanoid positive-ancestor-scale policy, general KHR texture transforms and
emissive-strength adapters, and source UV/atlas cases awaiting the separately
qualified UV1/explicit-atlas integration. Typed MToon sampled roles must count
when deciding whether a mesh is genuinely untextured. Do not apply a PBR-only
no-map test to a VRM part.

The generic imported rig uses authored/implicit inverse-bind and original node
placement authority. The current source-preserving VRM writer still explicitly
refuses that original-space state. Full VRM0 re-export and VRM1 export, shared
source-joint graph transport across distinct skins, unskinned parts and all
expression/physics remaps remain required. GLB export is not a substitute for
those version-specific requirements.

Final validation is recorded in `validation/GENERIC-VRM-IMPORT.json`: four
source-exact Release suites pass in 19.58 seconds; the same four suites pass
with the entire own C/C++ core/render/support instrumented for ASan, UBSan and
leaks in 105.53 seconds. The actual Arc A770 run passes 31 checks with six
image errors all zero and maximum posed-position error 3.87506e-7 metres. The
private Xvfb/DBus FLTK chooser/options/Save/history workflow passes 11 checks.
Controller checks feed synthetic spatial frames through the production input
worker; no physical controller/headset or XR loader test is claimed. The native suite
imports both actual CC0 avatars, a modified external source and an independently
authored VRM1 envelope over licensed CC0 geometry. It checks native restart,
malformed references, real file-descriptor cancellation/retry, one Undo/Redo,
ordered Save and actual joystick Apply. The graphics fixture deliberately
creates external source variants with the independently implemented main/shade
shader subset. Original geometry/images/skins/morphs remain unchanged; these
images do not qualify every shading/outline property of the unmodified models.
Every native corner is compared against separately parsed cgltf skin/IBM/morph
world positions. A PBR substitution negative prevents a false fallback pass.

The retained initial typed MToon descriptor and role image/sampler identities
are explicit in the first UV origin. A direct negative on the earlier adapter
proved that merely retaining raw appearance was insufficient for that existing
provenance contract. A separate malformed-source fixture proves that typed
MToon sampling still requires its original UV channel even when PBR maps are
absent.

Both captured sources pass 17 official VRM0/VRM1/MToon schema invocations with
zero errors. Full, unfiltered Khronos validation reports zero errors and 2,476
warnings for the independently authored VRM1 source. The external VRM0 source
retains one inherited `SKIN_SKELETON_INVALID` error at `/skins/0/skeleton`, plus
2,460 warnings: its exact advisory hint is the same as the original bundled
female source. It is preserved, not repaired or serialized as a new authoring
choice. Warnings include original zero-weight joint entries, generated tangent
space, overlapping unlit/MToon declarations and unsupported extension notices.
Neither full report is truncated. This is raw-source validation, not a claim
that a new VRM export has passed; original-space VRM export remains guarded.

The first corner oracle correctly exposed an independent test omission: both
actual models omit node and mesh default morph-weight arrays. Assigning only
existing arrays left the independent model at zero while the native model used
0.35. The preserved negative reports 0.00111418 metres. The corrected independent
consumer owns explicit current node weights; neither the 2e-6 metre position
bound nor the pixel tolerance of two was changed. Initial renderer-name and
GUI group-count expectation mistakes are retained in the ignored proof logs.

Primary references are pinned to vrm-c/vrm-specification
`94e82dd346fa6cf0337c4421728640e5252dd38e`:

- [VRM1 model-space and metadata specification](https://github.com/vrm-c/vrm-specification/blob/94e82dd346fa6cf0337c4421728640e5252dd38e/specification/VRMC_vrm-1.0/README.md).
- [MToon1 material specification](https://github.com/vrm-c/vrm-specification/blob/94e82dd346fa6cf0337c4421728640e5252dd38e/specification/VRMC_materials_mtoon-1.0/README.md).
- [VRM1 metadata schema](https://github.com/vrm-c/vrm-specification/blob/94e82dd346fa6cf0337c4421728640e5252dd38e/specification/VRMC_vrm-1.0/schema/VRMC_vrm.meta.schema.json).
- [VRM1 expression schema](https://github.com/vrm-c/vrm-specification/blob/94e82dd346fa6cf0337c4421728640e5252dd38e/specification/VRMC_vrm-1.0/schema/VRMC_vrm.expressions.expression.schema.json).
