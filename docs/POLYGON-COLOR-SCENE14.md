# Durable editable polygon corner colors

This private checkpoint extends the existing native FLTK/OpenXR polygon workflow. Raw source `COLOR_0` converts from the actual visible skin/morph pose into editable polygons, retaining the complete original source. Topology edits, local/document history, cancellation, ordered Save, restart and explicit Restore carry the current corner field together. The full production objective remains active.

## Current authority and persistence

`PolygonAuthority::colors` owns an immutable `PolygonColorField`: initial linear straight-alpha CORNER RGBA samples and bounded convex current-corner provenance. Source owners are shared through edits. Corner seams retain their identity instead of averaging by position. Conversion uses the exact captured native-vertex expansion map; inconsistent evaluated poses at a shared topological vertex are refused with the original source untouched. Existing source triangle-basis and nonconstant dissolve refusals remain.

Scene14 uses polygon archive3. It reads the actual Scene13 placement/capsule representation and legacy archive1/2, deduplicates `color_bases` and `color_fields`, restores shared owners, and checks references, reachability, stable IDs, unit RGBA and finite convex partitions before payload construction. Color and geometry/provenance entries share the bounded aggregate quota. Required Scene14 propagates through active, inactive and retained authorities; older Scene readers cannot silently discard the field.

Scene surface DTOs retain their exact strong topology owner. Validation uses ordered REST geometry before pose/world packing. Retained normal-frame expansion follows its explicit REST remap. Native paint and immutable Document picking consume the same packed RGBA stream as rendering, including held-object preview transforms. The field is not relabelled as raw source data or a fabricated preparation receipt.

## Native workflow

The existing Polygon editor, universal menus and controller pages are reused. Its established diagnostic geometry preview remains solid; accepted rendering uses the actual material and corner stream. Capture, geometry/color preparation and file work run on the existing workers. Local history carries geometry and colors atomically and accounts for corner provenance. Apply followed immediately by Save captures the completed immutable endpoint. Empty geometry retains its complete latest-source capsule and suppresses stale Volume/paint surfaces.

Actual GUI pixel acknowledgements exposed a desktop redraw defect: closing a preview could leave its diagnostic surface visible after the model had cancelled. The canvas now compares against the last actually drawn preview state, so Cancel and Undo immediately restore the material view without requiring another input event.

Generic PBR Automatic-alpha GLB export preserves referenced vertex transparency alongside material/texture alpha. Independent consumers exercise Automatic, Opaque, Mask and Blend for gradients and seams. Production drawing adds no CPU readback or mapping; held motion and the second eye reuse the existing color buffers.

## Final component evidence

Exact source/binary/log hashes and source ancestry are recorded in `docs/validation/POLYGON-COLOR-SCENE14.json`; the complete private manifest is `artifacts/polygon-color-scene14-manifest.json`. Product source is `c166f3b`, feature source `0c890a8`, based on published `63df2c6` plus the actual `ad9e035` placement codec. The later public `2074565` worker recovery/status refinements are not in this component proof and must be preserved during integration.

- Coherent regular-user Release 25/25 passes in 65.48s with canonical `toolkit-eee23b7` and bitmap runtime. All affected core/app/renderer/runtime consumers were rebuilt together.
- Complete own core, bundled C/C++ Mikk/xatlas/ufbx support and renderer O1 ASan/UBSan/leak qualification passes all 25 selected suite names in two frozen-source groups: focused 1/1 in 201.98s, then remaining 24/24 in 217.76s after the complete target build finished. This is not a single CTest 25 invocation. External system libraries are not instrumented.
- All 6,661 measured persistent allocation refusals pass: 4,067 encode and 2,594 decode. Every injected failure must actually occur, preserve authoritative field/basis/paint/source owners and exact source data, and permit deterministic retry.
- Native checks include actual color Document 58, Scene14 archive 27, placement 31, existing polygon Document 118/controller 31/workspace 31 and current REST/remap 24. They cover live skin/morph pose capture, no-op/Undo/Cancel, immediate accepted Save, native Open/restart, Delete-all, prior/full-source Restore, alpha-aware paint/picking and held transforms.
- Actual FLTK/DBusMenu workflows pass 16 normal, 19 compact and 10 original-regression checks. Return defaults to Cancel; real conversion, edit/history/Save and restored material pixels are acknowledged. Screenshots were visually inspected.
- Actual OpenXR loader/stereo GL with synthetic devices passes 42 new color and 41 original polygon checks across eight modes each. Real pressure/joystick paths exercise conversion, extrusion/Apply, exact cancel/no-op/unconfirmed work, rendered focus cancellation, Restore/default Cancel and balanced stereo image/session ownership.
- Independent RGBA consumers pass 558 software and actual Intel Arc A770 checks, plus 100 original source-color checks. All eight alpha/gradient/seam image cases have maximum error zero. Drawing performs no readback/map traffic; the second eye and 20 held-motion frames add no buffer transfers. Existing native paint/allocation, glTF and independent retained-frame/VRM consumers remain in the selected regression set.

## Retained dependency failures

The initial assertion-enabled complete-core sanitizer run passed 15/16 in 236.35s; permanent denial exposed `json::binary` tagging a completed result before allocating its payload. Correction `21e140e` fixes all four factories. Normal/diagnostic assertion-enabled Release 2/SAN 2 each cover six actual boundaries, exact inputs/subtypes/bytes and successful retry; the old-header negative abort remains retained.

The next scene probe reached a later null-to-object promotion failure during MToon decode (106.21s). Correction `c166f3b` fixes nine existing member promotion sites. Normal/diagnostic assertion-enabled Release 2/SAN 2 each cover 42 actual boundaries with valid owner/state, pre-existing data, serialization and exact retry checks. All 11 old-header negative cases abort. Both exact GDB traces remain retained; neither assertion nor fault was suppressed. These fixes preserve successful values/API/layout/bytes and do not claim universal JSON strong transaction semantics. External constructor replacement is unchanged.

## Remaining scope

Prepared color/normal topology changes, current-field baking and appearance-transferring plane cuts still require real projection/frame adapters and remain explicitly refused. Color-aware polygon edge cuts work. Retopology/voxel conversion keeps the complete source object but retains its existing blank/clay output; appearance transport remains required. General affine application, Scene15 normals and Scene16 source roots require coordinated integration, preserving ordered REST color identity and archive3 data. This component makes no new OneAPI compute, complete application matrix, actual DomeD Wayland, physical headset, system installation or public-push claim. Earlier transport-only checkpoint `14ca381` retains its independent historical receipt.
