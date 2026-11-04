# FBX morphs with explicit generated normals

The existing native FBX importer now supports **Generate flat** and **Generate
smooth** when the source contains morph targets. Previously either choice
refused the whole import before adopting any object. The existing FLTK Import
options and shared `ImportOptions` drive the adapter; no frontend, dependency,
public C++ layout or native archive field is added.

Each target retains its original sparse position offsets and effective weights.
Flat normal deltas use the target's existing ordered triangles. Smooth deltas
integrate the original polygon faces across the same shared control points as
the existing generated base normals. Both subtract the actual generated base
normal and write standard glTF morph NORMAL accessors. Original skin, hierarchy,
materials, UVs and morph identities remain on the existing conversion route.
This defines the individual target endpoints and ordinary glTF linear morph
semantics; it does not claim nonlinear geometric normal reconstruction for
arbitrary simultaneous shape combinations.

The complete original FBX and its authored normal data remain immutable in the
source receipt. The new explicit route is labelled `native adapter 5 / generated
morph normals`; default Preserve retains adapter4 and its prior bytes. Generated
normals do not reuse an authored tangent frame. A cancelled or invalid conversion
fails before scene adoption. Existing finite/nonzero-normal guards remain, and
new per-target temporary-byte and cumulative deformation-work checks run before
allocating the target buffers. The worker polls cancellation while constructing
positions, accumulating face normals and emitting target deltas.

## Actual evidence

The two pinned genuine Maya/Blender morph fixtures exercise both choices. An
independent sparse-offset evaluator and Newell/triangle normal oracle check216
normal corners, including a significant0.779345 target-normal change. Each
Release/sanitizer run passes1158 checks, with maximum normal component error
1.53527e-8 against the fixed2e-6 limit. Actual owning imports, current source
positions, native persistence/restart and independently parsed exports also
pass. Both default Preserve canonical files are byte-identical to the unchanged
prior core in each profile.

Four existing complete FBX consumers pass in each profile: deformation,
Document import/cancellation, named UV bindings and the75-row unit/axis matrix.
The complete `import-native.cpp` TU is compiled against the exact owned parent
headers and inserted into its matching core; app and consumers are relinked with
that same core and original matching support. Every other product/header/asset
input matches the owned parent. This is component/application qualification,
not a new whole-project or vendor build.

Actual native FLTK chooser tests pass17 checks per choice per profile (68 total),
including default Cancel, actual native flat/smooth popup input, background
import, explicit adapter/normal-mode persistence, retained original-frame target
normals, exact Save/Undo/Redo and normal app/graphics/font teardown. The tested
650px dialog with canonical bitmap text was visually reviewed. This uses the
current27/POLL client SDK and private Xvfb; no new Wayland, XR or positive GPU
qualification is claimed for this continuation.

All20 canonical/exported GLBs (10 each profile) pass the pinned unfiltered
Khronos2.0.0-dev.3.10 validator with0 errors and0 warnings;12 informational and62
hint messages are retained. All10 corresponding Release/SAN files are
byte-identical. A fresh CMake configuration registers the new native command;
registration is not an additional build/test execution.

The original whole-import refusal is reproduced against the old core in each
profile. The first new independent test incorrectly treated a shared glTF mesh
as a node instance and crashed; the sanitizer identified that test-only null
node lookup. The corrected oracle resolves the original parser mesh through its
explicit retained element identity. The production adapter is unchanged between
that failed test and the final positive runs. No assertion, quality bound or
sanitizer setting was weakened.

## Remaining scope

This private milestone still needs coherent main admission. Broader production
and singular/degenerate target fixtures, normal-map/tangent and curved/mixed
material consumers, device workflows and the remaining FBX conversion cases
stay required. Existing dual-quaternion/layered-deformer/cache restrictions are
not removed by this patch. It does not complete FBX, interoperability or the
full Dome Sculpt goal. No system installation, headset launch or active-session
restart occurred. Exact commands, hashes, scope and negatives are recorded in
`validation/FBX-MORPH-GENERATED-NORMALS.json`.
