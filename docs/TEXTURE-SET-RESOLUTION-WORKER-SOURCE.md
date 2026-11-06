# Painted field resolution worker: implemented source checkpoint

This implements the plain worker/API and native controls proposed in08b1feba.
It is a source/syntax checkpoint, not passing runtime, visual, device, allocator
fault or production qualification. Source combines9de9f348 with actual field
bridge/Scene registration7f19ac30. No qualified tree or old ELF was modified.

## Implemented behavior

`resample_texture_set` accepts an actual member and64–2048 power-of-two target.
Same-size is an exact owner/value no-op. The normal detached selected-field edit
seam builds a private draft, validates the full existing collection/Scene, seals
it and publishes only a complete accepted field. Geometry, logical membership,
material values, siblings, hidden layers, layer order/IDs and next-ID cursor are
preserved. New dimensions receive an actual newly constructed member raster,
retaining the exact current immutable geometry and domain control blocks.

The kernel filters every authored texel, including currently uncovered or hidden
values. It uses pixel-center bilinear magnification and exact area minification.
The14 common components preserve separate color/metal/rough/emission coverages,
linear masks, sRGB versus linear units and alpha. Values are associated with their
own coverage during filtering, with independent raw-value fallback when coverage
is zero. Masks are never baked into coverage. Sparse authoring presence remains
explicit. Optional second-color and FLOAT tiles are independent; actual float
legacy RGB, physical RGB and authoritative coverage are carried. Common emission
fallback is rebuilt using the native float transfer/rounding convention. A legacy
sample never creates positive physical radiance. Hidden and sub-UNORM8 FLOAT data
are not replaced by composite previews.

Every layer reports raw and associated channel/PBR/FLOAT reconstruction errors,
mask/coverage errors, normalized physical error and absolute radiance error. The
actual layered material images and replacement coverages also receive a separate
reconstruction check, catching mask/layer correlation that a flat color test
misses. The default maximum component error is.30 and RMS bound.08. Lossy detail
that exceeds the explicit policy refuses with the exact original still held.
Same-size does not run a filter. Constant and asymmetric cases are regression
sources, not yet passing runtime measurements.

Byte forecasts include old authored storage/raster, shared geometry, new raster
constructor/padding, worst-case output tiles, metadata and tile scratch, material
images/scratch and authored candidate storage. Work forecasts include filtering,
quality, actual member raster and material reconstruction. Raster/candidate
constructors also retain their independent bounds. Cancellation is polled through
capture, raster forecast, layers, quality rows and every256 filter samples. No
flattened dense image is allocated for every input layer.

## Native transaction and UI

`TextureSetAction::Resolution` appends without moving existing action numbers.
Document captures the real recipient, queues the existing detached scene worker
and records one complete Undo/Redo endpoint. Its normal ordered Save fences remain
in use. The FLTK material-set dialog adds an explicit resolution choice and
`Resize active set` Apply action. Moving the choice creates no worker; Cancel
retains the document. The selected member must be the active set before resize.

The existing material-set XR page opens a staged `TextureResolution` parameter
at sector6. Its label displays the chosen dimension and Apply runs the same
worker. Layer opacity remains available in the general paint controls. Other
actions cancel a staged resize rather than silently applying it. Existing bitmap
font/theme, universal-menu routing and common modal controls are reused. These
are connected implementations whose runtime/UI qualification remains pending.

## Explicit unsupported transitions

The actual scene/private grid entry refuses source descriptors, bindings,
appearance, recipe, retained sampling attachments, source maps, prepared normal or
color projection and baked data. Source-authored UV policy cannot borrow the plain
route. Real plain polygon-authored fields keep their actual policy with a bounded
candidate forecast. No original39 normal/source grid is relabelled as the new
writable resolution, and no owning validator or schema guard is weakened.

The raw kernel supports independent PBR tiles; the regression uses the real color
preparation producer to exercise that filtering. Publishing a resized prepared
PBR projection remains refused until its actual source-aware transition exists.
Selected non-CPU backends explicitly refuse this newly implemented CPU reference
operation; no accelerator build silently routes it through CPU. Real GPU work,
sourceful/current-grid adaptation and prepared/baked transitions remain required.

## Regression source and checks

`tests/texture-set-resolution.cpp` builds same-UV independent64/128 fields with
actual native color/metal/rough/legacy-emission/physical-emission/mask stamps,
hidden zero-opacity layers and sub-UNORM8 physical coverage. An asymmetric mixed
unit case includes a genuine single-texel color stroke. Independent long-double
2x2 footprint integrals check common components, FLOAT authority and units. It
requires data that distinguishes area filtering, nearest-neighbor copying and
incorrect sRGB averaging. No product filtering helper computes those expectations.

The fixture also covers same-size exact no-op, real raster/domain identities,
sibling preservation, byte/work/cancel/quality refusals, genuine PBR kernel planes
and prepared-grid refusal. Native cases queue Cancel/retry, immediate Save/fresh
reopen, one Undo/Redo and stale source. Actual public SpatialFrame joystick/ray
input navigates the XR page and exercises staged values, Cancel, Apply, ordered
Save, Undo/Redo and focus loss. Assertions have not been executed yet. FLTK pointer
resize/cancel and compact-layout automation still need qualification.

All8 whole TUs passed syntax0 in artifacts/resample-seal-syntax-v4. The final
analytic fixture passes0 inv6; the kernel's added256-sample cancellation poll
passes0 inv7. The other6 product TUs are unchanged sincev4. Each run authenticates
424 inputs with no drift. Last child allocated5,332,992 bytes under8MiB. All
receipts/previous sources are preserved. No object, archive, ELF, link, runtime,
installation or system-session action occurred.

## Bounded integration proposal (unlaunched)

After root's current frozen matched cohort closes, compose currentd1 sources,
7f19 field/history registration and this seal (plus current sibling export if
ready). Fresh complete own R/S app/core/render/XR providers are required; existing
qualified binaries cannot qualify these new sources or callback APIs.

Use executable /tmp, with Release cap512MiB including128MiB prospective and SAN
cap3GiB including256MiB prospective; source plus results cap128MiB. Current root
Release270.9MB and in-progress sanitizer928.5MB measurements underpin the proposal;
/tmp had30.35GB free. Keep root free20GiB +root192MiB +own16MiB and child8MiB.
Run the new fixture plus current native families, actual FLTK pointer/compact and
XR public-input gates, exact Save/fresh reopen, hardware/software material
quality and allocation/cancellation boundaries before any admission. No heavy
build has been started for this checkpoint.
