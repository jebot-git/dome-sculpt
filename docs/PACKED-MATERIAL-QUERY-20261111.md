# Native packed material-query qualification

Private source `2ef16028` includes product `38c7577d`/`ad73d6e9` and test
`7eecb416`/`c84d14f7`/`2ef16028`. It is based on frozen owning candidate
`44be6d0d`; that candidate, its providers and main were not edited. This is a
query/preview milestone, not full layered painting/material/baking completion.

The earlier original 4.8M and separate 9.6M publishers failed because their one
MaterialSurfaceQuery inherited standalone SurfaceQuery's per-mesh 2M structural
caps. The private PackedMaterialQuery retains the complete original Mesh for
material, UV, color, paint and global ID authority. It builds bounded internal
native BVHs over compact referenced geometry. Standalone caps remain unchanged;
small inputs delegate the original math, traversal, budgets and tie behavior.
All original unused attributes, indices, faces and authoring bounds are validated.

Resource admission precedes source/part copies: 512 MiB retained, 768 MiB
retained plus build peak, 128 parts, 262144 triangles/part and 4194304 BVH nodes.
The forecast includes full original optional arrays, compact parts, reserved
order/node storage, caller material metadata, sorted-reference scratch and the
new 8-byte/node triangle-ID sidecar. Construction cancellation never publishes
partial acceleration or retains worker callback captures. Exact source callback
cleanup from `db56e6fb` is retained.

One native visited counter spans all parts. Legacy unfiltered closest keeps its
2M default; explicit filtered and mixed queries retain 65536 total triangle
tests. Results/callbacks use original global triangle IDs and binary64 distances.
Aggregate exact ties prefer earliest global primitive; existing small tie behavior
is deliberately unchanged. Genuine sculpt chunks retain their separate namespace
and share the mixed triangle-work counter.

After fixing the structural cap, the real original indexed case exposed another
failure: second-object direct contact exhausted 65536 tests while traversing nearer
excluded owners before reaching its target. New private material-policy hints
prove only that an original triangle interval contains no eligible object/material.
An 8-byte min/max sidecar over actual post-build BVH order conservatively retains
gaps and boundary nodes. Spatial AABB pruning precedes this hint. Arbitrary
triangle/barycentric filters still spend the original shared budget; alpha,
dynamic UV and source appearance never masquerade as coarse eligibility.
Unfiltered and visibility-only paths use no hint. Known policy routing has a
262144 metadata-work budget, charging interval lookups, binary-search comparisons,
range visits and exclusion comparisons. Typed partitioned and sculpt routing share
that same counter. The new policy-comparison charge applies only to partitioned
inputs, preserving original small and small mixed behavior.

## Actual GPU store evidence and retained failures

The trace-only `8113a1e8` consumer leaves the original assertions unchanged and
records real owning buffer names, Data/SubData sizes, source-array addresses and
VAO attributes. Ordinary snapshots contain canonical tangents, even without
retained source frames. Attributes 0/1/2/5 bind one actual VBO: positions 12 +
normals 12 + UV0 8 + canonical tangents 16 = **48 bytes/vertex**. Material
DYNAMIC/null pools have different buffer names and are excluded from geometry.
No private paint-coordinate stream or invented source-frame arrays are present.

| Original input | Actual vertex store | Actual index store |
|---|---:|---:|
| 4,800,012 ordinary vertices / 3 independent actors | 230,400,576 B | 72 B |
| 9,600,024 ordinary vertices / 6 independent actors | 460,801,152 B | 144 B |
| 1,083,603 indexed vertices / 6,480,000 indices | 52,012,944 B | 25,920,000 B |

The earlier renderer-recovery receipt's 153.6/307.2 MB figures were unrendered
forecasts omitting canonical tangents. This actual trace corrects those figures;
the historical source, failed assertions and receipts remain preserved. The
separate six-actor case still proves a real >256 MiB requested vertex store, with
its small old owner plus new stores below the test's 512 MiB endpoint envelope.
The flat renderer's product geometry ledger defaults to 1 GiB; the separate
sculpt arena retains its 512 MiB hard bound. The test envelope is not a new or
changed product cap. Earlier sealed wording conflated these distinct limits;
its original archive remains intact.

The corrected forecast `7eecb416` preserves all inputs, picking checks and exact
pixels; both profiles then expose the genuine indexed selection-budget failure.
`c84d14f7` fixes that failure, but review identifies an unintended new metadata
charge on small per-face exclusions. `ad73d6e9` restricts it to partitioned
inputs; the final native test compares 65000 small coplanar triangles with eight
nonmatching object/material exclusions against actual standalone SurfaceQuery,
including its exact tie. Earlier failures and intermediate qualification scopes
are retained separately, rather than attributed to the final source.

## Focused qualification

Each profile compiles two complete owning TUs (`mesh-processing.cpp` and
`material-visibility.cpp`) and two complete consumer TUs, then replaces exactly
two members of the authenticated current 192-member core. The other 190 members
remain pinned. Serial matched links and source/provider/output hashes are checked.
This is an affected-ownership closure; it does not claim a whole application build.

| Final source 2ef16028 | Release | Sanitizer |
|---|---:|---:|
| Independent native query/typed material controls | 61 / 0.92 s | 61 / 4.99 s |
| Original 4.8M and indexed 6.48M capacity/picking/pixels/recovery | 64 / 3.31 s | 64 / 21.34 s |
| Separate 9.6M / >256 MiB requested-store case | 63 / 2.05 s | 63 / 7.03 s |

All six runs pass, no source/provider/output drift or external guards. Native
controls retain prospective memory/node/part rejection, unused data validation,
construction/per-call cancellation, exact near-distance/ties, original UV/alpha,
all object/material exclusion facades, mixed namespaces, the shared 65536 budget
and explicit metadata refusal. The generic 70k rejection predicate still exhausts
65536 across partitions. Capacity retains every real original actor, worker picker,
global/material/logical IDs, analytic barycentric reconstruction, direct contact,
exclusion, nonblank baseline, exact pixels, unchanged-eye zero allocations and
real GL allocation-failure/accepted-frame recovery assertions.

Runs use explicit software GL, actual llvmpipe LLVM22.1.8. Sanitizer uses
`detect_leaks=1:halt_on_error=1`, strict UBSAN and no suppressions; no diagnostics.
Upfront native deadlines remain 180 s, capacity 240 s, RSS 4 GiB Release / 8 GiB
sanitizer; highest observed sanitizer RSS is about 2.36 GiB. No hardware, headset,
GUI, install or session launch is claimed here. An independent read-only source
review found no blocking finding. Parent whole owning/UI/XR/hardware/P1 admission
and the original full painting/material/baking targets remain separate work.

The [machine receipt](validation/PACKED-MATERIAL-QUERY-20261111.json) and hashed
companion archive retain raw build/runtime/failure logs, current source patches,
source snapshots and orchestration. ELFs remain in the local artifact directories;
the compact archive intentionally contains no binaries.
