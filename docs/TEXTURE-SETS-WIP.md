# Native texture sets: source implementation in progress

Pinned base: e79dbe9cbe3243b17b224843c247987b8f2595f8. No build or test claim.
Scene40, Paint7 and polygon_sources8 are reserved. Material22 is available only
if a genuinely new connected material envelope is needed. Shared texture controls
are reserved page31/count32; older private31/32 proposals are not current main.

A set is a persistent material slot on one native mesh instance. Set0 retains the
existing object geometry/paint/material/source fields. Extra sets have independent
layer/channel/F32 state, resolution, material/source context and bake recipe.
There is no global name-based sharing between objects, and no implicit UDIM
requirement. The active set is persistent; switching it must not replace the
object's authoritative geometry or hide the other sets.

Current source foundation has immutable shared topology/face membership,
object-local IDs, original Polygon face/corner IDs, detached COW fork/assignment,
constant-time candidate membership, shared geometry across resolution rasters,
paint snapshot/swap retention, and complete first-origin capture/Restore fields.
Original `PolygonFace.material` values remain original topology metadata; the new
slot table is keyed by actual persistent face IDs and never reinterprets those
old numbers as a new source/material authority.

The public/private sizes changed. Every actual product consumer requires a fresh
coherent build. Old support overlays are not qualified by outer class size.
The shared geometry owner now lives independently from raster caches. All affected
sampling seals, cache lifetimes and owner guards require new runtime proof.

Next implementation stages remain required:

1. Scene40/Paint7 geometry-deduplicated membership codec, strict old/context-free
   refusal, polygon8 before-state retention, complete source/material/image
   closure, quotas, min-version and native append/recipe remapping.
2. Subset render/query proof with explicit logical corner/winding mapping. All
   sets stay visible and occlude stencil/projector rays; only the selected set
   receives edits. Extra-grid raster work and shared geometry/tiles/cache leases
   need truthful aggregate accounting before allocation.
3. Independent active-set Bake/Clear and all-channel measured resolution change;
   existing painted-resolution transfer currently refuses instead of discarding
   layers, physical values, masks, baked fields or normal ownership.
4. Multi-primitive glTF/VRM compilation under the original single object/node,
   with exact skin/IBM/morph and source identity; no duplicate-object workaround.
5. Shared native Document/desktop/XR selection/assignment/layer/bake controls,
   immutable set/paint/geometry leases, one Undo, real Cancel/stale/ordered Save,
   actual loader/UI/export and allocation/resource proof.
6. Exact producer remaps for UV and topology edits, including mixed-set face
   creation and material-boundary operations. No nearest-position matching,
   averaging, raw material-index guessing or silent unsupported-owner fallback.

All old field/source guards and normal/color receipt independence remain. A set
membership seal does not grant any new normal or source-color authority. The
current ordinary native weight capture explicitly refuses unrepresented set
membership until its actual multi-material resource/retention adapter exists.
