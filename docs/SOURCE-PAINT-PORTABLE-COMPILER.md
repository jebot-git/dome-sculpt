# Explicit portable painted-material compiler

Private d8870e6 extends qualified b2ca53b with original Source4 material baking
and sparse linear paint evaluation. It does not enable ordinary painted Source4
export or claim lossless conversion of independent sampler/LOD semantics.

The UV baker now distinguishes genuine logical UV0/UV1 from PBR-generated image
carriers. A sealed read-only coordinate accessor avoids constructing sparse tile
packets on the asset worker. Sampling plans deduplicate only actual identical
image/coordinate-owner/transform/filter interpretations. Unchanged writable
atlases preserve every sparse layer owner, byte, ID, mask, opacity, visibility and
active selection instead of re-rasterizing paint. Fixed geometry/layer staging
is reserved before raster allocation; image/work/raster/layer refusals preserve
the original source. No existing public class layout or archive is appended.

The separate bake_source_paint API produces a finite RGBA8 linear/clamp portable
view. It composes the original material with correctly filtered premultiplied
paint contributions, rather than stepping through byte paint cells. Its PBR and
MToon role views share the explicit portable filtering policy; untouched original
samplers remain in source/preparation data. This transient result is not a native
Scene authority. Native owner packaging and explicit policy remain required.

Four complete changed/test translation units are rebuilt against immutable own
734/fdc support/render and whole5f Scene/Document archive replacements. Final
component builds pass Release49.83s/SAN100.73s, all3103 inputs exact each. This
is not a fresh current native frontend/all-target build.

The lit compiler fixture passes192 checks in Release, owned ASan/UBSan and
strict actual A770. Eight material cases exercise16 authored/PBR comparisons:
UV0 and genuine UV1, generated carrier2, transformed MToon shade UV0/UV1,
MatCap-only, untextured and genuinely UV-less sources, normal/AO/MR images and
actual color/metal/rough/emission layers. Independently removing emission or
replacing metal/rough fields visibly changes the lit reference. Maximum visible
mean is0.350887 and interior maximum1, below the unchanged2/2 bounds. Pixels on
the declared discontinuous edge footprint can differ255; no all-pixel2 claim.
Each explicit compiled PBR GLB independently renders within2. All24 GLBs across
Release/SAN/Arc are byte-identical and validate with0errors/0warnings/42infos.

The sparse CPU point sampler passes42 checks per profile against actual GPU
five-plane composition, independent double arithmetic and257 filtered points per
field variant, including PBR/authored colour and tiny/HDR physical coefficients.
The existing4e-6 relative bound remains unchanged. It allocates no dense float
composition image and introduces no runtime GPU readback. Arc is GL component
proof, not a new OneAPI/CUDA/ROCm execution claim. Both software --hardware
controls correctly refuse; all1563 hardware-run inputs remain exact.

Twenty-two old registered commands pass on21 matching test-object relinks:
Release CTest23.19s/wrapper26.47s and ownedSAN138.07s/160.12s; all3144 inputs exact
each. Original raw source-UV98 plus adoption consumers pass on14d in both
profiles, including all19 parts of both CC0 baselines. These precede only the
later callback-only source change and are not new full skinned-avatar workflows.

The earlier failures are retained: straight image conversion gave mean5.966 and
interior3; sparse composition fixed the paint step but inconsistent MToon role
filtering still gave mean3.04805. Their actual fixes retain the original bounds.
An additional real128-square checker at a small model footprint still has
mean26.0125 in software/25.6375 on Arc, maximum106. The test explicitly detects
this loss and verifies strict export still refuses without source mutation.
Flattening independent sampling into one portable image remains a format/view
limitation, consistent with the [glTF image/sampler model](https://github.com/KhronosGroup/glTF/blob/main/specification/2.0/Specification.adoc).

The next integration must retain the authenticated native Scene alongside its
explicit portable view, then connect actual import/append, file worker and UI
workflows. Repeated UV/domain composition, Source4+HDR, full VRM/skin/production
fixtures, acceleration/device coverage, controller drawers and every unfinished
REQUIREMENTS row remain required. No system, headset or active session changed;
main remains0b5ada0. See validation/SOURCE-PAINT-PORTABLE-COMPILER.json for exact
source boundaries, commands, manifests and preserved negative cohorts.
