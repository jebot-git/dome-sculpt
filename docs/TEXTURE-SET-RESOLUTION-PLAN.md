# Per-set resolution editing: concrete implementation plan

This is an implementation plan, not a resampling implementation or passing
resize test. It follows actual coherentee557 FLTK qualification:19 polygon and51
texture-set workflow checks pass on the complete current Release application.
The manager displays and preserves independent64/128 fields but provides no
resize widget. The current painted-resolution guard remains correct. The
qualified root and closed12c tree are untouched by this sparse child.

## Existing boundaries and reuse

`TextureSetEdit::resolution` already carries a target dimension and Document
validates powers of two64..2048. The action enum lacks a resolution operation.
`TextureSetPaintAccess::bind` can rebuild empty membership rasters, but deliberately
refuses a changed resolution when authored paint/projections exist. Its current
clear-tiles branch must never become a resize implementation for painted fields.
`PaintedMesh::reparameterize` has useful14-channel coverage-associated/sRGB math,
but runs at a fixed resolution and explicitly refuses texture sets, FLOAT,
retained paint and source/projection owners. It cannot be reused as a resize API
by dropping those guards.

Coordinate with `work/texture-set-fields-e412` source6bc73578. That bridge adds
real field-source owners and cached current39 reads, including exact original
paint/attachment, membership and normal/source owners. Its current resolution
must match the original. The proposed plain resampler must not relabel a new
raster as that original, invent SourceAuthored authority, or weaken
`PolygonSurfaceOwner::validate_current`. Bridge record/domain layout changes
require a coherent full-owned build before runtime; currentee557 executables
cannot qualify those new layouts.

## API and transaction

Append `TextureSetAction::Resolution` without changing existing action numbers.
Reuse the request's existing `set` and `resolution`; do not add duplicate editor
state when `TextureSetInfo` already exposes the current size. Add explicit
`TextureSetResolutionOptions` and a bounded report containing old/new sizes,
visited samples/tiles, bytes/work, per-plane reconstruction/coverage errors and
quality status. Proposed detached entry:

```cpp
TextureSetResolutionReport resample_texture_set(
    Scene&, uint64_t object, TextureSetId,
    const TextureSetResolutionOptions&, const TextureSetLimits&);
```

The entry authenticates the actual recipient, selected field, source and sibling
collection, immutable mesh/topology and member-domain control blocks before
allocating. Same-resolution requests are exact no-ops. It returns no changed
Scene on cancellation, quality failure, work/storage refusal or stale authority.
A successful request replaces only the requested field's paint/raster, preserving
all material properties, other fields, face assignments, full mesh/UV/deformation
identities and active-set/layer/high-water IDs. Existing recipient leases and
`queue_scene_operation_locked` provide detached worker execution, atomic accepted
publication, one global Undo/Redo, progress/cancellation and immediate ordered
Save. Exact old data remains in the immutable before/history snapshot.

## Private grid kernel and all authored planes

Extract a private reusable filtering kernel, rather than changing `bind` to clear
paint. It reads immutable tile views and emits bounded COW output tiles plus a
new actual membership raster. Process tiles/rows incrementally; do not flatten
every input layer to a dense image or allocate a full scene copy per set. Forecast
both resident old/new storage and scratch/raster candidates before each phase.
Poll cancellation through extraction, filtering, quality evaluation and sealing.

Preserve every layer, including hidden/zero-opacity layers, its ID/name/order,
active selection and next-ID cursor. Include the entire authored grid, including
values currently masked or outside visible membership; visible geometry is not
permission to discard hidden authoring data. Keep masks independent from channel
coverage. Never multiply the mask into stored coverage/values merely to make a
current composite agree: that destroys independently editable masked paint.

Resample all14 common components with their actual meanings: sRGB color RGBA
(alpha linear), metallic, roughness, sRGB legacy emission, separate color/metal/
rough/emission coverages and the linear mask. Decode sRGB before filtering; filter
channel values associated with their own coverage and unassociate when valid.
Filter mask separately. Reuse/extract existing transfer math only after tests
cover its new footprint, membership and dimension contracts.

