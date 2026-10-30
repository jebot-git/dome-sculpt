# Authenticated local meshing and geometry identity

The producer and GPU consumer are combined with six-worker native plane-cut
authoring in `cf89a81` (test-only follow-up `91ceb04`). The implementation and
proof below describe its earlier pinned foundation; current combined checks are
in [VALIDATION.md](VALIDATION.md). Whole-map stamp/history copies, complete
packed-array assembly and geometric-edit VBO replacement remain open production
bottlenecks. Paged storage and resolution controls are separate ongoing branches.

This private follow-up combines `37e38c7` local extraction with the complete
`20c132d` mask/symmetry, 24-byte Sample and five-worker Document milestone.
The merge checkpoint is `092dd8a`; no shared primary files or installations were
changed. The final feature commit and source/binary hashes are recorded in the
artifact manifest after validation.

## Actual geometry producer

`CowMap::owner()` exposes a strong const owner. A writer must detach while that
owner exists. `Volume::geometry_owner()` identifies the SDF map; cache consumers
also compare voxel size and physical distance band. Strong owners prevent
allocation-address reuse from impersonating an earlier geometry state.

Each actual stamp derives regions from keys whose computed float value differs
from its prior value, including positives, creation and removal. Regions are
bounded within 16³ sample chunks, keeping distant reflected supports separate.
A region event contains at most 128 chunks. An immutable journal holds at most
64 shared event records; overflow requires a full rebuild.

Events contain weak parent/source map owners. Control-block comparison works
after intermediate maps expire and cannot confuse reused allocation addresses.
It avoids retaining 64 potentially huge SDF maps. A query from the cached strong
base follows the exact current ancestry and coalesces regions into at most 128
chunks. A missing branch, expired history, unknown source, region overflow or
context replacement returns `Rebuild`. It never guesses from a revision number
or a recent brush bounding box.

All map, displacement and journal allocations are prepared before publication.
End-stroke cleanup and layer recomposition also stage their allocations before
changing authoritative state. Undo/Redo append real changed-key events on their
new COW maps. Mask/settings/Fill/Invert and their history retain the exact geometry
owner. Name/lock-only layer changes avoid geometry recomposition.

Cancel restores the starting map, displacement and journal pointers without
allocating. A cache from before the gesture can reuse that restored owner; a
cache from a canceled preview branch safely rebuilds if its base is absent from
restored ancestry. Native decode/from-samples replacement starts unknown history.
The metadata is ephemeral: no archive version or persisted field was added.
`geometry_snapshot()` still carries full authoring data, including masks/layers,
plus its journal pointer.

## Cache and Document adapter

`LocalSurfaceCache::update(const Volume&, Cancel, LocalSurfaceUpdateReport*)`
queries the producer internally. There is no public unchecked dirty-box update.
It returns the same cache for geometry-identical data, updates only authenticated
dirty tiles, or rebuilds safely when ancestry/capacity cannot support a local
update. Limits and cancellation also apply to fallback builds.

The retained source is a typed `SurfaceGeometry` containing only the SDF owner
and sampling context, with the same sampling/gradient arithmetic as Volume.
It does not retain any Volume masks, displacement layers or history. A lifetime
regression proves that a retired displacement map expires while cached SDF
geometry remains readable. Retained sample-map bytes are estimated explicitly
and included in the cache's byte budget; allocator overhead and externally held
immutable snapshots remain separate from process RSS accounting.

The existing ordered mesh worker caches by object ID within a generation and
prunes invisible/inactive sculpt objects. It publishes a candidate cache only
after full materialization succeeds. A failed/canceled job retains coherent prior
cache entries and cannot publish stale snapshots. Existing material, paint, UV,
pose, skin, retopology, picking, mask overlay and worker-shutdown paths remain.
No sixth worker was introduced.

`MeshSnapshot::sculpt_meshing` reports actual local updates, rebuild/fallbacks,
reused/rebuilt tiles, scanned samples, evaluated cells, extraction time and full
materialization time. `worker_ms` measures preview-through-packing publication
work, excluding queue wait and preceding weight-gesture candidate preparation.
Geometry-identical mask publications report zero extraction cells and zero
materialization. Scene mesh copies, world packing and picking can still cost
O(surface), and geometry changes still materialize a complete mesh.

## Static geometry token

