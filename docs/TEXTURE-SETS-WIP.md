# Native texture sets: source implementation in progress

Pinned base: e79dbe9cbe3243b17b224843c247987b8f2595f8. No build or test claim. Qualified query corrections c3a1b19/5b02bfc are merged;
The first plain model/codec slice now has a fresh optimized Release app/core/
support/render/XR build (225 steps) and 32 executed native model controls.
No app UI, renderer, sanitizer or device execution is claimed.
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
Additional painters are sealed `shared_ptr<const PaintedMesh>` values: immutable
collection/history snapshots retain them, and edits create detached paint and
collection replacements. A per-call ledger now bounds live slots separately from
unique retained domains and charges shared geometry/assignment/ID owners once,
including polygon/shape/rig origins and actual material before-state edges.
Pixel/source/raster accounting still needs its connected Scene integration and
new runtime/fault proof; this source ledger is not a qualification claim.
Original `PolygonFace.material` values remain original topology metadata; the new
slot table is keyed by actual persistent face IDs and never reinterprets those
old numbers as a new source/material authority.

The public/private sizes changed. Every actual product consumer requires a fresh
coherent build. Old support overlays are not qualified by outer class size.
The shared geometry owner now lives independently from raster caches. All affected
sampling seals, cache lifetimes and owner guards require new runtime proof.

The next source checkpoint adds the initial plain-mesh Scene40/Paint7 codec:
current set payloads omit their mesh, one owning topology table supplies the
shared geometry, standalone readers refuse Paint7, and Scene preflight includes
all extra grids and membership data before SurfaceMap reconstruction. Genuine
polygon IDs are replayed against the current triangulation. An immutable edit
operation snapshots before mutation and seals a second COW snapshot before
publication; failure, unfinished strokes and cancellation discard that draft.
The source test is registered but has NOT been executed.

This first connected codec explicitly refuses source/normal/recipe-backed sets
and retained texture-set history. The reserved polygon_sources8 path is not yet
implemented. These are missing adapters, not supported completion exceptions.
Subset rendering is still guarded, so this is not a usable UI feature yet.

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

A subsequent source refinement splits immutable mesh arrays from face/corner
identity records in the owning geometry table. Distinct topology/instance
records can refer to one genuine shared mesh owner without duplicating its
vertices, normals, UVs or edit flags. The unexecuted fixture now includes a
second independently editable instance with the same geometry owner. Six-TU
syntax checks still pass; actual Release execution is the next finite gate.

The first actual Release gate is closed0 on source984de42: configure5.089s,
fresh own build439.549s, one CTest passed (32 controls, test0.01s). Every
project object/archive was freshly compiled in this cohort:210 objects,11
archives, app and model-test ELF.1476 source,483 canonical SDK and223 owned
binary hashes had no drift. The model checks COW history snapshots, independent
F32/layer/material fields, duplicate instances sharing geometry, actual native
file Save/reopen, membership/context/foreign-ID/orphan refusals and cancellation.
This is not Document Undo/Redo or a usable texture-set UI yet. The original
polygon-flag association gap is separately recorded for the next source repair.
See validation/TEXTURE-SETS-INITIAL-RELEASE.json for exact evidence and scope.
