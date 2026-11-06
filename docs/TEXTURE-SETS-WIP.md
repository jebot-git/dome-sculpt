# Native texture sets: partial implementation

Pinned original base: e79dbe9cbe3243b17b224843c247987b8f2595f8. Qualified query
corrections c3a1b19/5b02bfc are merged. Scene40, Paint7 and polygon_sources8 are
reserved; Material22 is available only if a new connected envelope is needed.
Shared controls are reserved page31/count32. The feature is private and incomplete.

The first actual gate passed on source984de42: a fresh optimized Release app,
core, support, render and XR build (225 steps,439.549s), followed by32 native
model/codec controls (one CTest,0.01s). All210 project objects and11 archives
were freshly compiled.1476 source,483 canonical SDK and223 owned binary hashes
had no drift. The app was built, not launched. No sanitizer, UI, renderer or
hardware execution is claimed. Exact evidence is in
validation/TEXTURE-SETS-INITIAL-RELEASE.json. Earlier syntax diagnostics and
source-only checkpoints remain in validation/TEXTURE-SETS-SOURCE-WIP.json.

A set is a persistent material slot on one native mesh instance. Set0 retains
the existing object's geometry/paint/material/source fields. Extra records own
independent writable layers,14 channels/F32, material/source context and bake
recipe. Selecting a set never swaps it into the object's primary paint field.
There is no global name-based sharing or implicit UDIM requirement.

The current model uses immutable shared topology and exact face membership.
Additional painters are shared_ptr<const PaintedMesh>; edits create a detached
COW draft and seal another snapshot before replacing the collection. History
snapshots and duplicated instances cannot acquire a mutable extra-paint alias.
The per-call graph ledger counts live slots separately from unique retained
domains, and charges shared geometry/assignment/ID owners once through actual
material and polygon/shape/rig before-state edges. Complete source/raster/cache
working-budget and failure qualification remains required.

The initial connected codec supports plain current meshes, including legacy
and F32 paint. Per-set Paint7 payloads contain no mesh; one shared mesh table
is referenced by distinct face/corner identity records. Standalone paint decode
refuses Paint7. Aggregate Scene preflight charges the grids and shared geometry
before SurfaceMap construction. Executed controls cover independent fields,
shared geometry, duplicate-instance isolation, native file Save/reopen, COW
history snapshots, context/foreign-ID/orphan refusals and cancellation. These
are not Document Undo/Redo or actual worker publication tests.

Original PolygonFace.material numbers remain original metadata. New membership
is keyed by persistent face IDs and does not reinterpret those numbers. The
polygon-ID association gap is now independently reproduced against984 and fixed
in769ce32: a forged raw marker can no longer bypass the genuine current polygon
authority. The matched two-TU component passes six polygon controls plus the
original32; its app/test link maps exclude both old archive members. This is
component scope, not a second full build. See
validation/TEXTURE-SETS-POLYGON-IDENTITY.json.

The public/private layouts and mesh_owner ownership changed. No old project
objects or archives are compatible by size. Every future layout addition needs
a fresh matching consumer closure. Normal/color/source authority remains
independent; membership cannot authorize a normal frame or receipt.

The following remain required:

1. Genuine source/normal/recipe-backed sets and retained polygon_sources8 codec,
   source/material/image closure, feature recursion, quotas, append and recipe
   remapping. Current complex and retained-history codec paths explicitly refuse.
2. Subset render/query/stencil proof with explicit logical corners and winding.
   All sets remain visible/occluding while only the selected set receives edits.
   Conflicting frame/paint/source attributes require complete corner signatures
   and necessary boundary splits; EBO ranges alone are insufficient. Rendering
   currently refuses texture sets until the actual adapter exists.
3. Independent active-set Bake/Clear and measured all-channel/F32 resolution
   transfer. Painted resolution changes currently refuse rather than discard data.
4. Multi-primitive glTF/VRM compilation under the original single mesh/node with
   exact skin/IBM/morph/source identity, plus corresponding genuine import.
5. Native Document, FLTK and XR selection/assignment/layer/bake controls, immutable
   leases, one Undo, Cancel/stale/ordered Save, fault/resource and quality proof.
