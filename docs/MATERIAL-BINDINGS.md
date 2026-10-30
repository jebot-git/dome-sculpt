# Resolved native material bindings

`SourceMaterialBindings` is an immutable, shared description of the current
material after source UV baking. It belongs to `SculptObject`, `ObjectSurface`
and renderer `MeshSnapshot::SurfaceRange` under `material_bindings`.
`CreatorAppearance` remains the original imported asset and avatar/accessor IDs;
its historical texture indices do not identify the current resolved images.

The current typed MToon descriptor owns UV selection, transforms and animation.
Its explicit eleven-entry role table owns immutable images and samplers; each
view itself has identity transform and UV0 to prevent applying transformations
twice. Absent roles remain absent. Base/Shade aliasing compares current image,
sampler and typed UV semantics. Generic PBR uses the holder for origin retention
without inventing a typed MToon material.

The first `SourceUvOrigin` retains a detached COW paint snapshot, original
appearance, optional typed material and original role table. Repeated baking
keeps this origin, while sampling the current role table. It never reinterprets
historical texture indices as current role IDs. Original UVs, authored tangent4,
skin/morph data, images, JSON and raw avatar bytes remain recoverable.

## Native archive

Scene format 8 is emitted when a resolved holder, dormant deformation or
retopology record is present. The reader accepts complete legacy format-7
material and retopology shapes, including both together; partial declared
feature records are rejected. Plain and baked-only scenes retain formats 5/6. `source_materials` version 1 deduplicates
holders, first origins and retained original paint snapshots by identity. Role
images use the document `PaintImageArchive`; appearances use its original model
archive. A holder records native factor baseline, explicit animation policy,
freeze time and normal-frame identity. Freeze is never an implicit default.

Limits include 256 holders/origins/original paints, 2 MiB typed metadata per
record, 16 MiB aggregate typed metadata, eight million retained original vertices,
and whole-scene 16 million raster texels and 256 MiB sparse/baked paint storage.
Current and original paints share the allocation preflight before constructing
SurfaceMaps. Existing shared image/model and native file limits also apply.
Immutable aliases are charged once within the shared archives. The caller must
validate and serialize the complete candidate Scene before committing history;
per-object success cannot guarantee that all retained originals fit a file.

## Independent paint emission

Color and emission have independent linear replacement coverage, each on its
own composite channel grid. Protected emission restores the original typed role
with its authored factor. Partial coverage corrects the generic PBR composite by
its remaining source contribution:

```
max(0, compositeEmission
       + (effectiveTypedFactor * originalTypedEmission
          - currentNativeFactor * originalPbrEmission) * (1 - emissionCoverage))
```

Emission strength is applied once after this expression. Native factor changes
use the original native comparison baseline; zero baseline components use the
explicit absolute-value rule. Editable paint, retained baked emission and typed
emission with differing images/transforms remain separate. Export uses the same
coverage equation and refuses unrepresentable version conversions explicitly.

## GPU source packing

The renderer stays within sixteen fragment texture units. One RGBA8 texture
buffer packs independently sized original PBR color, original typed emission,
original PBR emission and linear emission coverage, including required mip levels.
Original immutable images remain ordinary GPU textures. `glGetTexImage` copies
those levels into a bound `GL_PIXEL_PACK_BUFFER` using byte offsets; the buffer is
then attached as a texture buffer. This is a GPU buffer transfer through an API
often used for readback. There is no host download, mapping, fence wait or CPU
resampling. It must not be described as absence of the readback API itself.

Manual shader sampling preserves wrap modes, nearest/linear and mip filters,
sRGB decode before filtering and linear alpha/data. Mips use the uploaded GPU
chain. LOD implementations may differ within OpenGL's permitted precision;
fixtures compare independent native sampling with bounded RGBA8 tolerance.
Combined original/mip/packed bytes and GL_MAX_TEXTURE_BUFFER_SIZE are checked
before allocation. Metadata offsets and dimensions are range checked. Immutable
caches retain owners and remain unchanged across repeated/stereo frames. Packing
saves and restores all eight GL_PACK states, the pack buffer and affected texture
bindings. Context abandonment skips GL deletion after context loss.

## Normal-frame boundary

The current enum explicitly says `UvDerivative`; it is an interim representation
and must never be relabelled canonical Mikk. Canonical paint-baked normal maps
remain separate. The static conversion is not invariant under arbitrary skin,
morph or sculpt deformation. Normal-mapped deformable adoption stays guarded
until original-frame transport is implemented. Authored tangent identities,
generated original-UV Mikk base and morph directions, handedness and versioned
sampling/frame semantics are required, not an atlas-rest tangent approximation.
Pinned primary-source and consumer fixtures are being coordinated separately
with original-frame transport. This boundary leaves full normal-frame source UV
authoring and full MToon completion active work.

## Dormant source geometry

When volume sculpting replaces the preview topology, the original painting mesh,
source appearance, resolved holder, editable/baked paint and deformation remain
on SculptObject for native persistence and undo. The ephemeral ObjectSurface
marks `source_dormant` and supplies no stale appearance, holder, textures or
deformation to the unrelated generated volume surface. This includes generic
painted source maps without a creator descriptor. GLB and VRM export refuse this
state before writing a file; Scene preflight also catches empty volume objects
that surface extraction would omit. Callers must restore the original mesh or use an
explicit validated transfer workflow. Restoring mesh mode reactivates the exact
original data. Valid dormant deformation is checked against its retained rest
mesh, not applied to generated volume geometry. Newly supported dormant
deformation requires format 8 (or a complete legacy material-format 7) even with
an empty material archive; inactive deformation
in older v4/v5/v6 input still rejects. Source-mode global Undo and Redo preserve
the complete mesh/paint/source/rig state in one history boundary.

Release fixtures verify 98 source-bake checks across 38 actual baseline parts,
30 native adoption/archive/dormant checks, 24 deformation checks, 46 cache checks
including the two-baseline resource profiles, 64 existing production renderer
checks, and 43 independent emission/packing checks. GPU suites pass on both
llvmpipe and Mesa Intel Arc A770. The 43-check suite verifies all eight pack states,
current Base/Shade role independence despite equal historical indices, and the
originally-zero native emission factor rule. A matching full-core C/C++ ASan+UBSan build with leak detection passes the
98 bake, 30 adoption, 40 focused cache and 24 deformation checks. Unimplemented
normal-frame transport is not included in those passing claims.
