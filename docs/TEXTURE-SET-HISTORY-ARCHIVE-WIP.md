# Shared texture-set history archive: source checkpoint

This is an implemented source adapter with syntax checks, not a runtime or
production qualification. It is based on field-source codec fc43fa7d. All changed
providers require a new coherent build; existing qualified app/core objects must
not acquire these headers piecemeal.

## Actual ownership

Ordinary current-only TextureSetArchive v1 bytes remain unchanged. An owning
caller selects `shared_records()` before encoding any holder when a real retained
collection requires shared records. Geometry v2 has explicit ordered tables:

- `meshes` and `topologies`: actual shared geometry, logical face/corner order.
- `assignments`: exact shared membership vector owner.
- `domains`: `{topology, assignments, set}`, actual authenticated face-domain owner.
- `rasters`: `{domain, resolution}`, actual SurfaceMap owner, independent of pixels.
- `paints`: `{raster, painted}`, actual PaintedMesh owner and Paint7 payload.
- `collections`: `{topology, assignments, primary_domain, active, next_id,
  additional:[{id,name,material,domain,painted}]}`, actual collection owner.
- `field_sources`: explicit source codec records; empty for the plain history gate.

Each current/retained holder is `{version:2,collection,primary}`. Primary pixels
remain separate from collection identity: capture_polygon_origin makes a distinct
primary snapshot sharing the raster, while retaining the same collection pointer.
Owner matching compares both pointer and control block. Equal-valued distinct
collections/paints remain separate records. No standalone mutable fake object is
used to validate a retained holder.

PolygonSources8 is emitted only for an actual retained collection. It uses an
explicit `bake_recipes` boolean because membership alone grants no recipe
capability. Its member origins store the shared holder in `texture_sets` and null
`painted`; member paint is omitted from the old separate paints pool. Versions1–7
retain their shape. Complete raw reference checks precede payload reconstruction;
reachability is required over every owner pool. Byte/work/cancellation bounds
cover encoded payloads, native grids and member domain vectors.

## Root Scene integration (not implemented here)

1. Collect the complete current and retained graph first; choose shared mode before
   the first current holder if any retained collection exists (or a real source
   codec requires that mode). A numeric outer version is insufficient.
2. Install `field_encoder(TextureSetView, geometryID)` plus the codec records
   callback before writing holders. Pass the same TextureSetArchive to the new
   eight-argument PolygonSceneArchive::encode overload. Serialize geometry last.
3. Pass actual geometry JSON to the six-argument PolygonSceneArchive::preflight.
   Decode geometry once; initialize TextureSetFieldCodec from
   `field_source_records()` and `geometry_at(id)`. Its node geometry callback must
   be installed before material/node replay.
4. Install `field_decoder(payload, actualDomain, geometryID, actualMaterial,
   rasterTemplate)` returning `{paint,source,binding,appearance,recipe}` and the
   source codec completeness callback. Supply real images/sampling/material
   resolver and SourcePaintDecodeContext through the root lambda. Primary binding
   and appearance must match the holder's declared owner. Inline recipe wire value
   must match the referenced seed recipe; adopt that explicit seed owner only.
5. Pass the archive as the last argument of PolygonSceneReader after cancellation,
   or to the new full-context PolygonSceneArchive::decode overload. Decode current
   and retained holders through that same pool. Call require_reachable only after
   all actual roots have been resolved.

The current PolygonScene preflight explicitly refuses sourceful Paint7 seeds and
unrepresented material/source/recipe edges. The sibling source codec exists, but
the full Scene graph/material prefix walk must be extended before admitting those
edges. This checkpoint does not bypass that guard.

## Regression scope and remaining work

`tests/texture-set-history.cpp` uses initialize_texture_sets, real per-set layer
editing, capture_polygon_origin, and typed current/retained archive reads. It
asserts exact alias cardinalities, distinct equal-valued collection ownership,
64/128 resolutions, independent layers, exact re-encoding, quotas, cancellation,
foreign refs, missing capabilities, source-seed refusal and unreachable equal
records. It has only been syntax checked; assertions have not been executed.

Fresh whole-provider build/runtime, full Scene Save/fresh reopen/Undo/Redo/Restore,
sourceful39+40 history/material reachability and SAN/device gates remain required.
In addition, ordinary polygon conversion currently replaces the mesh without
rebuilding/clearing the collection: plain40-to-polygon authoring cannot yet satisfy
its existing membership validation. This must become a genuine production
transition before any GUI Restore claim. Per-set painted resolution editing also
remains unimplemented (see TEXTURE-SET-RESOLUTION-PLAN on branch08b1feba).

## Syntax receipts

Ignored artifact scope `artifacts/archive8-syntax-v2`: archive and PolygonScene
whole TUs pass0 after merging fc43fa7d; the new fixture initially failed because
its source-paint context header was missing. That failure is preserved.
`artifacts/archive8-fixture-syntax-v3` adds that actual include and passes0.
Both runs authenticated408 inputs, no drift. Final child allocated4,444,160 bytes
under8,388,608 cap. No object, archive, ELF, runtime, installation or old-artifact
cleanup was performed. Qualified root and old12c sources remain untouched.
