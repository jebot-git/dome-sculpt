# Native MToon material and shader foundation

This component implements separate MToon 0 and MToon 1 material models, native
persistence, numerical reference shading and reusable GLSL. It replaces no scene
or renderer code by itself. A generic `toon` boolean is not an MToon material.
The caller must bind typed materials, texture roles, render states and outline
passes before claiming MToon rendering in desktop or XR.

## API and durable data

`MToonMaterial::from_vrm0()` accepts one original VRM 0
`extensions.VRM.materialProperties` entry whose shader is `VRM/MToon`.
`from_vrm1()` accepts an entire glTF material with a
`VRMC_materials_mtoon.specVersion == "1.0"` extension. The version is immutable;
there is no silent conversion between the two models. `parameters` contains
linear rendering values and explicit version-specific legacy fields.

The original JSON, unrecognized float/vector/texture properties, keywords/tags,
extensions and extras survive. Named diagnostics identify unmapped shader data,
unsupported coordinate sets and conflicting legacy alpha keywords. The source
format is not reduced to a common PBR material. `source_json()` merges only known
edits and preserves original omission/default choices and unrelated payload.
Texture IDs remain indices in the original asset's texture array; an exporter
must remap them deliberately. Unrecognized fields are retained, not presumed
safe to remap or execute as custom shader code.

Native `encode()`/`decode()` use `format="dome-mtoon"`, native `version=1`,
`sourceVersion=0|1`, the source material and a complete typed `draft`. Drafts retain
settings for unassigned texture slots and exact linear authoring values even
when the source format cannot yet represent an unassigned slot transform/normal
scale. This avoids inventing invalid glTF texture references merely to save an
editor draft. Default native construction produces a valid MToon 1 material.
The parser rejects malformed versions/types/dimensions, nonfinite values,
invalid domains, slot references and inconsistent derived render state.

Raw source JSON is bounded to 2 MiB, 100,000 visited values and 32 nesting levels;
its native wrapper allows 4 MiB, 200,000 values and 36 levels. Known rendering values
must fit native floats. Unknown fields remain explicitly uninterpreted. Passing
an asset texture count validates all supported slot indices against that asset.
Additional coordinate sets are preserved with diagnostics; the renderer must
supply the selected set rather than silently use UV0.

## Texture roles and color space

| Role | Encoding | Missing sample | Notes |
|---|---|---|---|
| Base | sRGB RGB, linear alpha | white | Opacity comes only from factor alpha times this alpha |
| Shade | sRGB | white | Independent shade factor and map |
| Normal | Linear tangent normal | (.5,.5,1) | Signed strength; world TBN supplied by renderer |
| Emission | sRGB texture | white | Factor is linear in both models |
| Rim | sRGB | white | V1 masks matcap as well as parametric rim |
| Matcap | sRGB | black | View-normal coordinates; independent of UV animation |
| Shading shift | Linear R | zero | V1 only; per-slot scale multiplies sampled shift |
| Outline width | V1 linear G; V0 legacy sRGB R | one | Versions use different channels and units |
| UV animation mask | V1 linear B; V0 legacy sRGB R | one | Masks animation speed |
| Receive shadow | Legacy sRGB R | one | V0 only |
| Shading grade | Legacy sRGB R | one | V0 only |

`mtoon_default_texel()` exposes encoded defaults. Encodings/channels are derived
from role and version and validate as such. Legacy mask sRGB interpretation
follows the pinned UniVRM 0 texture importer, which creates all non-normal maps as
sRGB. It is not silently changed to the V1 packed linear-mask convention.

Slots retain original texture index, texcoord, KHR texture transform/override,
offset/scale/rotation and applicable shift scale. Normal-map strength is the
material's `normal_scale`, not a second independent slot strength. The host must
resolve sampler/filter/wrap state through its original texture/image cache.
Images can share decoded bytes while GPU texture views/storage distinguish sRGB
and linear roles. No CPU color baking or GPU readback is needed for shading.

V0 lit/shade/rim/outline color factors are decoded from gamma/sRGB values, following
UniVRM's migration implementation. Its emission factor is already linear. V1
factors are linear. Alpha and normal/data values are never gamma-decoded. Typed MToon takes
precedence over a companion `KHR_materials_unlit`; that companion is a fallback
rather than a reason to skip the supported MToon model.
`mtoon_decode_texel()` is for raw texels; decode before CPU filtering, or use
`GL_SRGB8_ALPHA8` color storage so hardware decodes before texture filtering.
Do not apply the helper a second time to an already decoded sRGB texture sample.

