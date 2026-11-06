# Unequal source/current texture grids: coordinated remaining adapter

This records the agreement with the field-source owner. It adds no executable
sourceful transition or permission flag. Current sourceful/PBR/baked resize guards
remain correct and active.

The immutable original Paint, its original raster/resolution, original39 normal
packet, geometry, logical source corners, role UVs and before N/T owners remain
unchanged. A new independently authenticated current writable-grid owner must
carry the actual member domain, new R, real raster, resampled complete layers and
an explicit mapping to the original source corners. Membership and unchanged
Mesh alone do not grant that relationship. A same-address foreign control block,
stale recipient, equal pixels or numeric outer file version cannot mint it.

Existing TextureSetFieldSource::validate_field and
TextureSetPaintAccess::valid_source_member require equal grids. Existing read_view
reuses the original raster/attachment. Those contracts must keep refusing unequal
R; neither an accessor nor a boolean exception can make them the new adapter.

A minimal future immutable descriptor needs two explicit tuples: original source
owners and current writable-grid owners. Accessors should return the actual owners
and dimensions separately (for example original_grid/current_grid), with a private
measured transition constructor and actual self/control-block authentication. A
source report can expose both real dimensions and provenance for diagnostics;
its presence grants no permission. No dummy accessors are added in this checkpoint.

The material/query/render preview must then sample current color/ORM/emission and
coverage from the new grid while obtaining original intrinsic normal/role-UV and
before-N/T inputs from the immutable original packet. It needs a real dual-grid
producer and cache key, including current COW pixels, domain, grid and both owners.
Calling the old read_view or relabelling new paint as the original raster would
silently keep old colors or validate against the wrong dimensions. Original
baked images remain source samplers; they are not renamed new baked normal fields.
Prepared PBR/projection and independent FLOAT/mask/layer planes need the same
measured, unit-correct current-grid transition and quality bounds.

Paint7 transport needs explicit current-grid references/records in addition to
actual field_source seed IDs. Preserve exact geometry/domain/raster/control-block
aliases over both current and retained PolygonSources8 roots. Original seeds keep
their original resolution and material/node prefix authority. Current grid records
must be replayed by the actual reader, with separate quotas, reachability and
cycle/depth checks. No inline duplication or equal-value owner merging is allowed.
Undo/Restore uses exact retained grids, not inverse resampling.

Before opening this capability, require actual original-R64/current-R128 positives
with N-only and prepared normals, independent source/paint/role UVs, reflection and
original winding, true layer/FLOAT/mask contents, real render/query/GPU sampling,
Save/fresh reopen/current+retained identity, Cancel/Undo/Redo/ordered Save, stale
and foreign-owner negatives and measured working/quality bounds. Source export
retains its independent genuine source sampler. The field-source/export owner is
implementing separate work; this design does not alter its frozen source child.
