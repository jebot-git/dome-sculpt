# Painted PBR export sampler consistency

This narrow work extends published78a1034. The actual native renderer keeps
original source color/emission wrapping and filtering for ordinary composites;
prepared PBR vertex-color projections deliberately use linear clamp sampling.
The generic GLB exporter instead assigned linear clamp to every derived image.

The UV0-only fixture uses actual native painting and Document::export_asset,
separately parsed cgltf/PNG/GLSL consumers, artist and generated-flat directions,
POSITION/NORMAL/TANGENT morphs, three alpha modes and two morph weights. Against
exact qualified public78 archives and matching public headers, its first painted
export loses NEAREST/mipmap metadata; the older pixel-only witness reaches an
error3 at unchanged tolerance2. An independent editable-UV1 import case also
exposed an error40. These negatives remain separate local artifacts.

The fix retains the viewport's effective color/emission sampler, resets already
baked coordinate transforms, and leaves ORM/normal/prepared-color sampling and
original source-only textures unchanged. A whole owned-export object overlay
excludes the old archive member and passes121 checks with all24 native/export
image comparisons exact0. A fresh coherent optimized core/render/support build
passes the new fixture plus GLB and existing source-color renderer suites3/3.
Fresh whole-core/render/support sanitizer and exact device qualification remain
pending. This is not full production completion or general UV1 admission.

The common test fixture now includes the two required POSITION-morph bounds;
original float data are unchanged. No system installation or headset/session
change was performed.
