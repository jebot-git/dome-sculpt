# Private static VRM1 expression rendering

This test-only descendant of662e711 qualifies actual authored-MToon output of
the static expression operation. It changes no runtime, public layout, archive,
frontend or input identifier. Interactive expression controls and dynamic painted
expression export remain required. It is not main admission.

The valid synthetic source has actual original node/material identities, two
skins with different authored inverse binds, a rigid child, position/normal
morphs and retained VRM1/spring/constraint metadata. Only one source mesh part is
visible for the image comparison; the other parts and complete raw graph stay
in the native Scene. Its base, shade and emission views have real, different
samplers, nontrivial transforms and genuine UV0/UV1 accessors. A128×128 checker
exercises minification; the64×64 writable atlas owns separate actual native
color and material-relative emission strokes. Shade lighting is deliberately
visible. Strength1.3 remains separate from painted emission values.

The independent consumer parses the original GLB with cgltf, evaluates source
skin/morph geometry and static expression uniforms itself, and samples original
images using actual OpenGL samplers. Its test-only dense reference reconstructs
paint from persisted sparse tile bytes, independent UV triangle coverage and
nearest padding. It calls no production expression evaluator, compositor,
overlay compiler, point sampler or padding helper. Writable paint stays fixed
while original role UVs change. The GL consumer lives for the entire test.

Three head poses and expression weights0/.35/1 run through primary, shifted-eye
and minified cameras:27 independent native/reference pairs,54 PNGs. Every pair
has maximum error1, below the unchanged4-level bound. Primary coverage is
3660..3840pixels; minified coverage is441..484. Reopened native pixels are exact;
no-op and second-eye draws upload neither new geometry nor new images. Genuine
wrong-source-UV, moved-paint, changed magnification/minification, isolated shade/
emission UV and paint-tint controls all exceed2. This demonstrates independent
paint contribution rather than tinting or moving a flattened texture.

The fixture executes real native paint workers and native Save. Each rendered
sample publishes a detached static-kernel result through an actual captured
`Document::edit_scene` operation with immediate queued Save and native reopen.
The preceding662 milestone separately executes the expression kernel inside the
native worker and proves one Undo/Redo and late Cancel. This rendering test does
not claim an interactive preview-state or controller implementation.

Valid-input Release and asserted ASan/UBSan/leak-detected consumers each pass7
fixture +94 rendering checks. They compile complete new consumer TUs against
verified immutable662 owned support archives and matching canonical SDK, not a
fresh whole-app build. Both actual registered tests also pass per profile using
exact copied consumer binaries and a generated CTest command-path relocation.
617tests are configured; no full617 run occurred. All54 Release/sanitizer PNGs
and11 native/input files are byte-identical. The current render executable is
byte-identical to the preceding input-control executable in each profile.

Strict PCI03 actual Intel Arc A770 runs94 checks/all27pairs with max1; forced
llvmpipe `--hardware` refuses. Source/helper/core/binary/input manifests are exact
before/after. This is GL/native-worker proof, not GPU baking/compute or a physical
headset/session test. No GPU lease or finite job remains.

Preserved controls explain the test refinements:

- CPU-precombined UV coefficients produced one second-eye pixel error33 while
  upload counters stayed exact. The independent consumer now keeps authored
  factors on the GPU and evaluates the primary Khronos translation×rotation×
  scale construction. The same camera and4-level bound pass at max1. No runtime
  shader, image or tolerance was changed to repair that reference precision.
- A close-view minification-only negative changed zero pixels because the source
  was magnified. Separate actual minified and magnified views now detect the
  appropriate sampler changes; the original zero-effect failure is retained.
- The reused source generator initially inherited four empty child arrays,
  irrelevant opaque cutoff and an unlit fallback alongside the new strength.
  That raw control has4 Khronos errors/7warnings. The final *initial authored
  input* omits those fields before its first save, preserving every node, child
  order and material/image reference. No emitted export JSON was rewritten.
  All54 authored-view pixels remain byte-identical to the old input cohort.
- The primary strength extension excludes unlit co-use, but the pinned validator
  reports that combination as a warning. Pinned MToon gives its shader precedence
  over unlit fallback. No blanket importer refusal is inferred or implemented.

The final input has zero Khronos errors,5 unfiltered skin/rigid-reference warnings,
15infos and12hints. Six pinned official extension-schema instances pass. These
are input controls; the stage emits native Scenes/images, not new portable VRM
files. The genuinely guarded painted dynamic-expression exporter is unchanged.

Remaining work includes current interactive expression state and shared controls;
alternate CorePBR background-factor/alpha/strength semantics without changing the
paint reference; normal-image and prepared before-state adapters; plain-PBR and
absolute-background expression authoring; and representable expression-preserving
painted interchange. The authored-only image scope must not be described as full
expression preview/export or full VRM support. Every other unfinished original
and additional production requirement remains active.

Primary references: [KHR texture transform](https://github.com/KhronosGroup/glTF/blob/9d23a5a5b1d10f22d1554e847f2a1a3f24222392/extensions/2.0/Khronos/KHR_texture_transform/README.md),
[emissive strength exclusions](https://github.com/KhronosGroup/glTF/blob/9d23a5a5b1d10f22d1554e847f2a1a3f24222392/extensions/2.0/Khronos/KHR_materials_emissive_strength/README.md),
and [pinned MToon1](https://github.com/vrm-c/vrm-specification/blob/94e82dd346fa6cf0337c4421728640e5252dd38e/specification/VRMC_materials_mtoon-1.0/README.md).
See validation/VRM1-STATIC-EXPRESSION-RENDERING.json for exact cohorts, source/
binary hashes, process maps, registered command paths and all old negatives.
