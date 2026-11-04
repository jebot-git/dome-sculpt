# Physical emission in editable-project GLB

This private continuation adds a measured finite Core-PBR view for genuine
Source4 painted projects while retaining the complete original Scene as the
sole exact native authority. It uses the existing explicit EditableGlb export
policies. Ordinary strict painted GLB and the existing HDR package entry points
keep their guards and interpretation. No public class layout, Scene version,
EXR record or competing HDR package marker is added.

## Owning compilation

`NativeAuthoringCapture` authenticates and snapshots the real Scene before
staging any emission-free compiler inputs. Only its internally derived surfaces
reach the private material compiler. The physical adapter reuses the existing
source-aware material raster for non-emission channels and a second pass over
the same face/barycentric raster and padding. It samples each actual original
emission image with its own logical UV set, transform, wrap and filter, and
samples the independently filtered editable float/legacy overlay in its real
paint domain. Masks, layer opacity, hidden layers, float replacement coverage,
absolute background overrides and material-relative versus physical units stay
separate. Both passes and their live buffers count against cumulative budgets.

The finite emission image normalizes each channel by its real peak and emits
an sRGB8 PNG, an emissive factor and KHR emissive strength. Zero channels remain
zero. No value is silently clipped or described as HDR/EXR storage. The private
finisher runs before native attachment hashes; it uses actual object/node/mesh/
material remaps, splits shared meshes/materials when required, and leaves
unrelated aliased objects unchanged.

The descriptor says `lossless: false` and reports maximum **texel quantization**
error in native linear radiance units, erased nonzero samples and zero clamped
samples. It explicitly has no between-texel error bound. Unlit materials ignore
emission; ignored nonzero radiance is reported separately from PNG quantization.
The complete native Scene, including float paint, source bytes, hidden objects,
rig/artist targets and immutable aliases, is restored exactly on Open and Save.

## Raw HDR initial adoption

An explicit editable-atlas import can now fold the actual original emissive
factor times strength into the bounded native material, then publish only after
the real Source4 factory produces a valid original-sampling attachment and the
complete Scene validates. The raw factor, strength, images, sampler/UV roles and
source bytes remain original values. No temporary rewrite makes an HDR source
look like LDR. The existing source/native 10,000 bound remains enforced.

Tests cover pristine raw-HDR projects with no float paint, followed separately
by genuine physical and legacy paint, masks and hidden layers. They also cover
nontrivial factors, an empty emission-image role, unlit and typed MToon. Unprepared
raw-HDR native import still refuses; preserving-only raw/native domains without
the authenticated Source4 route remain required work.

## Evidence and limits

The frozen5082 foundation passed six selected Release and six matching owned
O1 ASan/UBSan/leak scopes, including128 independent/native checks on seven cases.
The exact A770128 run passed; all21 files/profile match across Release, sanitizer
and hardware. Its63-file Khronos cohort has zero errors and nine unfiltered
unlit-plus-strength warnings. Seventeen historical portable-bake outputs and
eight historical HDR outputs retain exact old bytes in both profiles.

A real persistent-allocation test then found a post-rename allocation in the
shared writer. The isolated old-core counterfactual reports prefix9385,
`FILE_CHANGED=1`, `OWNER_CHANGED=0`. The portable atomic-write fix is documented
in ATOMIC-WRITE-ALLOCATION.md. Rebuilding every dependent core TU and relinking
the app/consumers yields34 functional contracts and9,387 denied C++ allocation
prefixes followed by exact successful recovery in each Release/sanitizer profile.
This is a C++ allocation sweep, not a malloc or disk-failure sweep. Actual running
and queued export cancellation, retry and a following ordered native Save pass.

The raw-adoption continuation passes127 checks in Release, owned sanitizer and
strict PCI03 A770: five cases, each pristine and painted. All ten independent
Core-PBR comparisons retain the existing two-pixel bound (interior maximum1);
actual default-authored/MToon and Core-PBR native recovery pixels are exact0.
All26 files/profile are byte-identical. Its78-file Khronos cohort has zero errors
and15 unfiltered unlit-plus-strength warnings. Matching selected legacy tests
and historical portable192/HDR byte controls also pass.

The5082 foundation owns its complete matching C/C++ core/support/render/app
builds. The later IO and raw-adoption proofs are explicitly **component rebuilds**:
all four IO-dependent core TUs, then import-authoring.cpp, are recompiled into
private copies of those immutable archives; apps and consumers are relinked.
There is no new public ABI. This is not a fresh whole-source/main integration,
full regression, actual GUI or physical-headset qualification. The external
canonical bitmap/FLTK SDK is not claimed instrumented by these tests.

A genuine finite-filter counterexample remains preserved: with independent
nearest original sampling and smooth float paint, the normal/UV1 case has an
interior display difference6 against the original two-pixel criterion. At its
measured point the original linear red is4.93488; four unquantized compiled
texels interpolate7.29034, while PNG reconstruction gives7.30308 and its derived
texel quantization bound is0.0413041. The filter mismatch is not normal loss or
clipping. This case does not pass the original lossless criterion. Native
recovery still has exact pixels; the other six foundation cases retain their
original bounds. No tolerance was widened.

Generic export for retained Paint24/native-only physical domains, nonprepared
preserving-only HDR projects, strict portable equivalence, and broader source/
normal/geometry authoring remain required. Their guards stay explicit. Existing
HDR package1/2 and ordinary strict export are not relaxed. All other unfinished
full-goal requirements, including general refinement and later controller
spatial drawers, remain required. Nothing here changes installed files or an
active desktop/HMD session.