6. Exact UV/topology producer remaps, including material-boundary face creation.
   No nearest-position matching, averaging or silent unsupported-owner fallback.

Ordinary native weight capture refuses unrepresented membership until its real
multi-material retention and resource adapter exists. No main/default promotion
or full texture-set completion is claimed.

The initial fresh Release cohort and old marker counter are preserved unchanged.
The later component rebuilt both complete changed production TUs and its whole
model consumer, then linked the app and consumer before the immutable984 core.
No public/private header changed, no archive was rewritten or copied, and all223
original binary hashes remain exact. Current total phase178,298,880B is below
the384MiB cap; the component/counter increment48,394,240B is below48MiB.

Current source-only continuation includes main537c98b through real merge9ac0af7.
Its Document header changes make the prior984/769 app and support past-scope;
they are not runtime inputs for new UI/layout consumers. The new private
TextureSetSurfacePlan retains one geometry/DF owner, partitions an EBO with
explicit logical corners, captures immutable fields and bounds preparation/REST
replay. It allocates no per-set Mesh or pixel/image copies. The source test has
new plan controls, not yet executed. Per-set native preview cache metadata is
now isolated at bind; same-frame/no-op image/cache evidence is still required.
The three-TU syntax-only result is in validation/TEXTURE-SETS-PARTITION-SOURCE.json.
The plan is not yet attached to drawing/query/native controls; full complex
attribute ownership and all previously listed workflow adapters remain required.

The next source boundary appends SurfaceRange.texture_set and its private range
proof pointer, and adds material-query object+set identity, logical-triangle
lookup and per-material exclusion. Old whole-object exclusion remains separate.
Private plans/range proofs are truly const factory nodes with exact control-block,
object/set/material/maps checks. This is not yet a complete packed-stream proof: the
actual layout/producer must bind and authenticate the EBO, logical-corner and
necessary attribute-split maps before surfaces can be enabled. Four syntax-only
TUs passed; no new execution or old-layout archive reuse is claimed.

Source-only worker continuation cf5c672 now incorporates mainf907's direct
immutable upload path. A worker-specific Scene bundle holds one genuine
geometry/deformation owner, membership partition and logical corners; it leaves
the legacy ObjectSurface Mesh empty instead of copying it per set. Document
source now emits every nonempty set range and material-aware picking. Private
geometry-only layouts/cache keys exclude paint/maps/collection values. Necessary
welded tangent conflicts split only their actual vertices. Range proofs bind
object+set, actual field/material/map owners, EBO/corner/UV correspondence and
explicit reflected winding; renderer material caches use full object+set keys.
Legacy single-material/export surfaces still refuse this unfinished feature.

The source audit also found that filtering stroke candidates on a shared full
raster was insufficient for coincident faces with identical UVs. Each set now
builds its own membership-filtered raster over the same immutable mesh, keeping
original logical triangle/barycentric IDs and retaining authored COW grids.
Paint7 reconstruction installs actual membership before building its raster.
Empty material slots remain valid. Global admission counts every implicit live
material slot, including volume objects, before staging another set. Raster,
geometry/output/query and per-field preview forecasts are bounded; measured full
request/process/fault qualification remains required.

The final14-TU syntax pass is terminal0 with no input drift. The new registered
surface fixture covers coincident-field isolation, codec/writable reopen, real
worker packets, same-object material occlusion versus whole-object exclusion,
owner/stream negatives, tangent boundary splits and reflected picking. It has
not been executed. A new fixture's missing complete-type include produced a
preserved syntax failure before correction. These source checks do not extend
the executed984/769 scope. No current linked/runtime/UI/SAN/device claim is made.
See validation/TEXTURE-SETS-SURFACE-SOURCE.json and the unlaunched bounded plan in
validation/TEXTURE-SETS-NEXT-RELEASE-PLAN.json.

