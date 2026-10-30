# Current polygon corner-color transport

This private checkpoint connects detached, immutable raw `COLOR_0` corner fields to the existing mesh worker, OpenGL RGBA stream, alpha-aware picking and GLB export. It does not yet admit those fields into a saved Scene or remove the user-facing polygon conversion guards. Scene14 authoring, history and restart are the next stage.

## Authority and interpolation

`PolygonColorField` keeps linear straight-alpha RGBA on immutable initial CORNER samples. Topology operations compose bounded convex corner provenance rather than averaging by shared position or topological vertex. Every edit shares the immutable source basis; current corners are detached. The frozen foundation documents exact operation geometry and conservative nonconstant dissolve/retessellation refusals in [POLYGON-COLOR-FOUNDATION.md](POLYGON-COLOR-FOUNDATION.md).

The appended read-only `ObjectSurface::polygon_colors` pairs an immutable topology with its exact field. `detail::current_vertex_colors` validates the complete ordered REST derivative (indices, positions, normals and UVs) before packing. UV pin/seam controls do not change this geometry/color identity. Evaluated skin/morph positions never become a new REST identity. A conflicting prepared color projection, preparation receipt or orphan resolved PBR plane is refused. There is no fabricated source appearance or synthetic Identity receipt.

All validation/packing occurs off the UI thread. The mesh worker uses the shared helper and the existing optional sixteen-byte-per-vertex GPU color stream. The additive four-argument `MaterialSurfaceQuery` takes the exact packed RGBA used by rendering; the original three-argument ABI retains legacy source-accessor behavior. Prepared surfaces explicitly suppress raw tint, including identity slots needed in a mixed packed scene. Raw alpha is checked for Opaque/Mask/Blend with bounded range offsets.

GLB export validates colors against the original REST object before any retained normal-frame corner expansion. `NormalFrameExport::rest_vertices` follows the exact layout mapping to expand the validated stream. The expanded surface is not relabelled as an independently authenticated REST authority. This preserves repeated positions and UV/color seams without position-based inference.

## Completed validation

Fresh Release CPU/UI/XR-enabled app and focused consumers were built against the canonical `toolkit-eee23b7` and bitmap runtime, with no stale archive reuse. Fifteen native/graphics suites passed in 35.14 seconds, including:

- 105 analytic RGBA checks and every 1,340 measured cancellation boundary.
- Nine actual raw GLB capture checks and every 471 capture cancellation boundary.
- 4,596 persistent owned allocation-denial boundaries across colored Extrude, Inset, Cut and Weld, preserving exact source geometry/colors and retry. The count is four lower than the frozen foundation because source-basis copies became shared.
- Original 52 polygon kernel checks and all original 3,132 allocation boundaries.
- 24 current-color transport checks, including actual normal-corner vertex expansion and exact ordered exported REST/UV/RGBA, invalid remaps, cancellation, prepared/raw mixed picking and wrong REST identity refusals.
- Existing polygon paint authority, controller and Document suites (18, 31 and 103 checks), glTF interoperability, normal-frame export, independent PBR and actual VRM frame consumers.

The new independent unlit GLB consumer and actual native SurfaceRenderer pass 419 checks on both software GL and Intel Arc A770. All six combinations of raw gradient/seam fields and Opaque/Mask/Blend have exact image agreement (maximum error zero). Exported float RGBA retains nonzero RGB at fully transparent corners. Real skin and morph channels are evaluated independently. World-space alpha picking agrees with GPU coverage; the second eye and twenty held-object GPU transforms perform zero buffer writes, CPU reads or maps. Existing source-color renderer100 also passes on both devices. These are unlit color/alpha transport proofs, not a claim that arbitrary material/normal projection after topology changes is complete.

Whole-core/support C/C++ ASan/UBSan/leak qualification is pending. A fresh unoptimized Debug run has no memory report but exceeds the existing 60-second default-sculpt atlas acceptance wait in `polygon-document`; its receipt is retained. A separate complete-core `-O1 -g1` build (matching the preceding workflow sanitizer configuration) is running with the identical source and unchanged assertions/timeouts. No complete sanitizer pass is claimed yet for this checkpoint.

## Remaining production work

Persist the current corner authority with explicit Scene14 admission and recursive retained-origin minimum versions; connect polygon workspace preview/local undo/Apply and exactly one document history transaction; verify save/reopen/export/restore and actual native/controller journeys. Preserve the published Strict PA-to-color bridge and single `capture_polygon_origin` helper on integration.

Arbitrary prepared-color topology transport still requires its own current-field adapter: existing source projections, independent PBR/common paint layers, baked planes and normal-frame interpretation cannot be replaced by sampling a raw vertex field. Nonconstant N-gons whose triangulation changes the original piecewise field remain refused until field-equivalence/subdivision proof is implemented. No physical headset launch, installation or external push is part of this checkpoint.
