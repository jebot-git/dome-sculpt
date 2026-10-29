# Agent handoff — persistent baked surfaces and native object transfer

Repository: https://github.com/jebot-git/dome-sculpt, main. This stage extends
0c266de; its complete voxel handoff is archived in HANDOFF-0c266de.md. Read
AGENTS.md, REQUIREMENTS.md and TOOLKIT.md. The full production objective remains
active/incomplete: keep all named sculpt/texture/UV/rig/avatar/plugin-equivalent,
full glTF/VRM and backend/device requirements. No agents were spawned here.
Remote handoff agent names refer to other workspaces, not live handles. Public
commits/pushes are authorized; fetch and preserve remote work before overlapping
changes. No installation or active-session restart occurred.

## Implemented data and worker path

- PaintedMesh owns an optional immutable baked base under its editable paint
  layers: color/alpha, ORM/AO, emission, optional +Y normal and linear coverage.
  Rebaking blends covered texels in linear light and preserves protected ones;
  it cannot discard an existing normal channel. Paint/erase/local stroke history
  retain the base. Snapshots, global history and ordered native saving retain it.
- UV reparameterization filters base maps with premultiplied coverage and
  reorients normals through old/new canonical tangent frames. UV rotations and
  mirrored handedness preserve object-space detail. Editable geometry IDs, UV
  flags, paint layers and deformation identities remain intact.
- Scene v5 is emitted only when baked data exist; older v1-v4 projects remain
  readable and unbaked scenes still use v4. Binary lengths, resolution and baked
  subversion are checked; v4 cannot masquerade as a baked scene. Maps plus paint
  tiles share 128 MiB/object, scene paint storage is capped at 256 MiB with the
  existing 16 Mi texel cap, and native files remain capped at 256 MiB.
- Document::bake_from_object(sourceID, ObjectBakeSettings, AssetSource) queues one
  guarded detached Scene operation. Target is the active visible/unlocked
  prepared painting mesh. Hidden/locked sources may be read; their displayed
  pose/morph mesh or scalar surface is sampled. Source and target transforms are
  reconciled in target-local space; cage/AO controls are world millimetres.
- Target geometry, material, paint-layer and rig data survive. Targets whose
  displayed geometry/normals differ from rest currently fail explicitly. Misses,
  overlaps, cancellation, invalid input and stale scene/source stamps publish no
  partial result. Success is one exact Undo/Redo transaction with worker stats.
  bake-authoring.hpp/cpp contains the detached operation; document-assets.cpp
  queues it. No renderer/UI/model callback does the bake itself.
- The preview uses linear normal maps and AO for indirect lighting. Meshing
  workers generate/expand render-only tangent corners without changing rest
  topology or skin/morph IDs. GLB adds normalTexture and shares its ORM image
  with occlusionTexture. No Python/runtime toolkit dependency was introduced.

The native end-to-end regression paints a real sculpt, creates a 60 mm voxel
sculpt, generates its 512px atlas, bakes from the retained source, verifies exact
source/target preservation and Undo/Redo, saves/reopens v5, exports GLB and waits
for the normal-mapped render snapshot. Run dome-sculpt-bake-workflow-test
--artifacts <directory> to retain projects/GLB for inspection. Default CTest uses
an owned temporary directory and preserves failed fixtures for diagnosis.

## Tangents, projection and provenance

MikkTSpace is pinned at 3e895b49d05ea07e4c2133156cfa94369e19e409. Keep its Zlib
notices and per-corner outputs; averaging onto existing vertex IDs breaks seams.
The default baker and GPU agree on interpolated T/B/N and its full inverse,
rather than assuming the interpolated frame is orthonormal. The explicit
imported-tangent path retains its prior orthogonalization semantics; broader
import/deformed-consumer tangent comparisons remain production work.

Instrumenting the C dependency exposed upstream UB: both quicksort seed
rotations could shift a 32-bit value by 32. The plainly marked minimal fix masks
the right-shift count in both paths. Original/patched SHA256 and the reproducible
0001-defined-seed-rotate.patch are in third_party/sources.json. Thirty vendored
hashes and a clean patch reapplication were checked. Do not suppress UBSan or
omit C instrumentation when checking this dependency.

