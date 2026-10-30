# Local surface extraction foundation

The original foundation is commit `37e38c7`, based on `2c89e8d`. Its current
private follow-up merges mask/symmetry milestone `20c132d` and connects the cache
to authenticated Volume changes and the actual Document worker. See
[AUTHENTICATED-LOCAL-MESHING.md](AUTHENTICATED-LOCAL-MESHING.md) for the current
integration contract and evidence; the initial timing evidence below remains
explicitly historical.

`LocalSurfaceCache` now retains a minimal immutable `SurfaceGeometry`: the
strong const SDF sample-map owner, voxel size and physical distance band. It
retains **no Volume, displacement layers, masks, controls or authoring history**.
Sampler arithmetic matches Volume at/sample/gradient. Retired authoring data is
therefore releasable even when geometry is unchanged and the same cache is
reused. Cache bytes now include a sample-map storage estimate as well as tile
payloads. These are retained payload estimates, not a process RSS limit.

The public `update(field,cancel,report)` queries `Volume::geometry_change()` from
its strongly held map owner. It accepts no caller-supplied dirty boxes. Real
mutations publish bounded changed-key sample regions atomically with the COW map;
skipped revisions coalesce through control-block ancestry. Unknown, replaced,
expired or over-capacity history rebuilds safely. Mask/settings edits retain
geometry identity. Full `geometry_snapshot()` authoring semantics are preserved.

Extraction uses fixed 8 by 8 by 8 cell tiles with immutable shared payloads.
Only dirty tiles are rebuilt. Unchanged tile pointers remain identical. A bounded
tile-index copy currently costs O(number of retained tiles); it does not copy
packets, scan stored samples, rebuild unchanged tiles or flatten the surface.
A persistent/sharded index can reduce that remaining update cost later.

Every cell retains original ordered tetrahedron edge requests and candidate
triangles. Shared vertices use exact global lattice-edge provenance; exact-zero
intersections use a point key. Degeneracy and winding are evaluated after global
edge resolution, because independently interpolating opposite edge directions
can differ by a float ULP. Premature cell-local degeneracy tests proved incorrect
in a mixed long-stroke differential fixture.

Normal dependencies include central differences and trilinear interpolation.
Each tile derives its support radius by reproducing the **actual float sample
coordinates** used for its cached vertex gradients, including multiplication,
division and floor rounding at large lattice coordinates. Updates expand sample
regions by the retained maximum radius, also covering new crossings. Positive
samples outside crossing cells can therefore invalidate normals without changing
positions. The maximum is retained conservatively across updates.

`materialize()` remains an explicit cancellable O(surface) adapter for current
Document packing, export and legacy consumers. A merge queue restores global
lexicographic cell order; edge resolution, winding, exact topology completion and
first-used compaction match the corrected reference mesher. Tile packets are
extraction requests, not independently finalized render meshes. Incremental GPU
seam ownership and chunk uploads remain unfinished. Static geometry identity is
now produced for separate renderer buffer reuse; it does not make CPU packing or
full geometry-change GPU upload incremental.

## Exact representable topology

The frozen `2c89e8d` mesher is retained as a separate test-only oracle. An analytic
sphere with voxel size 0.04, radius 9 voxels and lattice offset (-9,0,7) exposed
an inherited defect: 4060 vertices / 8111 faces, including three boundary edges.
Near-zero crossings on different lattice edges rounded to identical positions;
another triangle rounded to three distinct but exactly collinear float points.
Simply dropping that zero-area triangle left a T-junction.

The private reference correction and cached assembler share an exact policy:

1. Resolve identical representable float positions without an epsilon (signed
   zero compares equal).
2. Preserve every nonzero-area face, however small, using double cross products.
3. Record distinct collinear triangle requests. Subdivide the adjacent face's
   longest edge at the exact middle vertex. Convex planar ear triangulation
   retains collinear boundary vertices where a triangle fan would lose them.

This changes indices without moving coordinates or deleting legitimate tiny
faces. The reproduced sphere becomes closed with 4058 vertices / 8112 faces.
A bounded adjacency pass also restores consistently directed face winding when
quantization reverses a tiny face cross product. Its component sign preserves
the maximum total original face area (summed across the component). Closedness evidence is fixture-scoped: completion returns early when there are
no collapsed/cut requests, so it is not a universal manifold validator for
arbitrary scalar fields. A small collinear defect can trigger a whole-surface
orientation sort, bounded by 16 million work steps, 256 MiB orientation scratch,
48 million output indices and 64 splits per edge. Large surfaces can therefore
refuse even when the affected defect is small. This correction is private pending
root review.
Ambiguous
or unsupported exact subdivision refuses atomically rather than silently
publishing incomplete geometry. Limits cover work and per-edge split points.