Also preserve optional independent PBR-color RGBA tiles and actual FLOAT emission
tiles. `PaintEmissionValue` contains **legacy RGB, physical RGB and float coverage**.
Filter both RGB triples in linear units using authoritative float coverage;
physical radiance may exceed1 and must retain the existing10000 maximum/finite
validation. Rebuild the legacy UNORM8 fallback from the resulting legacy RGB and
coverage so its consistency predicate holds. Do not turn a legacy fallback pixel
into an authored physical plane just because the fallback accessor can supply a
value. Keep optional-plane presence and native units truthful.

Use bilinear reconstruction for magnification and area/footprint integration
for minification, with exact pixel-center and UV conventions. Each operation reads
only its own field's grid even where another set shares UV0. Its raster/padding
continues to authenticate actual member faces and original logical corners.
Filter raw authored planes independently of current material/mask visibility;
measure both authored-plane and actual compositing error so mask/coverage
correlation and sharp patterns cannot pass only a flat-color check.

## Quality, persistence and complete source support

A resolution change is a measured reconstruction, not a bitwise equivalence at a
different sample count. Same-size and constant-field controls must be exact.
Measure per-layer/channel contribution/coverage and FLOAT radiance errors,
including hidden/masked paint, plus independent current-material images and
alpha/cutout behavior. Reject reductions that cannot meet declared bounds with
a concrete diagnostic while retaining exact before data. No automatic flattening,
HDR clamp, dropped layer, arbitrary mask baking or relaxed limit is acceptable.
Save/reopen must reproduce every resulting native plane and metadata exactly;
Undo restores the exact original grid/pixels/owners, not an inverse resample.

Implement ordinary native fields first, retaining explicit guards for sourceful
or prepared owners until their real transitions exist. That is a sequencing step,
not completion of resolution support. The next required adapter separates the
new writable-paint grid from the bridge's immutable original grid and independent
normal/image source resolutions. It must carry and serialize real mapping/owner
witnesses for current paint,39 domain3 and all original normal/T/W/UV owners.
Baked color/ORM/emission/normal and projection planes cannot be omitted or
numerically relabelled to the new dimension. Reserve any new persistent owner
encoding with root and the bridge agent before changing file capabilities.
The original source and normal grids remain immutable; current writable pixels
at the new resolution require their own authenticated transition and Save/Restore
path. Full source-aware/typed/baked support remains in the original goal.

## Existing FLTK/OpenXR interface

Extend the existing material-set manager with an active-field resolution choice
and a specific Apply action, using existing64..2048 values and source-captured
requests. Selecting a different member must remain a separate accepted action
before its resolution is changed. Keep Cancel/Escape/Ctrl+W exact; changing a
widget alone must not start a worker. Use the existing fixed footer/scrolling
layout, DomeD bitmap/theme helpers and universal-menu routing. Expose the same
request through the existing XR texture-set controls, with visible current/target
size and worker status, without new frontends or nested-menu proliferation.
Do not expose a painted resize control until its actual worker is qualified.

## Validation and build gate

Use real64/128/256 grids and both magnification/minification. Cover asymmetric
color/alpha, metallic/roughness, masks, coverage borders, zero and hidden layers,
second PBR color, mixed legacy/physical FLOAT radiance and sub-UNORM8 coverage.
Use independent footprint/contribution oracles, meaningful visible images and
wrong-unit/coverage/neighbor-set counterexamples. Same-UV different-set fields
must remain isolated, and geometry/domain/control-block identities must be real.

Exercise native worker Apply/no-op/Cancel, allocation/work/quality failures,
stale source and sibling mutations, exact Undo/Redo, immediate Save, fresh reopen
and retry. Test ordinary and compact FLTK/XR choices and all loss/focus/close
boundaries. Sourceful cases need actual original-grid and normal-owner positives
and unchanged explicit negatives, not a plain-fixture approximation. Establish
CPU reference results and selected-backend execution/numerical/loss behavior for
eligible GPU work; do not silently execute CPU work in a selected accelerator
build.

This child contains only this plan and the reusable already-qualified GUI test.
No new C++ API, layout, file format, object, archive, executable or system install
is produced. A later build needs a measured resource proposal and coherent
providers including the source bridge; the root's current live/qualified sources
remain frozen. Full requirements, appearance quality and device/interoperability
qualification remain active.