Paint integration must distinguish source samples from already composited maps.
Existing generic painted color/emission maps may already contain Material
factors. Multiplying those maps by the same factors again is wrong. Prefer
original cached source maps plus factor-aware paint composition, or pass effective
factor 1 only when the supplied derived sample already contains that factor.
Preserve the authored typed material separately from such draw-time overrides.
Vertex colors are not part of MToon shading and are intentionally ignored.

## Lighting and version differences

`mtoon_shade()` and `dome_mtoon_shade()` return linear straight RGBA for one
primary light plus host-provided environment irradiance. They implement:

- V0 asymmetric shift/toony thresholds, receive-shadow and shading-grade terms,
  light-color attenuation/desaturation, legacy GI interpolation and its diffuse
  clamp to lit color.
- V1 symmetric additive shift/shift-texture/toony terms and GI equalization from
  directional irradiance toward the average of up/down environment samples.
- Linear emission, parametric Fresnel/lift rim, matcap, rim-lighting mix and
  version-specific outline color multiplication.
- Opaque opacity 1, strict mask cutoff discard with surviving opacity 1 and
  straight blended factor-alpha times texture-alpha.

A negative legacy shade shift does not mean the same thing as a negative V1
shift. Likewise, V0 `indirect_intensity` interpolates from uniform GI toward
normal-dependent GI, whereas V1 `gi_equalization` interpolates in the opposite
direction. V0 matcap is independently additive; V1 rim texture multiplies both
matcap and parametric rim. V0 outline lighting excludes emission/rim/matcap;
V1 outline mix includes the complete shaded result.

`mtoon_shading_normal()`/its GLSL equivalent orient geometric N before building
TBN on V1 backfaces. They preserve tangent X while reversing geometric N and its
bitangent, rather than incorrectly negating an already mapped world normal.
V0 instead flips the mapped normal toward the view, then reverses it for the
outline pass. `mtoon_material_normal()` additionally respects V0's explicit
`_NORMALMAP` keyword independently of retaining a normal texture. V1 normal-map
activation follows texture presence. All results of the complete normal helpers set
`normal_oriented=true` (GLSL `normalOriented=true`) to avoid a second facing flip.
The host supplies post-skin/model inverse-transpose normals and a tangent frame
matching the selected transformed UV set, including tangent handedness.

Matcap helpers accept a caller-supplied up vector and handle view/up poles without
NaNs. They map right-handed glTF world vectors to top-left image UVs, accounting
for the handedness/image-origin conversion of the Unity reference; normal-up
samples the top and normal-right the right of the matcap. Camera-up gives literal view-space matcap coordinates; world-up reproduces
the stable-horizon convention of the inspected V1 reference. Scene environment
probes, shadow-map generation, fog, exposure and final output transfer belong to
the renderer. The V1 `shadow` input multiplies externally provided visibility;
legacy receive-shadow behavior follows the reference base-pass formula. Unity's
alternative PBR-correct keyword and additional-light pass variants are not claimed.
Do not repeatedly call the complete primary-light function for extra lights, as
that would duplicate GI/emission/rim.

The fully sharp V1 transition has a deterministic hard-step tie at zero; the
legacy curve retains its reference half-float epsilon. This avoids division-by-zero
without claiming bit-identical Unity half precision.

## Render state, UVs and outlines

`render_state(outline_pass)` reports depth writing, alpha-to-coverage metadata,
culling, queue and independent RGB/alpha blend factors. Blend factors use Unity's
numeric enums:0 Zero,1 One,2 DstColor,3 SrcColor,4 OneMinusDstColor,5 SrcAlpha,
6 OneMinusSrcColor,7 DstAlpha,8 OneMinusDstAlpha,9 SrcAlphaSaturate,
10 OneMinusSrcAlpha. Map them to the backend deliberately; reject unsupported
custom combinations instead of guessing.

V0 preserves authored render queue, blend and cull overrides. Its alpha blend
operation is MAX, matching the reference `BlendOp Add, Max`; V1 uses ordinary
straight-alpha RGB blending and source-alpha factorONE. V1 ordering is opaque,
mask, blended-with-depth-write, then blended-without-depth-write. Queue offsets
are validated in the corresponding 0..+9 or -9..0 partition and are zero outside
blend mode. V1 outline drawing always culls front faces, even with a double-sided
surface. V0 retains `_OutlineCullMode` separately. The host must implement the
separate inverse-hull draw after deformation; these functions do not schedule it.

