# Retained-frame export implementation and remaining work

This milestone is partial progress toward complete deforming glTF/VRM authoring.
It does not enable the live Source UV Apply control or reinterpret the legacy
Scene8/material-archive1 derivative normal data.
Its optional retained-frame data is explicitly Scene9/material-archive2.

The native worker retains independent original PBR, original MToon and native
canonical normal/tangent frames. Base normals and every original or native morph
direction remain authoritative, including genuine normal-only primitives. Stable
target IDs separate current native slots from historical source target indices.
Current skin weights and pose remain editable. Source UV adoption retains those
frames while remapping position, skin and attribute identities through seams.
The captured original avatar and raw attributes remain immutable.

## Standard output currently represented exactly

GLB selects the PBR frame; VRM0 selects the legacy MToon0 frame and proves the
shared core PBR fallback needs compatible full attribute channels. Differing
shader interpolation alone does not require different attributes. Every needed
normal/tangent target and all other vertex attributes are split and remapped
together. No geometric normal replacement is performed. Generated missing-base
tangents include the original target keys; authorial tangent/normal keys are
preserved. An N-only export does not invent a tangent or UV authority.

Full canonical normal coverage has a conservative exact path: every texel touched by a triangle’s continuous bilinear sampling footprint
is 255, selected and canonical complete direction channels agree, each
triangle has constant orthogonal base N/T, all direction deltas are zero, and
skin weights are absent or all three triangle vertices have the same single
joint at weight one. Position morphs are allowed. Inverse-transpose normal and
linear tangent transport preserve orthogonality for a nonsingular rigid joint
transform, including anisotropic scale. Unused atlas gaps are allowed only after bounded triangle/texel-support
intersection testing proves they cannot affect a sample. The raw canonical bitmap becomes the
standard normal texture at unit strength with the native clamp/linear sampler.
VRM0 also requires identity shared ST and no active UV animation; expressions
that would alter the new normal role's scale or sampling domain are guarded.

## Why the remaining cases need a representation change

Let a fragment's interpolated canonical columns be C=(T,B,N). Native canonical
normal evaluation is normalize(C d). A standard consumer that normalizes each
column first evaluates normalize(normalize_columns(C) d). On a curved triangle
the interpolated columns generally have different lengths. For a fixed pose one
can change the texture vector d to compensate, but the required compensation
depends on those column lengths. Morph and skin pose change them. A single
static normal bitmap cannot encode that pose-dependent correction in general.
The strict flat/rigid path above is a sufficient case where this difference
vanishes; it is not a declaration that all other inputs are mathematically
impossible.

With partial coverage the native result is
normalize((1-c) normalize(S d_s) + c normalize(C d_c)), where original and
canonical frames can differ. Their relative directions can change under
deformation. Converting the two maps to one texture in a rest frame does not
establish equality in other poses. A single glTF primitive also has one NORMAL
and TANGENT channel set, so incompatible required PBR/MToon direction channels
cannot both be silently substituted by one set. Explicit source-role and frame
compatibility checks remain necessary even if a validator accepts the file.

KHR texture-transform normal-frame consumer conventions remain guarded when
unproven. Legacy MToon0's established sampling-ST/vertex-TBN convention is kept
separate from generic PBR claims. See [the pinned source review](NORMAL-FRAME-TRANSPORT.md).
The chosen weighted-joint inverse-transpose native skin convention is explicit;
consumer evidence applies to the independent fixture's declared convention,
not to every implementation of the glTF ecosystem.

Nonlinear creator Shape export to GLB retains the newly authored native key
model. Version-preserving VRM0 export still refuses bone-local spring/collider
or first-person metadata without a proven native reauthoring path. A nonlinear
map of an original sphere is not generally a sphere. Matching geometric output
does not justify silently retaining incorrect local physics metadata.

## Next concrete routes

1. Add an explicit reversible preparation operation that selects a standard
   representable target frame and material policy, reauthors the native preview
   to that same policy, and produces a bounded receipt retaining original paint,
   frames, raw origin, target identities and conversion settings. Report angular
   differences at the current pose and declared morph/skin stress samples; do
   not claim those samples prove equality for every possible pose. Require the
   user to inspect the concrete quality report before replacing authoring data.
   Native preview and independent exported consumers must then match the newly
   authored model, with an exact restoration path to the original.
2. Alternatively support a standard representation with enough explicitly
   defined channels/evaluation to express the native blend, backed by actual
   independent consumers. An undocumented engine-specific extras field alone
   does not provide interoperable normal evaluation.
3. Reauthor creator bone-local metadata at the actual known Shape Jacobians and
   key creation step, version the operation, and describe any collider fitting
   error explicitly. Preserve original metadata in the reversible receipt.

Until one of these routes is complete, deformation cases outside the proven
subset receive an actionable refusal and retain the full native source data.

## Vertex-color proof boundary and required follow-up

The original retained-frame checkpoint `414799f` preserved source `COLOR_0`
accessors through full corner, skin and POSITION/NORMAL/TANGENT morph remaps,
but its graphical proof covered fixtures without active vertex colors. The
subsequent [static color implementation](SOURCE-VERTEX-COLORS.md) adds native
PBR modulation, versioned MToon ignore behavior and independent GLB/VRM
consumers for raw, painted and adopted-UV static colors under position/direction
morphs and skinning. This does not add actual COLOR_0 morph targets.

General colored-target baking, fractional coverage without double modulation,
and color/UV morph-channel import/export remain required follow-up. Original
immutable color attributes remain retained; neither checkpoint is complete
glTF support. The rendering contracts are:

* [glTF core at a55d8458c1e82720413acd3b420fd09e73b8e40b](https://github.com/KhronosGroup/glTF/blob/a55d8458c1e82720413acd3b420fd09e73b8e40b/specification/2.0/Specification.adoc):
  interpolate linear vertex RGBA and multiply the base-color value; decode
  texture RGB before linear filtering and multiplication, and keep alpha linear.
* [MToon1 at 94e82dd346fa6cf0337c4421728640e5252dd38e](https://github.com/vrm-c/vrm-specification/blob/94e82dd346fa6cf0337c4421728640e5252dd38e/specification/VRMC_materials_mtoon-1.0/README.md#vertex-colors):
  vertex colors are ignored. The inspected README blob is
  `887afb75e21bcc8f9bf697f5f33280a8d4d21ada`.
* [UniVRM0 at efd20f62f1bf0ac3fcc34de1e497324c5197d146](https://github.com/vrm-c/UniVRM/blob/efd20f62f1bf0ac3fcc34de1e497324c5197d146/Packages/VRM/MToon/Shaders/MToonCore.cginc):
  `InitializeV2F` carries the vertex color, but fragment evaluation does not
  consume it. The inspected shader blob is
  `204e6ca2484b65a340a991f5d1e45ebbe977f527`. This is pinned legacy consumer
  evidence, kept separate from the core PBR fallback's required multiplication.
