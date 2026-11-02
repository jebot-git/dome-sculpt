# Native humanoid rig fitting

This private component connects explicit humanoid landmarks to actual native
FLTK/OpenXR authoring. It extends the existing application and retains the full
production goal. This checkpoint fits an **unskinned painting mesh**; fitting,
reparenting and changing the rest/bind space of existing grouped/source VRoid
rigs remains required and is the next Scene25 phase. An existing rig is refused
rather than replaced with a newly guessed hierarchy.

## Using the tool

Choose **Paint → Humanoid rig fitting…** on an editable painting mesh. Select
required and optional roles, including fingers, mark each placed landmark and
edit its world XYZ position. Existing landmark or humanoid metadata seeds its
own original bone heads. Otherwise landmarks begin unplaced; this is explicit
anatomical authoring, not silhouette recognition. No default mesh or MakeHuman
geometry is added. The original verified VRoid-only creation choices remain.

The scrolling native dialog has fixed Cancel/Apply controls and respects the
DomeD fonts/theme/universal Close route. Review the model-space body frame and
world surface/segment limits, terminal bone length, skin seed distance/margin,
falloff and influence cap. Apply runs fitting and optional automatic binding on
the ordered authoring worker. Cancel before Apply changes no Scene/history.
Worker Cancel retains the complete prior unskinned source; the same worker can
retry. A following Save waits for the actual accepted fitting endpoint.

In XR, open **Objects → Avatars → Rig and shapes → Humanoid fitting** (page27).
Previous/Next selects any of the55 roles. **Place landmark** owns a separate
controller trigger gesture: release to arm, press/hold to place the selected
landmark at the grip, then release to accept its guide position. Focus/tracking
loss or joystick cancellation restores its prior guide. X/Y/Z buttons move it by
a world-metric step, including reflected/sheared and nested group placements.
**Fitting options** switches the same page to distances/binding/body-frame
rotation/optional-landmark controls. Apply fits; Cancel leaves the original
Scene unchanged. Global Undo remains available on the root/other workflow pages.

A bounded GPU line guide displays placed landmarks and anatomical continuations.
It creates tool glyphs, not model geometry; there is no scene extraction or GPU
readback. The application still uses the existing bounded CPU fitting and weight
kernels on the worker. It makes no sustained production-size performance claim.

## Coordinates, sources and persistence

Landmarks and the explicit anatomical left/up/front frame belong to the immutable
painting REST coordinates. Shared UI/controller positions are converted through
the exact full object/group matrix. Independent proximity checks and automatic
binding use world-space triangles and bone distances. Reflection changes triangle
orientation in the detached metric copy without touching the original mesh.
Existing skin/morph/source/paint data is never silently flattened.

The fitted skeleton, skin weights, humanoid mapping and reviewed landmarks use
the existing DeformationState format. Original painting/UVs, materials, raw
imported bytes and attribute mappings, shapekeys and active morph values remain
owned unchanged. Tool selection/placement/options are ephemeral. This initial
component introduces **no Scene schema**. Accepted fitting is one exact global
Undo/Redo, and Save/restart retains actual editable authoring data.

The upcoming source-rest phase reserves Scene25 for exact pose-local affine
matrices and durable acyclic first rig-source capsules. Its work must preserve
original local graph/node identities across linked skins and rigid children,
transport authored inverse binds and current pose coherently, refit supported
VRM bone-local metadata and refuse unrepresentable affine physics shapes. The
initial unskinned guard is a checkpoint limit, not completion of that requirement.

## Qualification scope

See [validation/RIG-FITTING.json](validation/RIG-FITTING.json) for source/binary
hashes, exact terminal logs and limitations. Fresh canonical Release and complete
owned C/C++ core/support/renderer/frontend ASan/UBSan/leak builds were used.
System driver/toolkit/LAPACKE libraries are not instrumented by that claim.

Independent fixtures cover rest/posed GLB weights, inverse binds, morphs and full
nested reflected/sheared world coordinates through cgltf. Actual native imported
GLB fitting retains its original raw source/appearance/paint identities. Pinned
Khronos validation reports zero errors across the three emitted GLBs; its two
warnings are retained. Native/controller worker/history/cancellation and strict
actual A770 guide images are qualified separately from synthetic OpenXR and real
FLTK/native Wayland input. Warm-kernel persistent allocation boundaries are
explicitly measured; they do not claim process-first-use or all-application OOM.

No physical headset/controller/stylus session or system install occurred. Existing
shared-backend Wayland EGL and standalone-XR leak reports remain open from the
lifecycle component; native X11 graceful sanitizer checks use no suppressions.
The full source-rig/rest/reparent/VRM workflow, physical acceptance and all other
REQUIREMENTS remain active. Deferred controller drawers still follow basic tools.

## Integration

The initial feature appends `EditorState.rig_fit` and new Document APIs; fresh
header consumers are required. It uses the existing SceneOperation worker rather
than adding a fake accepted result or another thread. Commands2048–2061 and
page27/count28 coexist with projection155–164/page25 and the separate PBR preview
1024–1026/View26. Preserve the real PBR range/view epoch on eventual union; view
changes must respect a live landmark-placement gesture. Scene24 writable paint
sampling and the posed projection sampler remain independent and unchanged.
