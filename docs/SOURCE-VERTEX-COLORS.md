# Original vertex colors

This checkpoint connects retained **static `COLOR_0`** to the actual native
Document worker, renderer and material-aware picking. It builds on `40ea548`,
including the ordinary-scene capacity correction and the runtime posed-Mikk
fallback for old avatar bakes. It does not claim completion of color/UV morph
authoring or general color transfer between differently colored meshes.

The general **Import mesh** command retains its existing explicit guards for
vertex colors, authored tangents, UV1 and textured materials. This checkpoint
exercises preserved-source/creator appearance paths; it does not remove that
separate GUI import limitation.

## Versioned rendering contract

* glTF core PBR and unlit multiply linear vertex RGBA by the base-color factor
  and sampled base-color texture exactly once. Texture RGB is decoded from sRGB
  before filtering and modulation. Vertex alpha participates in Mask/Blend;
  Opaque ignores the resulting alpha. Vertex RGB does not multiply emission.
* MToon 1.0 ignores vertex colors, including alpha. Legacy MToon 0 follows the
  pinned UniVRM shader's behavior: its input color is copied into the vertex
  output but is never consumed by the fragment shader. The same exported VRM's
  core PBR fallback still retains and applies `COLOR_0`.
* Native painting edits the texture component. Original vertex colors remain a
  separate multiplier, including zero components. Source-UV adoption preserves
  original color identities through its explicit vertex remap; it does not
  rasterize the colors into the texture and multiply them again afterward.

Inspected primary sources:

* [glTF 2.0 Specification, vertex attributes, material base color and morph
  targets](https://github.com/KhronosGroup/glTF/blob/a55d8458c1e82720413acd3b420fd09e73b8e40b/specification/2.0/Specification.adoc),
  revision `a55d8458c1e82720413acd3b420fd09e73b8e40b`.
* [MToon 1.0 Vertex Colors](https://github.com/vrm-c/vrm-specification/blob/94e82dd346fa6cf0337c4421728640e5252dd38e/specification/VRMC_materials_mtoon-1.0/README.md#vertex-colors),
  revision `94e82dd346fa6cf0337c4421728640e5252dd38e`, inspected blob
  `887afb75e21bcc8f9bf697f5f33280a8d4d21ada`.
* [UniVRM legacy MToon shader](https://github.com/vrm-c/UniVRM/blob/efd20f62f1bf0ac3fcc34de1e497324c5197d146/Packages/VRM/MToon/Shaders/MToonCore.cginc),
  revision `efd20f62f1bf0ac3fcc34de1e497324c5197d146`, inspected blob
  `204e6ca2484b65a340a991f5d1e45ebbe977f527`. Inspect `InitializeV2F`,
  `GetMToonColor` and all fragment uses of the vertex output.

## Data ownership and GPU traffic

The original imported primitive remains immutable. The worker resolves current
vertex identities through `CreatorAppearance.source_vertices`, then follows the
same full corner layout as positions, UVs and retained normal frames. Mixed
ordinary/colored objects and generated cut caps get identity white where no
color authority exists. The optional snapshot stream is linear float RGBA.

The stream occupies a separate 16-byte-per-vertex sub-block in the static
geometry VBO, after the compiler-sized base and optional normal-frame blocks.
Ordinary objects without colors allocate no color block. Attribute location 10
has an explicit white constant when disabled. Checked `GLsizeiptr` arithmetic
covers each block and their sum; driver upload errors invalidate the cache.
Immutable geometry identity includes the appearance owner, so stereo, scalar
feedback, material and clip-only changes reuse the existing GPU colors. There
is no production readback, host mapping or GPU wait added by this feature.

## Independent consumer fixtures

`source-color-render` imports a real generated GLB with nonwhite and zero RGBA,
sRGB base texture, original authored normal/tangent targets and one anisotropic
joint. It compares actual Document/GL pixels against an independent
cgltf/PNG/GLSL consumer for raw, adopted-UV and painted cases; Opaque/Mask/Blend;
and morph weights 0, 0.35 and 1. Its original colors survive native restart.
MToon 0/1 ignore tests and material-aware alpha rays use separate assertions.
Actual `glBufferData`/`glBufferSubData` wrappers verify allocation/upload bytes,
second-eye reuse, material edits, mask scalar isolation and clip uniforms.

`source-color-vrm` adds fixture-authored vertex colors to a copy of the CC0
nineteen-part avatar's original GLB and imports it normally. Original source
files remain unchanged. It separately materializes native legacy-MToon and
native PBR fallback views and compares both with the same exported VRM's
independent consumers. Exact per-corner RGBA, UV, culling and independently
posed positions are checked before pixel comparison. The fixture includes
original/native-only morph identity, skinning, UV adoption and the previously
scoped low-level canonical normal subset.

Release software results are 100 color-renderer assertions and 41 actual-avatar
consumer assertions, with maximum channel error 0 against the independent
consumers. Actual Intel Arc A770 results pass the same 100/41, with maximum
channel error 1 and 0 respectively. The source-color buffers use actual measured
GL allocation/upload counts; the hardware fixture refuses software renderers.
The 40 CPU visibility assertions pass; the color fixture additionally covers
the corrected Mask cutoff-zero edge case. Existing Arc renderer regressions pass 47 frame,
81 MToon, 43 emission, 38 mask, 25 visibility, 24 buffer reuse, 22 Document buffer
reuse and 19 ordinary-capacity assertions.

Khronos validates all 34 generated GLB/VRM files with zero errors; its seven
warnings are the existing legacy VRM extension-name warning. The official 15
VRM0 schemas validate all seven VRMs with zero errors. Hardware-generated files
match the validated software files byte-for-byte. A separate whole-core C/C++
ASan/UBSan build also passes both new fixture suites (100 color-renderer and 41
avatar assertions, 2/2 CTests in 101.70 seconds), with leak detection enabled and
no sanitizer diagnostics. This is focused fixture coverage with the complete
core instrumented, rather than all registered native tests. Exact local source, binary, output and log
hashes are recorded in `docs/validation/SOURCE-COLOR-CHECKPOINT.json`.

These are independent consumer proofs, not evidence that all legal glTF
attributes or every possible bake is representable. Existing atlas ownership
and canonical-normal export guards remain active.

## Required follow-up

The importer currently rejects morph target attributes beyond POSITION,
NORMAL and TANGENT. Modern glTF also permits color and texture-coordinate morph
channels. Supporting them requires preserved raw channel identity, stable
source-target/current-native-slot mapping, current posed RGBA in picking, and
matching independent consumers. UV morphs need original sampling coordinates
and per-corner transport after atlas changes; copying source UV deltas onto a
new atlas would be incorrect. No unsupported channels are silently discarded.

General high-to-low baking is also separate. If a source vertex color is baked
into a texture while the target still applies its own vertex colors, the result
is multiplied twice. Dividing by target colors cannot solve zero components or
overlapping target UVs with different color authority. The next viable route is
an explicit, reversible preparation that rasterizes the current static vertex
color component into a writable nonoverlapping atlas, retains original source
and undo provenance, uses identity current vertex colors, and reports bounded
resampling error. This needs a versioned material/archive receipt; it must not
silently whiten the original asset or claim lossless conversion. General
deforming canonical-normal export remains the separate documented task in
`NORMAL-FRAME-EXPORT-STATUS.md`.