## Native source evaluation

No external meshing code was copied for this module. The tetrahedron kernel is
the project's existing GPL-3.0-or-later kernel. These pinned primary sources were
inspected for useful architectural practices:

* [Blender PBVH](https://github.com/blender/blender/blob/3a4070ad41ff715f9c2e1a27f1003fbd01b80cd2/source/blender/blenkernel/BKE_paint_bvh.hh)
  separates position/normal, topology, mask and visibility dirty tags, and
  distinguishes shared from node-owned vertices. That separation informs the
  cache/producer contract. PBVH is a mesh hierarchy, not a drop-in sparse SDF
  extractor. Inspected file SHA256
  `9817fe44975a3418eed4c04b77ccd02620a3cdeeb0fb3e4b41c61c1b339f1e65`.
* [Gmsh BDS](https://github.com/jebot-git/gmsh-xr/blob/2ed37a92b4a6862a7ea1ff620ea5e37b8c6070f1/src/mesh/BDS.cpp)
  implements local split/swap/collapse operations on explicit adjacency. It is
  valuable for later topology editing, but replacing this deterministic SDF
  extraction with mutable BDS would change geometry and add an unrelated mesh
  model. Inspected file SHA256
  `0a2f7bafe0462a1f252917df8f67654e4aaae7ea757bab06115894686d0de8f8`.
* [OpenBrush native mesh](https://github.com/jebot-git/openbrush-native/blob/1373d5e839a9720adaeb187064785f9d9d0e8492/src/mesh.cpp)
  builds bounded stroke-local tube/ribbon geometry and transports its frame.
  Its stroke granularity supports retaining independent immutable chunks, but
  it has no matching SDF halo or tile-seam extractor to reuse. Inspected file
  SHA256 `4263b577ef8aca823bb0a7db7606502b6359309acafa7b97651bf4f9f1a151e4`.

## Validation and measurements

Final foundation fixture: **75 Release checks** and **75 ASan/UBSan/leak
checks**, the latter 9.34 seconds under CTest. Both C and C++ core, xatlas and
MikkTSpace were built coherently with the same sanitizer flags (`-O1 -g1
-DNDEBUG -fsanitize=address,undefined -fno-omit-frame-pointer`); runtime used
`ASAN_OPTIONS=detect_leaks=1`, `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`.
No findings. The separate Release regression run passed **6/6 suites in 19.63
seconds**: UV, sculpt geometry, glTF interoperability, baking, atlas robustness
and local surface. No shared application integration or device test is implied.

The frozen legacy oracle is the extraction function from `2c89e8d/src/mesh.cpp`
with its name changed and duplicate `Mesh::validate` definition omitted. Ordinary
well-resolved inputs must match it bit-for-bit. The inherited closure fixture
intentionally differs and independently verifies directed incidence, identical
unique positions and preserved total area. Cached assembly matches the corrected
reference exactly. These are separate correctness claims.

Three sequential CPU Release timing trials on an Intel Core i7-12700, GCC
16.2.1 (Red Hat 16.2.1-2), C++20, `-O3 -DNDEBUG`, nice 15. The fixture uses a
0.6-metre sphere at voxel size 0.025 and 24 alternating local clay/carve brush
steps. All emitted full meshes are compared to the corrected reference.

| Work over the 24 steps | Trial 1 ms | Trial 2 ms | Trial 3 ms |
| --- | ---: | ---: | ---: |
| Local tile extraction | 344.249 | 348.376 | 327.266 |
| Frozen original whole extraction | 6480.77 | 6611.79 | 6238.62 |
| Corrected whole extraction | 6930.69 | 7175.46 | 6647.75 |
| Explicit full assembly | 795.542 | 792.443 | 730.894 |

Initial cache construction took 187.996–202.969 ms. Every trial rebuilt 543
and reused 3348 tile instances across its updates, retaining 20,084,336 bytes
of mesh cache. The median local extraction is about 18.8 times faster than the
frozen original; local extraction plus full assembly is about 5.7 times faster.
These are workstation observations, not application frame-time guarantees.

Dirty-region discovery in the fixture compares maps to establish an independent
correctness oracle. Its scan, brush evaluation, Volume COW detachment and renderer
upload are excluded from extraction timing. It is **not** a production delta
producer. Renderer tile uploads and authenticated dirty producers are necessary
next integration work, so full production local meshing is not complete.

Evidence in the parent artifacts directory: `tests-local-surface-release.log`,
`tests-local-surface-sanitize-final.log`, `build-local-surface-sanitize-final.log`,
`tests-local-surface-regressions-release.log`, and
`benchmark-local-surface-release-{1,2,3}.log`. The final source/binary manifest
and commit are reported in the handoff after validation completes.
