# Sparse floating paint overlay compiler — private component

This component supplies an immutable sparse native paint packet and the GPU
compiler needed by the separate Source28 preview work. It is not full HDR import,
painting controls, MToon rendering, standard export, or an application workflow
admission. Existing guards remain until those routes are implemented. The full
production goal remains active.

Public data layouts append `TextureMaps::overlay` and optional absolute linear
`SourceMaterialBindings::physical_emission`, including meaningful authored zero.
Scene27/material11 carry actual radiance, comparison baselines and retained
before-state closure; ordinary unit output keeps old fields. Raw source factors,
strength and historical no-binding corrections are unchanged.

`detail::PaintOverlayFactory::capture` is the sole packet constructor. It captures
COW common/PBR/seven-float emission tile owners, visible opacity, exact optional
source-map identity and immutable baked base. Padding is a real aliasing
`shared_ptr<const vector<int32_t>>` to its validated SurfaceMap. There is no
current Paint/Scene back-edge or dense floating CPU raster. Baked payloads reject
recursive overlays. Composite retention conservatively charges retained
SurfaceMap/domain geometry and sparse tiles. Empty metadata-only packets require
no output planes or GPU image budget.

The compiler uploads sparse fourteen-byte native pixels in padded sixteen-byte
integer TBO records, seven physical floats in R32F records, per-layer tile indices
and immutable padding. Baked bytes remain existing source data. One fullscreen
GPU operation produces five RGBA32F contribution planes and GPU mipmaps:

1. Premultiplied colour RGBA, separately keyed for authored/PBR interpretation.
2. Premultiplied AO/roughness/metallic, with baked AO coverage in alpha.
3. Premultiplied legacy emission RGB and float emission coverage.
4. Independent colour/metal/rough/emission coverage.
5. Premultiplied absolute physical emission RGB and float emission coverage.

Original source/material sampling is not flattened. The consuming renderer must
combine original radiance times `(1-C)` with legacy paint times authored MToon
strength and absolute physical paint. Source coordinate/picking/native integration
belongs to the parent Source28 branch. That branch's packed-map wrappers and
canonical contribution identities are not silently claimed in this component.

Prospective pointer-only alias planning precedes large packing allocations. Actual
unique records, CPU working scratch, complete mip texels, input uploads, device
limits and resident GPU image/byte budgets are checked. The default packing budget
remains256MiB and GPU budget1GiB. Metadata records are bounded to two colour
interpretations per256 native objects. GL state and partial resources are restored
on failures. No production download, buffer mapping or geometry extraction is
introduced.

Optional MToon source packing accepts an actual compiler Resource/plane owner,
never a fabricated byte Image. Mixed packs use RGBA32F and GL_FLOAT transfer into
a bound GPU pixel-pack buffer, charged16bytes per full mip texel. Original-only
packs retain RGBA8 and4bytes. Source sRGB values remain encoded until the shader;
Khronos EXT_texture_sRGB issue20 explicitly defines this readback behavior.
[Primary specification](https://raw.githubusercontent.com/KhronosGroup/OpenGL-Registry/main/extensions/EXT/EXT_texture_sRGB.txt).
Only tests download float planes/buffers to compare the independent oracle.

Qualification and exact terminal results are recorded in the receipt. Fresh whole
own core/support/render/native FLTK/OpenXR application builds establish coherent
new layouts. Header-dependent continuations rebuild affected code afterward.
There is no claim of an all-target/full-regression, actual GUI/controller workflow,
Source28 semantic union or physical headset test here.

Preserved negatives include the real old planner allocating a large sparse vector
before budget refusal; the initial denial fixture's missing nothrow allocator
interposition; and its subsequent invalid4-byte posix_memalign adapter (the R LLVM
OOM abort and SAN alignment stack remain). Test allocator overloads were corrected,
not suppressed. Known CPU budget refusal now precedes driver queries. These fault
controls establish owned planning behavior, not global driver allocation safety.

Final current-source results: eight affected direct executables pass in Release
and whole-owned ASan/UBSan/leak mode, including background21, owning-scene37,
physical-paint50, GPU compiler/transfer34, source-version22, core-unlit28 and
existing scene/texture painting. First fresh app/core/support/render builds took
494.56s/766.54s; private-header continuations preserve the same public layouts.
The34 compiler checks compare all five actual float planes against an independent
double-precision sparse-layer oracle (maximum relative error2.36515e-7). They also
cover common/PBR distinction, fractional baked coverage, tiny+.005 and10000
radiance, a real pre-allocation failure control, empty metadata, exact source-map
identity, GL-state restoration, cache reuse, recursive-bake refusal and actual
GPU PBO float bits/mips. Source-only RGBA8 packs stay unchanged.

Final actual Intel Arc A770/PCI03 hardware34 passes,0.1227s,1506 input/artifact
hashes unchanged. `LIBGL_ALWAYS_SOFTWARE` was unset and the renderer identity was
captured. The unchanged binary explicitly requested as hardware under llvmpipe
refuses with exit1. The lease was released. No physical headset or user session
was touched. Parent Source28 app/native tests are separate evidence, not folded
into this component's functional claims.
