# Actual CC0 avatars after prepared-normal UV editing

This test-only checkpoint extends frozen `b0cdeb5`; product sources, public
layouts, Scene19 and normal UV receipt version1 are unchanged. The complete
female and male CC0 VRoid models retain all nineteen parts. Their selected face
parts have1064/1072 triangles and56/54 original morph targets, respectively.
The fixtures use256-pixel paint surfaces and actual original normal fields.

`post-uv-avatar` drives two opposite one-texel UV translations through real
Document workers, creates and replaces a key, then edits that key through both
the native desktop gesture worker and joystick/ray/trigger input. It verifies
native Undo/Redo and gesture cancellation, controller focus cancellation,
Save/new-Document/reopen and exact source restoration. Source paint, raw asset
identity and the complete before-UV/key graph survive these operations.

The female fractional0.95 UV scale is a genuine negative. Its geometry draft
validates, but applying the intrinsic field exceeds the existing0.02-radian
sample-quality bound. Rejection retains the entire original Scene encoding;
closing the failed draft creates no global history entry. This is not a claim
that arbitrary UV scales are supported. Integer translations are the qualified
representable subset.

`post-uv-avatar-render` retains original normal images, samplers, factors and
artist direction arrays. Unrelated base/shade/color and illumination roles are
controlled to isolate normal and attribute transport in the independent
consumer. No constant normal texture is substituted. A deliberately visible
face deformation avoids testing an occluded back-head vertex. Both genders
export all nineteen parts, with three mixed-joint poses and morph weights
0/0.35/1. The independent parsed VRM0 consumer derives its own matrix inverses,
applies POSITION/NORMAL/TANGENT morphs and skinning, and evaluates the declared
legacy MToon policy. It shares no production frame/morph/skinning helper.

The qualification receipt records exact source, archive, fixture and emitted
file hashes. Fixture TUs link against the immutable, already coherent `b0cdeb5`
Release and whole-core/support/renderer O1 sanitizer builds. This is a new
fixture qualification using those frozen product archives, not a new all-target
product rebuild or a new GUI/headset test.

Both native runs pass17 counted assertions plus their direct equality checks.
The independent consumer passes87 assertions in Release software, actual
PCI03 Arc A770 and whole-core/support/renderer ASan/UBSan/leak runs. All18
graphical comparisons have maximum byte error0. Software and hardware emit
byte-identical files. Khronos validation reports0 errors and360 unfiltered
warnings:342 retained-parent skinned-mesh warnings and18 legacy VRM extension
name warnings. All18 files pass the15 hash-verified official VRM0 schemas.
Exact hashes and scope are in `validation/POST-UV-AVATAR.json`.

## Remaining work

The initial affine-padding prototype was measured separately and parked rather
than admitted: the worst fractional-scale constraint uses four covered texels,
so padding changes alone do not fix the actual failure. No quality bound was
widened. A fixed-resolution bilinear texture cannot represent every affine
reparameterization of every sharp source field exactly.

Scene21/material archive7/normal-preparation entry3 are reserved for genuine
retained intrinsic sampling-domain authority. Its design must retain exact
intrinsic and existing resolved-detail fields and their sampling domains.
Simply blending separately sampled intrinsic/detail normals after filtering
would change the current normalize/quantize-before-filter interpretation.
Format-specific normal/detail export therefore needs explicit compilation and
quality/resource limits; VRM0's common UV0/MainTex-ST convention cannot be
treated as arbitrary independent normal/paint domains.

Independent edits to duplicated UV vertices still require a real native
corner-domain key conversion. The current authoring guard continues to refuse
contradictory source-vertex edits instead of averaging them. Neither the
sampling-domain work nor this corner conversion is completed by this checkpoint.
