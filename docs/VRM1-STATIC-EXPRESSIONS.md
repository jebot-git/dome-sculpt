# Private static VRM1 expression authoring

This a3f4268 descendant adds a pure VRM1.0 expression evaluator and a detached
native static authoring operation. It does not replace interactive expression
preview or expression-preserving painted export. Both remain required.

`evaluate_vrm1_expressions` validates actual retained-version metadata and returns
original node/target and material identities. Finite input weights clamp to[0,1];
binary threshold, binary overrides, additive procedural attenuation and invalid
same-category overrides follow the pinned VRM specification. Preset and custom
namespaces stay distinct. Custom expressions can suppress preset procedural
categories; they are not implicitly categorized as procedural themselves.

`bake_vrm1_expression_pose` authenticates the actual source model, live instance
root, mesh node and original material. It edits only that source instance,
including its hidden parts, refuses locked linked parts, and leaves unrelated
instances exact. Source morphs start at zero, then accumulate original-node binds;
native-only targets retain their weight. A bound target missing from any live
primitive refuses instead of silently losing its contribution. Each call takes
the supplied Scene as its initial material state. Interactive sampling must keep
an immutable initial Scene, rather than feed prior samples back into evaluation.

All six typed material color roles use target-minus-initial accumulation. RGB
roles ignore target W. UV-accessed roles use their own initial scale/offset;
rotation, coordinate selection, image and sampler stay authored. MatCap is
view-dependent and excluded. The private Source4 parameter factory authenticates
and independently replays original sampling transport into the same writable
grid. It shares actual sparse legacy/physical paint tiles, preserves masks,
layer metadata, first source/rig/shape owners and current normal-frame owners,
and gives changed source views new visual cache identities. It does not flatten
paint, move its UVs, fabricate image roles or alter raw original bytes.

Use the operation through existing `Document::edit_scene` for captured native
worker admission, cancellation, one Undo and ordered Save. The generic worker's
existing exclusive-isolation publication check remains in force. No frontend,
interactive weight state, existing C++ data layout or archive version is added.
Existing Paint4 matrices and material fields persist the actual static result.

The schema makes UV binding scale/offset optional, defaulting to[1,1]/[0,0]. Both
retained-source validation paths now accept genuine omissions without rewriting
raw metadata. The exact old a3 core refuses `key 'scale' not found`; the same
probe with the new core passes. This corrects existing VRM1.0 semantics.

Release and sanitizer each pass81 native/data checks,57 persistent allocation
failure boundaries and all18 real parameter-factory cancellation polls. Tests
include actual paint, all six colors, independent affine-column arithmetic,
separate source instances, missing source morphs, forged root/locked-part
refusals, physical paint retention, native restart, one Undo/Redo and immediate
queued Save. A late gate follows actual detached expression computation: Cancel
refuses its dependent Save, preserves prior file/Scene/history, and a subsequent
Save succeeds. All six Release/sanitizer fixture/native-file pairs are exact.

Five affected legacy consumers pass each profile: VRM1 source export, real export
worker and source replay, Source4 owning data and Source4 Scene persistence. Final
relinks are byte-identical to the preceding five-consumer cohort in each profile.
The pinned, unmodified three-vrm texture/material binding implementations pass44
independent numerical checks. They are optional test consumers, not runtime
packages. That consumer omits shadingShiftTexture; native follows the normative
all-UV-role rule and tests it separately. Two original input fixtures pass12
pinned official extension-schema instances. These are input controls, not new
portable export proof. Configuration registers615 tests; no full615 run occurred.

Qualification uses five complete changed/new runtime TUs and whole consumer TUs
over immutable, matching a3 owned core/render/support archives. The later
missing-target guard rebuilds its complete Scene TU. All consumed support in the
sanitizer profile was built with assertions, ASan/UBSan and leak detection. This
is a matched component proof, not a fresh whole-app/vendor/device/GUI claim.
The receipt records each exact build continuation and retained failure.

Remaining adapters are explicit: interactive native/desktop/XR expression
preview; general normal-texture tangent-frame transport; prepared key/normal/
color before-state reauthoring; plain-PBR static factor authoring; absolute
background emission expressions; and representable expression-preserving
painted VRM export. The painted dynamic-export guard is tested and unchanged.
The current static Source4 rebind uses its existing256MiB caller working bound
and existing512MiB canonical maximum; no limit is raised. No GPU, headset,
system installation or active-session action occurred. Every other unfinished
original/additional goal requirement remains active.

Primary references: [VRM1 expressions](https://github.com/vrm-c/vrm-specification/blob/94e82dd346fa6cf0337c4421728640e5252dd38e/specification/VRMC_vrm-1.0/expressions.md),
[UV binding schema](https://github.com/vrm-c/vrm-specification/blob/94e82dd346fa6cf0337c4421728640e5252dd38e/specification/VRMC_vrm-1.0/schema/VRMC_vrm.expressions.expression.textureTransformBind.schema.json),
and [three-vrm expression sources](https://github.com/pixiv/three-vrm/tree/3caf7a21ee68f0d5dfa363ab3d577450bbf60136/packages/three-vrm-core/src/expressions).
The ignored primary-source cohort includes exact hashes and the third-party
license. See validation/VRM1-STATIC-EXPRESSIONS.json for source/artifact ledgers.
