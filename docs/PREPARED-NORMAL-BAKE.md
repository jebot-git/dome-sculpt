# Explicit normal attributes for new bakes

This private checkpoint connects an opt-in native normal-bake model to the real Document worker, paint composite, renderer, native archive, glTF/VRM export, FLTK dialog and controller panel. It is an explicit authoring conversion with retained original state. It does not reinterpret historical normal bakes or complete arbitrary source-normal conversion.

## Authoring and persistence

Enable normal preparation in the existing Bake dialog. The normal-error control is measured in degrees. Controller page12 cell9 opens the same preparation policy with four rows: color opt-in, color error, normal opt-in and normal error. Defaults preserve historical behavior. Cancel and focus loss discard staged choices. Projection/partial-miss settings remain unchanged.

`prepare_source_normals` creates immutable intrinsic PBR/MToon planes and a `SourceNormalPreparation` before-state containing the exact original paint, appearance, deformation, material binding and target-frame authority. Imported complete PBR attributes remain authoritative; N-only inputs retain their artist normals while explicitly authoring missing tangent channels. Generic native geometry authors base and individual morph-key N/T from native key geometry before skin. This defines a new editable model; it does not claim recovery of missing artist directions.

Typed preparation currently requires matching complete PBR/MToon attribute channels and identity supported sampling. It preserves separate role images and strengths by preparing separate intrinsic planes. Different original attribute roles, historical baked normal pixels, unsupported UV transforms/animation and UV1 source maps remain actionable refusals. Original source bytes and identities are retained; no original image whitening, fabricated CreatorAppearance, or canonical-frame relabeling occurs.

Scene15/material archive4 and baked payload3 discriminate the new interpretation. `BakedNormalInterpretation::PreparedAttributes` stores paired final detail planes; previous payloads retain CanonicalMikk semantics. Initial conversion planes, receipt and first before-state survive repeat bake, clear-bake, Undo/Redo and restart. Current paint/receipt and retained before-states must each possess their own matching authority. Cyclic material graphs, unbound planes, unrelated ownership aliases and changed REST/source-corner/sampling-UV data are refused. Weak owner-based proof caching avoids repeating source rasterization and key generation across pose/stereo validation without retaining deleted source graphs.

The receipt's observed angular error measures initial sampled encoding/quantization only. It is **not** an all-pose, all-filter or universal renderer preservation bound. The new model blends intrinsic/detail vectors in its declared tangent space and exports the exact same resolved planes. Each projected texel independently satisfies the selected conditioning/quantization limit. This is authored behavior; it is not the historical normalized-world-space blend under arbitrary nonorthogonal bases.

## Declared consumer conventions

The PBR policy is named `SampleRenderer2026`. Its vertex bitangent is normalized before interpolation and interpolated frame columns are normalized before normal-map evaluation, following inspected Khronos Sample Renderer revision `6e8a3d1aaa6f07e9f1310a488453e21325677786`: [primitive.vert](https://github.com/KhronosGroup/glTF-Sample-Renderer/blob/6e8a3d1aaa6f07e9f1310a488453e21325677786/source/Renderer/shaders/primitive.vert#L138-L151), [animation.glsl](https://github.com/KhronosGroup/glTF-Sample-Renderer/blob/6e8a3d1aaa6f07e9f1310a488453e21325677786/source/Renderer/shaders/animation.glsl), and [material_info.glsl](https://github.com/KhronosGroup/glTF-Sample-Renderer/blob/6e8a3d1aaa6f07e9f1310a488453e21325677786/source/Renderer/shaders/material_info.glsl). Skin transport uses weighted per-joint inverse-transpose normal matrices separately from position matrices. These are pinned interoperability conventions, not guarantees about all glTF implementations.

Legacy MToon0 uses raw interpolated TBN, following UniVRM revision `efd20f62f1bf0ac3fcc34de1e497324c5197d146` `Assets/VRMShaders/VRM/MToon/Resources/Shaders/MToonCore.cginc` (see NORMAL-FRAME-TRANSPORT.md). Sampling transforms and attribute-frame authority remain distinct. Unity/VRChat's complete mixed nonuniform-bone skin behavior has not been independently qualified. The independent VRM consumer tests the declared weighted-joint transport plus raw MToon0 evaluation. No external Unity interoperability claim follows.

The new PBR policy applies only to prepared-normal objects. Historical source shaders and canonical-bake semantics remain unchanged. Generic GLB exports include base TANGENT and every retained NORMAL/TANGENT morph channel. VRM emits its separate MToon plane while keeping the same file's PBR fallback correct; current normal strength0/fractional/amplified and disabled states remain effective.

## Qualified scope and remaining work

The focused native workflow exercises actual Document bake, save/reopen, export, repeat/clear, one Undo/Redo, mixed skin and morph targets. Independent cgltf/PNG/Gauss-Jordan/GLSL consumers compare visible normal-map pixels across three poses and morph weights0/.35/1. The fixtures include curved surfaces, artist direction deltas, mixed anisotropic skin, both MToon0 and PBR fallback, current normal strengths and disabled mapping. Actual second-eye/static draws perform zero texture/buffer allocation or upload. Fresh whole-core/render/vendor ASan/UBSan/leak checks, CPU/OneAPI, native FLTK normal/compact flows and actual synthetic OpenXR controller flows are recorded in the adjacent receipt.

This is not a full-suite requalification of every application feature. Schema validation does not itself prove normal-map pixels. Current graphics evidence uses the independent declared consumers; filtering/all-external-consumer equivalence is not claimed.

Still required:

- Explicit source-aware UV reprojection of intrinsic and paired detail planes. Actual changed UV Apply currently refuses while retaining the exact draft, scene, source data and history.
- A source-frame-aware bake adapter for prepared sources and differing original PBR/MToon source roles. Prepared sources currently refuse instead of substituting posed-atlas Mikk.
- Pose-qualified conversion outside the supported shared target-frame route, including arbitrary conditioning, distinct role frames and unsupported sampling transforms.
- Coherent Scene13 affine placement, Scene14 current polygon RGBA and Scene16 shape-source unions. New normal preparation is not a witness for current polygon colors or topology. Shape-key replacement must explicitly reauthor its receipt/frame identity or refuse; it cannot silently swap target arrays under an old recipe.
- Native canonical/prepared normal behavior under arbitrary affine placement; original glTF frame tests are separate evidence.
- Full COLOR_n/TEXCOORD_n morph-channel support remains separate from static color with POSITION/NORMAL/TANGENT morphs.

At schema union, take the maximum feature version and recurse all preparation before-bindings, UV origins, PolygonOrigins and direct shape-source capsules, charging retained images/paint/deformation once under the existing bounded owning graphs. Reserved Scene13/14 are deliberately not guessed by this isolated Scene15 decoder.