Curved-normal rays exposed shared-edge projection holes. SurfaceQuery now
adapts the pinned Blender 5.4 Alpha isect_ray_tri_watertight_v3[_precalc] with
double intermediates and bounded distance; BVH slab differences use double too.
Tests cover oblique shared edges across .001/1/100 metre scales and reject real
outside gaps. Existing parity-based voxel filling is unchanged. Detailed API,
contracts, limits and source roles are in BAKING.txt and the manifest.

## Validation

Final source verification is recorded in VALIDATION.md. Logs and generated
projects/captures remain ignored under artifacts/ and build/.

- After the seed fix, all 24 existing Release CTests passed in 259.98 seconds:
  artifacts/tests-bake-maps-verified.log. Real-loader XR report: 53 checks,
  complete=true/status=passed, zero failures. CPU/software GL/synthetic tracking.
- The subsequently added native bake-workflow target is checked separately;
  artifacts/tests-bake-workflow-verified.log (8.93 seconds) and the retained fixture run
  artifacts/tests-bake-workflow-final.log cover the full sequence above.
- Six whole-core ASan/UBSan/leak suites, with BOTH C and C++ instrumented,
  passed in 165.24 seconds after the seed correction:
  artifacts/tests-bake-maps-sanitize-verified.log. The new end-to-end workflow
  separately passes in 80.36 seconds:
  artifacts/tests-bake-workflow-sanitize-final.log.
  Earlier failed UB runs remain diagnostic evidence, not passed verification.
- Existing native GUI 23/23 and universal menus 50/50 pass:
  artifacts/gui-bake-maps-final.log and artifacts/menu-bake-maps-final.log.
  A new v5/baked-asset GUI fixture separately opens the real baked sculpt,
  paints over it, undoes to exact GPU pixels, and opens/cancels its owned UV
  modal (5/5): artifacts/gui-baked-surface-final.log and
  artifacts/gui-baked-surface/report.json.
- Independent cgltf plus PNG decoding check normal/AO bindings and unchanged
  linear normal bytes, including retained skin/morph export. Actual OpenGL
  tests reconstruct reference curved-normal shading and verify indirect AO.
  Captures were inspected. No fresh external Khronos validator is claimed.

The workflow's first color assertion exposed inadequate test paint buildup,
not lost bake data: a single falloff stamp was translucent. Setup now accumulates
eight real stamps in one stroke and verifies a blue source before baking; the
strong destination-paint assertion remains. The initial GUI run used that weak
fixture and timed out; the corrected retained fixture passes. New GUI reports
reset on startup and distinguish running/complete/failure to prevent stale passes.

Canonical toolkit remains DomeD742d009 FLTK1.4.5 with 22 patches in
artifacts/toolkit. Use its lib directory first in LD_LIBRARY_PATH for GUI tests.
Linkage in artifacts/linkage-bake-maps-final.log has no Qt/GTK/Pango. No physical
headset/controller/stylus/3D-mouse or accelerator validation occurred here.

## Resume next — preserve the whole objective

1. Connect the guarded bake API to FLTK and XR: an explicit source picker,
   world-distance/sample/padding sliders and fine wheels, status/progress/cancel,
   preview/rebake/clear and eventual cage editing. Preserve universal-menu
   ownership, modal isolation, Cancel-default Enter, Ctrl+W/Escape and BDF style.
   XR must remain joystick-spawned/driven with controller-sized ray sliders.
   The native API/data tests above do NOT establish those unfinished controls.
2. Expand source/target fixtures and production atlas quality, editable masks,
   stencils/projection/texture sets, multi-source baking and supersampling.
   Keep existing target layers and the full retained source; never flatten or
   strip unsupported import attributes silently. Source reuse is not permission
   to drop the full requested authoring feature set.
3. Add real large-asset timing/memory evidence. Upstream Mikk sorting/grouping
   has no cancellation hook: checks bracket the bounded call. Render-only corner
   expansion and dense map snapshots have costs; global history is still capped
   by 128 entries rather than a complete retained-byte budget. Do not claim
   production memory/cancellation/VR timing proof from the small fixtures.
4. Continue full rig/weight/morph/avatar/MToon/glTF/VRM and the named plugin
   workflows, plus real device/session/accelerator verification. Rest-target
   baking and static normal-map byte validation do not establish full posed
   skin/morph consumer shading. Read the unchanged full requirements matrix.

Inspect live process handles/logs before starting/restarting tests. Preserve
immutable render snapshots, bounded/ordered input, job generation/revision and
selection/content guards, ordered saves, accepted UV transfer ordering, exact
cancellation/history and all prior DomeD/voxel/UV/recovery work.