V0 UV animation uses turns/second and shared `_MainTex` offset/scale for UV-mapped
roles. In its bottom-left convention, scrolling occurs before rotation; the
native API accepts glTF top-left coordinates and converts explicitly. Legacy
non-main ST values remain in the payload but are not falsely used by a shader
that samples only MainTex-transformed UVs. Legacy outline width samples are not
animated. V1 uses radians/second, top-left rotation followed by independent
scrolling, then the selected KHR texture transform. UV-mask sampling itself is
not animated. Matcap sampling does not use mesh UV animation.

The pinned V1 prose's displayed transform matrix and coordinate conventions are
followed for animation. Its pseudocode matrix wording and current Unity shader's
scroll-before-rotation implementation are inconsistent with that transform/order;
this choice is explicit and covered by numeric/GPU fixtures. The host should use
bounded time/phase for long sessions to preserve floating-point animation precision.

V1 world outline width is metres; screen width is a screen-height ratio. Legacy
width has the reference 0.01 multiplier, distance cap and view-normal fade for
screen outlines. GLSL also exposes exact legacy object-normal extrusion with a
normal matrix, preserving its nonuniform-scale convention. World/screen helper
functions provide offsets/data, not geometry scheduling, pixel-perfect silhouette
coverage, Unity derivative alpha-to-coverage edges or clipping robustness.

There is a real pinned upstream default conflict: V1 prose says omitted shade
color is black, but its JSON schema and UniVRM material default are white. This
module uses the schema/UniVRM white default and reports a diagnostic when omitted.
Authored values always take precedence. Legacy `_MToonVersion` is retained;
missing versions use the inspected reference defaults without injecting a version
into the original map. Arbitrary custom/debug Unity keywords remain retained with
diagnostics rather than claimed executable shader features.

## GLSL integration

`mtoon_glsl()` returns a reusable GLSL include string from `mtoon-shader.hpp`.
It has no version directive, external file lookup, Unity include or runtime.
Prepend the host's GLSL version and append its vertex/fragment entry points.
The `DomeMToonSamples` values are already linear, and `DomeMToonLight` contains
world-space vectors plus raw GI/up/down irradiance, shadow visibility and explicit
normal orientation. Use the normal/UV/matcap/outline helpers as required.

`gpu_parameters()` is 12 contiguous vec4 values, suitable for `glUniform4fv`:

| Index | Contents |
|---|---|
|0|Linear base RGBA|
|1|Shade RGB, shift|
|2|Emission RGB times strength, toony|
|3|Rim RGB, power|
|4|Matcap RGB, lift|
|5|Outline RGB, outline lighting mix|
|6|GI equalization, rim mix, normal scale, alpha cutoff|
|7|Legacy receive-shadow, grade, light-color attenuation, indirect intensity|
|8|Version 0/1, alpha enum, cull enum, legacy fixed-outline flag|
|9|Scroll X/Y, authored rotation speed, authored outline width|
|10|Outline mode, legacy max distance, outline cull, shift-texture scale|
|11|Normal-map enabled flag, reserved zeros|

Parameters remain linear until the host's final framebuffer/output transfer.
No Scene, paint, renderer or exporter file is modified by this foundation.

## Exact primary references and licensing

Official VRM specification repository commit
`94e82dd346fa6cf0337c4421728640e5252dd38e`:

- `specification/VRMC_materials_mtoon-1.0/README.md`, SHA-256
  `ac2b6060587b2cd5ddabb26bd7a07e04992a73d0bcbae5e6acb4cdc94da3f70b`.
- Its `schema/VRMC_materials_mtoon.schema.json`, SHA-256
  `39e69f102c7ae3bdaa35ff8f2bcbc022a5a4c052fe97009198566105c667e941`.
- `specification/0.0/schema/vrm.material.schema.json`, SHA-256
  `b084587aa63a9486f2cc44f2aad3f57ec9a2f6f3ff4c003f61ef2a2d335dcf50`.

The spec defines material fields, channels, defaults, order and equations; no
specification text is vendored. Primary source:
<https://github.com/vrm-c/vrm-specification/tree/94e82dd346fa6cf0337c4421728640e5252dd38e/specification>.

UniVRM reference commit `efd20f62f1bf0ac3fcc34de1e497324c5197d146`, MIT:

- `Packages/VRM/MToon/Shaders/MToonCore.cginc`, SHA-256
  `b623d50bdfd64d808ba789d528b9d8ce7b8f55633af073962b50fd0a05f43fd9`:
  legacy shade/GI/rim/matcap/outline/UV equations adapted to native CPU/GLSL.
- `Packages/VRM10/MToon10/Shaders/vrmc_materials_mtoon_lighting_mtoon.hlsl`,
  SHA-256 `9450a91f375b30fde1e0a7e85d133fefb3eca5e80c72bfe9f4bb2370bfd45ba9`:
  V1 base-light, GI, rim, matcap and outline structure inspected/adapted.
- `.../vrmc_materials_mtoon_geometry_normal.hlsl`, SHA-256
  `bd4aa0aa4c834b856da00270883dfbaf9663de653afaef4a4bbe37409dc0b2d8`:
  correct normal-facing/TBN order inspected/adapted.
- `.../vrmc_materials_mtoon_geometry_uv.hlsl`, SHA-256
  `4f9d3d49569c59003ca103492dc5258675f37b94ce8d547a1c20578e29838a9c`:
  inspected; V1 animation order deliberately follows the normative spec instead.
- `Packages/VRM10/Runtime/Migration/Materials/MigrationMToonMaterial.cs`, SHA-256
  `67e62639c53906272008c593decc536cd37dea9a6507b84d6319871ac097d5d5`:
  V0 factor color spaces, ST packing, animation units/signs and outline units
  inspected; no silent migration is performed.
- `Packages/VRM/Runtime/IO/VRMMToonTextureImporter.cs`, SHA-256
  `cda806445e4fd24055fe14cc3f53037fbf59cbe586df2e15a13de78242d334f2`:
  legacy texture encoding and offset.xy/scale.zw inspected.

Reference source root:
<https://github.com/vrm-c/UniVRM/tree/efd20f62f1bf0ac3fcc34de1e497324c5197d146>.
Full MIT notices are retained in `MTOON-LICENSE.txt` and the standalone shader
header. CPU/schema integration is GPL-3.0-or-later, shader adaptation MIT.
No Unity, C#, Qt or external shader runtime is linked. Root integration must
update the central source manifest and distribute the notice with installed
artifacts. Do not claim the full UniVRM renderer or unsupported extensions.

## Focused validation

Native fixtures cover exact source/native-draft persistence, unknown fields,
version/color/mask differences, lit/shade endpoints, sharp boundaries, GI,
emission, rim/matcap, opacity/cutoff, culling/blend/queue state, front/back tangent
frames, normal-map keywords, UV order/units/transforms, outline width/projection,
invalid schema/domain values and matcap poles. Both verified actual CC0 beta
VRoid assets additionally round-trip all 15 original MToon 0 material entries each.

The independent EGL test runs the GLSL on Intel Arc A770 hardware, not a software
renderer. It checks 36 lighting/alpha/outline cases against native results, normal
and sRGB functions, true sRGB texture storage, UVs and outline offsets. Analytic
shader comparisons use tighter float tolerances. The hardware sRGB decode fixture
measured max RGB error 0.000451371 with exact alpha against the analytic transfer;
it uses an explicit 0.001 RGB bound and 0.00002 alpha bound, while still detecting
missing/double color decoding. This is GPU kernel validation, not a rendered
avatar/outline screenshot or XR session test.

```sh
c++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -Iinclude tests/mtoon.cpp src/mtoon.cpp -o /tmp/dome-mtoon-test
/tmp/dome-mtoon-test assets/humanoid/vroid/base-female.vrm assets/humanoid/vroid/base-male.vrm
c++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -DDOME_MTOON_GPU -Iinclude -Ithird_party/openbrush tests/mtoon.cpp src/mtoon.cpp third_party/openbrush/graphics.cpp -lEGL -lGL -o /tmp/dome-mtoon-gpu-test
/tmp/dome-mtoon-gpu-test assets/humanoid/vroid/base-female.vrm assets/humanoid/vroid/base-male.vrm
```

Final focused validation passes 48 native assertions in Release and
ASan/UBSan/leak detection, plus 58 assertions in the hardware EGL build (the
native cases and 10 independent GPU checks, including the 36-case shading sweep).
The sanitizer target instruments the new native material/test code;
the separate hardware run uses the Release driver. Logs are `artifacts/mtoon.log`,
`artifacts/mtoon-sanitizers.log` and `artifacts/mtoon-egl.log`.
