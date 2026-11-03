# Independent original sampling and writable paint ownership

This is a **private data-layer checkpoint**, not application admission or full
production completion. Runtime begins with `5d74b0e`, Scene closure with
`08932be`/`819a2cc`; resource correction is `17f5dd0`, test fixture correction
`59fc945`, registered persistent-failure oracle `6987cf6`. The following merge
of published `db8fc94` changes documentation only. Exact evidence and retained
negative results are in `validation/SOURCE-PAINT-OWNERSHIP.json`.

## Actual ownership and persistence

`prepare_source_paint` takes the real pristine original PaintedMesh, an exact
UV-only target and its ordered source-vertex correspondence. The existing
sampling transport retains original images, sampler/filter/wrap/transform data
and logical UV1/extra sets; it allocates a genuine independent carrier for raw
UV0 rather than resampling or changing a sampler. The new strict UV0 grid owns
writable sparse paint. A private immutable SurfaceMap attachment holds the
pristine COW snapshot, exact remap and actual current source-map owner. There
is no owner cycle or value-equality substitution for original provenance.

Only the factory can attach its freshly produced maps directly. The reader
independently replays and compares their original sampling. A bare alternate-UV
PaintedMesh does not acquire writable authority. Current layer IDs, metadata
and active selection are preserved. Real begin/stamp/end/cancel and local
undo/redo are exercised, as are Scene snapshots and native CBOR Save/load.

Conditional Paint4 contains its current legacy payload, pristine original
payload and little-endian source-vertex map. It uses the actual shared image
archive. Standalone records carry that archive once. Older decoder overloads
refuse Paint4 even when no paint tile is allocated. Scene28 supplies the typed
capability only for an actual independent-source attachment; a complete Scene28
without that feature refuses. Current, retained material/UV-origin, shape and
rig capsules participate in version selection, validation and decode preflight.
Original source TEXCOORD identities are authenticated through the remap; a
new generated carrier is never asserted to be an authored original accessor.
Legacy ordinary Paint1/2 and Scene output bytes remain unchanged in the controls.
Physical-emission Paint3 capability is separate and is not granted by this code.

## Resource correction

A preserved real diagnostic built both 2048-resolution grids with the existing
explicit 512 MiB producer option, saved Paint4, then failed to reopen because
its reader hardcoded 256 MiB. One common numeric forecast now covers original
and current geometry, raster/padding peaks, original coordinates, detached
current-coordinate replay, remap, sparse tiles and reserved sixteen-layer slot
arrays. The existing producer maximum of 512 MiB is the common ceiling. The
smaller caller default remains 256 MiB; it still refuses the large test case.
The forecast also checks the existing 128 MiB retained paint-storage ceiling.
Saving refuses inconsistent oversized states. No image resolution, sampler,
visual tolerance or legacy paint-storage cap was relaxed.

Prospective strokes now include retained original geometry/coordinates/remap
and existing projection/bake storage before publishing new sparse tiles. A
real 100,000-row source at resolution1024 accepts eight full paint layers; the
next stroke refuses with exact whole-stamp rollback. The last accepted state
reopens, and undo/redo plus reopening still work. The old runtime accepts tile
storage while exceeding the retained-storage ceiling, which the oracle detects.
This numeric forecast is not an RSS/allocator-overhead or complete process-memory
measurement; broader linked-source-raster, archive-wide and GPU budgets retain
their separate bounds and require continued production coverage.

## Verified scope

- Owning standalone26 and Scene74 checks pass in optimized Release and owned
  ASan/UBSan/leak configurations. Scene tests cover empty/painted current,
  retained material, shape and rig states, actual CBOR Save/reopen, downgrade,
  malformed/forged owners and exact feature selection.
- Final resource24 checks pass in each profile. The prior unpaired-UV1 fixture
  failure is preserved; the fixture now uses an actual UV1 normal image view.
- Twelve selected runs per profile pass after a preserved harness output-path
  failure and an unchanged-object relink/rerun of the final Scene regression.
  These include source sampling47782, initial source UV123, source UV1,
  source UV Bake using both nineteen-part CC0 bases, adoption, extra-UV closure,
  paint and Scene. They are not a full registered CTest pass or fresh app build.
- Registered final fault tests exercise all643 measured persistent allocation
  boundaries per profile: prepare45, encode486, decode109, stamp3. Every denial
  preserves original/current bytes and permits a complete deterministic retry.
  All nine measured preparation cancellation points prevent publication and
  permit retry. Injection covers stamps, not allocating end-stroke publication.
- The actual public native application refuses all eight Scene28 outputs with
  `Unsupported scene version`. The later resource-fix outputs remain byte-exact
  with those original eight and between Release and sanitizer profiles.
- Every finite proof records source/artifact inputs and checks for drift. None
  reports drift or a sanitizer diagnostic. The changed paint TU is combined
  with the matching earlier eight-TU Scene/archive/kernel cohort and unchanged
  owned support. Public PaintedMesh/TextureMaps layouts are unchanged here;
  this is not the HDR runtime's new TextureMaps ABI.

## Remaining application work

Unpainted preview uses the exact original-source path. Painted preview,
composite/standard-export flattening, material-aware alpha picking, native import
and authoring menus, retained normal/MToon adapters, and combined bake/UV/rig/key
workflows still require their actual source-aware implementation and evidence.
This checkpoint deliberately does not expose the incomplete route in the native
application. The separate immutable GPU-overlay packet/compiler work must be
integrated with fresh coherent frontend/render/support builds before doing so.
Untextured/MatCap-only initial capture and broader source/raster/work budgeting
also remain required. No physical headset, GPU, system installation or session
restart was performed by this checkpoint. Controller drawers and every unfinished
original requirement remain required.