`MeshSnapshot::geometry_identity` is a strong opaque token for a separate renderer
static/scalar buffer split. Empty tokens retain conservative legacy upload
behavior. The producer compares dependencies, never fingerprints packed arrays:

* Generation and ordered visible IDs, including empty objects.
* Strong immutable SDF map owner plus voxel/band, or
  `PaintedMesh::mesh_owner()` aliasing the immutable SurfaceMap mesh.
* Immutable deformation, original appearance and material-binding owners. The
  normal-frame registry is inside the immutable binding holder.
* Exact world/group transform components and canonical corner-expansion flag.

The token retains these dependencies strongly, so pointer reuse is safe against
ABA. Mask/control, color painting and material-factor-only changes retain the
token when dependencies match. Geometry, transform, UV/layout, deformation,
binding or generation changes invalidate conservatively. A token is accepted
only with the current noncanceled snapshot. This is not per-tile GPU uploading;
renderer traffic/pixel validation is a separate milestone.

## Current verification

Fresh CPU Release build: **66/66 suites passed, 117.17 seconds**, including
**77 foundation checks** and **94 authenticated integration checks**. The full
UI/XR-off suite covers ordinary document/recovery/assets, masks, UV, deformation,
weights/skin, Retopo, Scene compatibility, native text and export regressions.

Fresh whole-core C/C++ ASan/UBSan/leak build: **21/21 selected suites passed,
118.60 seconds**, including both new fixtures and existing document, mask,
worker, Scene, deformation, skin/weight, Retopo, glTF and atlas cases. MikkTSpace C
and xatlas C++ are instrumented too. Flags are `-O1 -g1 -DNDEBUG
-fsanitize=address,undefined -fno-omit-frame-pointer`; runtime uses
`ASAN_OPTIONS=detect_leaks=1` and
`UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. No findings.

The independent whole-map oracle checks every changed sample against producer
regions. Whole-surface extraction independently checks local output after long
strokes, skipped revisions with expired intermediate maps, Undo/Redo, cancellation,
reload, voxel replacement and distant symmetry. Actual grouped Document scenes
are compared in world coordinates. Allocation faults cover journal preparation,
Undo/Redo, layer recomposition, mixed net-zero End cleanup and actual worker
publication; Cancel and ancestry query are checked with allocations disabled.
Real paint pixels change while the static token stays; UV and transform changes
invalidate it. Retained source lifetime, mask-only work counters and generation
replacement are covered.

Evidence is in the parent artifacts directory:
`tests-local-auth-release-all-final.log`,
`tests-local-auth-sanitize-final.log`, their `build-local-auth-*` logs, and each
`build-auth-*/Testing/Temporary/LastTest.log` assertion transcript. CPU-only builds
do not verify GPU buffer traffic, DomeD appearance, GUI, Wayland or OpenXR. The
combined renderer milestone requires its own follow-up validation.

## Settled Document timing

Three sequential nice-15 CPU Release trials on the Intel Core i7-12700, GCC
16.2.1 (Red Hat 16.2.1-2), C++20 `-O3 -DNDEBUG`. Each starts a fresh real Document
with a 0.6-metre sphere at voxel size 0.025, then submits 24 alternating local
clay/carve steps through begin/stamp/end_desktop and waits for each current mesh.
The inactive fault-instrumentation fixture evaluates the same CPU brush kernel.
Every accepted packed mesh is independently compared to whole-scene extraction.

| 24 settled edits, milliseconds | Trial 1 | Trial 2 | Trial 3 |
| --- | ---: | ---: | ---: |
| Public API through current mesh ready | 1768.40 | 1728.16 | 1750.58 |
| Accepted worker preview/packing | 987.329 | 960.012 | 976.821 |
| Authenticated local extraction | 356.447 | 339.943 | 350.712 |
| Explicit full materialization | 554.476 | 543.744 | 550.357 |
| Independent corrected whole-scene extraction only | 6454.17 | 6367.65 | 6307.82 |

Every trial reports 24 local updates, zero full rebuilds, 543 rebuilt tiles and
3348 reused tile instances. Public API wall time includes brush/COW work,
metadata, worker scheduling, full assembly and packing. Reference extraction is
outside that wall interval and measures extraction alone; it is not a baseline
Document wall-time measurement. GPU upload and rendering are not timed. Logs:
`benchmark-local-auth-document-{1,2,3}.log`.