Active-set native edits, shared FLTK/XR/page31 controls and transactional
Undo/Cancel/ordered Save are still unfinished. Current generic paint/layer
commands are not an active-set workflow. Source/normal/canonical/retained-domain
and corner-colour adapters, retained polygon_sources8/history/recipe closure,
independent Bake/Clear/resolution transfer and true multi-primitive import/export
remain required. A fresh matching whole reachable cohort is mandatory after the
ObjectSurface/private-raster boundary; original project archives are not reused.


The first frozen722 fresh optimized CPU gate compiled all231 reachable steps,
including app/core/support/render/XR and both consumers. It passed model32,
polygon6 and partition24; the new surface fixture correctly refused its bare
SourceAuthored constructor before creating any set. The exact original failure
remains retained. The additive11cf producer now creates genuine initial material
membership on a new empty actor before constructing independent Strict rasters
over one detached Mesh. It keeps all original UV/depth/normal/index data and
never supplies a source-authority flag. Existing layouts and inline ownership
remain unchanged. A matched two-whole-TU component plus whole consumers excludes
both old core members; it retained a later Strict same-grid overlap refusal.

Finaldbc907 fixture-only follow-up passes all four direct modes: model32,
polygon6, partition24 and surface72. The same-grid merge remains an exact
model/native owner/history rollback negative, with actual subsequent Save/reopen.
A valid new-set reassignment preserves the same input data while testing cache
invalidation. Coincident independent-field painting, owning codec/writable
reopen, material occlusion, owner/stream negatives, tangent boundary splitting,
reflected picking and the actual Document surface worker pass. All protected
source/SDK/old/new binary comparisons have no drift. See
validation/TEXTURE-SETS-INITIAL-OWNER-RELEASE.json. This is scoped CPU component
evidence; the app was not relaunched with11cf and no SAN/UI/GL/device proof is
claimed. Active-set workflow and all complex/native/import/export rows remain
required in the separate continuation.


## Native selected-field source continuation — 2026-11-06

Source7ae8bd4 carries the qualified initial membership producer by real merge
and adds owning selected-field native painting, typed accepted worker controls,
material-visible logical face picking, shared page31, and ordinary FLTK controls.
A strong immutable recipient lease protects actual target/collection/source/pose
and group/volume authority; accepted drafts replace only the requested field.
Scalar material/layer values are staged until Apply, keeping raster work off UI
publication. The new actual Document/Cancel/Undo/Save fixture is registered but
has **not executed**. Sixteen whole own-source/frontend/fixture TUs passed syntax
checking, with final label-only frontend checks; no object/link/app/runtime proof
is claimed for this appended layout. See TEXTURE-SETS-NATIVE-SOURCE.json.

Fresh complete owning Release gate is proposed separately; no old-layout project
archive reuse is permitted. Complex per-set source/normal/recipe/retained-history
adapters, all-channel resolution/UV remap, independent real bake targets and
shared-node multi-primitive glTF/VRM import/export remain required. This source
checkpoint and earlier scoped plain Release controls do not complete texture sets.


## Fresh native selected-field Release qualification — 2026-11-06

Frozen cdfadd18 now passes a fresh236-step optimized CPU Release cohort, with
all234 own object/archive/app/test binary inputs built from this layout. The
ordinary application version command exits0; five native CTests pass: model32,
polygon6, partition24, raster/worker/query72 and selected-field native45. The
last mode executes genuine face picking and typed material/layer workers,
source/pose/collection leases, preview ownership, Cancel/retry, multi-sample
painting, one Undo/Redo and ordered native Save/reopen. Source, canonical SDK and
owned binary hashes are unchanged. Exact evidence is in
validation/TEXTURE-SETS-NATIVE-RELEASE.json.

All three configure-only sparse setup failures are retained. Full tests/tools
were materialized before the successful frozen build; product bytes did not
change. Cumulative fresh phase151,797,760B includes those failures, below the
additional256MiB cap including64MiB prospective serial-link reserve.

This qualifies the named CPU native workflows and application build, not an
actual FLTK window or GL/OpenXR/HMD/device interaction, sanitizer or full-suite
production test. Root remesh44fe is merged separately in the export child, which
requires a new owning cohort. Complex owners, full per-set baking/resampling,
import/export, production fixtures and main integration remain required.
