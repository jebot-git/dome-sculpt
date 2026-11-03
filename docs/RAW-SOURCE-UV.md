# Raw source-coordinate viewing and explicit paint preparation

This private continuation of412a565 admits default original-coordinate import
while preserving the exact source arrays, sampler IDs, frames, pose and morphs.
It provides source-preserving viewing/export and an explicit preparation route.
Raw alternate-coordinate painting remains unsupported and is rejected before
accepting a gesture. This is not completion of the full authoring requirement.

`Document::reject_texture_paint_locked` provides the shared refusal for native
ray/contact painting, desktop admission and legacy callbacks. The paint kernel
also rejects at begin_stroke before changing any editable tile or stroke state.
The message asks for an editable texture atlas; it exposes no codec details.
Weights and explicit shapekey editing retain their separate paths. The import
report explains the same limitation. No source array is relabeled, no stored
material role is changed, and no source data is cleared to obtain editability.

The user can explicitly prepare during import or through the native UV
workspace. The tested same-object UV Pack/Apply path runs through the existing
asset worker, retains exact first-origin UV2 and makes the current atlas
paintable. Accepted paint followed by immediate ordered Save/reopen succeeds.
The former default-UV1 refusal fixture now proves represented readonly import
and an atomic paint refusal; the explicit editable-atlas positive remains.

The focused optimized run passes7/7 in5.49s. A fresh whole-owned C/C++core,
support and renderer O1g1 ASan/UBSan/leak build passes7/7 in79.90s. It includes
57 actual new native checks, old UV1/kernel/render/atlas regressions and the
additional-UV importer/generated-frame/all-role consumers. The57 checks prove:

- Exact original input pixels at three morph weights (error0), nine mixed-pose
 native/current-output comparisons (error0–1), and visible wrong-set sensitivity.
- Raw Save/reopen, exact import Undo/Redo, no rejected-paint history entry and
 no scene/revision/dirty/native-work changes from a refused stroke.
- Direct native, desktop, kernel and deferred/controller refusals; focus loss
 and trigger-held return; genuinely nonrigged legacy callbacks and the actual
 affine reflected/sheared evaluated-world path.
- Held isolation, live held motion, queued Save sealing placement independently
 of rejected paint, and exactly one motion history entry with paint bytes exact.
- Native import Cancel/retry and actual UV Pack→Apply→paint→ordered Save/reopen.

Actual FLTK chooser/options/Cancel/Save/Undo/Redo pass12 raw +14 atlas checks.
The atlas screenshot waits for a real rendered checkmark. An initial white-only
pixel predicate timed out because DomeD draws the active marker blue; its
negative and diagnostic bitmap remain preserved. Correcting that test predicate
required no runtime change. Actual synthetic OpenXR loader/GL swapchains pass
2/2 suites with20 raw +20 prepared import/Cancel checks in73.68s, including
balanced stereo ownership and captured native palette frames. This does not
claim a physical headset test.

The final PCI03 Intel Arc A770 batch passes288 assertions: raw57, generated26,
role151 and explicit native54. Strict device assertions reject software/UHD;
all672 source/header/test/CMake/core/support/render/native/fixture hashes match
before and after. The runtime is byte-identical to the sanitizer source; only
the GUI marker test changed afterward. All pixel limits remain unchanged.
The raw workflow corpus has24 GLBs with0errors/22warnings (two native runs,
not24 unique semantic variants); the final hardware corpus has101 GLBs with
0errors/71warnings. Khronos2.0.0-dev.3.10 uses unlimited, unfiltered issues.

Scope remains explicit: direct default-import/controller/UI qualification here
uses real UV2, with UV1 compatibility and independent UV2–UV7 role coverage from
the qualified foundation/regressions. It does not qualify arbitrary UV editing,
Paint24/current-domain composition, actual FBX extra sets, broader VRM export or
sets above7. The new projection-paint UI is absent from this older Source21
base; native/kernel seams protect its shared entry path, but actual composed
projection qualification remains a future union gate. Root's latest public
FBX4/rigfit/PBR mask-fence repairs and private Source24/25 fields must be preserved
when integrating; this isolated branch does not infer those features. No main
push, installation, session restart or physical headset launch occurred.
