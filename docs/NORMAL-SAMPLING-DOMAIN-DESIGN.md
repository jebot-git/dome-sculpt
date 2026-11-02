# Retained normal sampling domains: Scene21 working design

This is an incomplete private implementation, starting from qualified
`dd2314b` (product `b0cdeb5`). No existing public object layout, Paint projection,
Scene writer or authoring admission has changed at this foundation stage.
The new pool decoder is structural; it grants no source/editing authority.

## Required model

The writable paint atlas and the normal-field sampling domain are independent.
A genuine UV edit copies sampling coordinates through exact ordered source
vertex/corner IDs. It retains the immutable intrinsic PBR/MToon images and the
exact pre-UV resolved normal images. No new angular tolerance or constant normal
substitution is involved. Repeated domains preserve the first image fields and
their correspondence; geometry or NORMAL/TANGENT changes are not authorized by
this sampling operation.

Existing prepared detail normalizes and quantizes its per-texel blend before
bilinear filtering. Separately filtering intrinsic/detail and blending afterward
would change that model. The retained leaf therefore also carries normal-only
detail authority: PreparedAttributes interpretation, PBR/MToon images and linear
coverage. Its owning before-Paint retains the original baked owner. Deep replay
must reproduce both the old resolved image and the mapped current detail
metadata. A hash or matching pointer alone is insufficient.

Clear-bake selects the retained intrinsic field. Exact unchanged mapped detail
selects the retained resolved field. Any new/partial/repeated bake must create
an explicit versioned field node or quality-controlled compiled field; it cannot
fall through the historical per-texel compositor. A temporary atomic refusal
must remain explicit until that transition is implemented. This refusal is not
completion of the bake requirement.

## Codec contract

Reserved: Scene21, material archive7, normal-preparation entry3. Contracts owns
Scene20/material6/polygon5; sampling and current corner RGBA are separate.
Independent corner-origin key geometry will use the separately reserved Scene22.

- PaintNormalProjection version1 and four-argument raster reprojection remain
  byte-exact. The padding-only version2 experiment remains parked/unadmitted.
- Projection version2 will reference a shared geometry-only NormalSamplingArchive
  leaf. A normal entry3 will own the full before-Paint/material/appearance/
  deformation, exact SourceNormalUvLayout and a distinct domain receipt.
  Raster quality counters are not relabeled as exact-domain proof.
- Scene decode resolves images, then geometry-only domain leaves, then material/
  Paint graphs. Every projection and recipe must reconstruct the same leaf
  owner. Unknown versions, unreferenced/orphan leaves, malformed REST/corner
  correspondence, unbound fields and partial records refuse.
- Limits charge unique aggregate coordinates, indices, coverage and shared
  images, including retained polygon, shape, normal and UV before-graphs.
  Cancellation polls occur in bounded binary preflight/reconstruction loops.
- Legacy bare Paint wrappers preserve version1. They must refuse an unbound
  version2 rather than treating a structural leaf as provenance. A new paired
  standalone surface archive must carry the complete source graph and local
  pool and prove roundtrip, malformed pairing, cancellation and allocation
  rollback. This is required delivery, not an optional follow-up.

## Worker/render/export contract

The worker must pack retained sampling coordinates through the exact existing
corner expansion. They are static geometry data, and the strong geometry token
must include the immutable domain owner and remap. Attribute10 is already
COLOR_0. Attribute11 is available only after checking GL_MAX_VERTEX_ATTRIBS>=12.
The initial intrinsic/resolved selection can reuse the existing normal texture
sampler; it needs no additional fragment texture unit and no GPU readback.
Clear, normal strength/enabled edits, stale work, Cancel, undo, file replacement
and save/restart must keep material/field interpretation explicit.

Generic glTF may express an independent normal coordinate accessor. VRM0's
common MainTex-ST/UV0 convention cannot be assumed to support arbitrary
independent normal/paint domains. Format-specific compilation must preserve all
sources, expose its actual sample-quality/resource limits, retain before_commit
cancellation and prove native/export pixels over multiple poses and morphs.
Schema validation and a rest-frame angular report alone are insufficient.

## Current foundation

`src/normal-sampling-domain.hpp` is a detached owning correspondence prototype.
Its returned domain has the original Paint, intrinsic and pre-filter resolved
fields, exact target REST mesh and source IDs. It supports split/repeated charts
without nearest matching, resampling or contradictory edit averaging. Its
geometry/key admission explicitly rejects changed REST positions/directions.

`include/dome/normal-sampling.hpp` and `src/normal-sampling.cpp` add only new
types: a shared structural leaf pool, explicit PreparedAttributes detail and
strict bounded binary codec. Existing public layouts are not yet appended.
The fixture independently decodes bilinear fields over360 split-chart samples
and exercises aliases, orphan/version/malformed-data refusal, aggregate quotas
and cancellation. This remains foundation evidence; no fractional Scale
workflow, GPU sampling, source replay, new-bake transition, Scene21 persistence
or standalone paired model is admitted yet.
