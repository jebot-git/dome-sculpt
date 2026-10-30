# Scale-robust sculpt atlases

Retaining tiny valid closing faces made a real atlas defect visible: on the
0.04 m voxel, 0.4 m radius sculpt sphere, xatlas rounded 23 of 10,992 faces to
zero UV area and 36 below strict painting's determinant threshold. Deleting
those geometry faces would open the surface; accepting their degenerate UVs
would leave painting undefined.

The wrapper preserves exact metre-space positions, normals, face order and
corner identity through xatlas's original vertex references. Position
normalization uses double area/centering arithmetic, with a bounded float frame
for the parameterizer. An unrepresentable or numerically small generated UV
face receives its own four-by-four texel triangle in a deterministic padded
right-hand band. Only affected faces use those separate charts.

On this fallback path, existing charts are repacked with twice the requested
padding plus a bilinear margin before their U coordinates are compressed. The
reserved-band bound guarantees a compression factor of at least one half;
requested chart padding therefore survives the transform. A new left margin
preserves the outer sampling footprint. Upstream xatlas deliberately strips
outer padding; the unchanged V coordinate retains its original border behavior.

Float-quantized transformed faces are checked again. Newly affected faces are
promoted before publication, and band columns grow geometrically with a fixed
pass bound, clamping the last step to the exact available column capacity.
Rounding nine legal columns to sixteen must not reject an otherwise valid fit.
Geometry, chart capacity, output vertices, dimensions and
final UV determinants are bounded and validated. Capacity failure reports the
requested resolution and affected count and leaves the caller's mesh unchanged.
Strict painting validation remains unchanged. Unwrapping still changes the UV
domain, so existing source normal/morph provenance guards remain required.

Private `atlas-detail.hpp` diagnostics expose actual packed chart identities to
the regression fixture without changing public Mesh or AtlasInfo layouts.
Independent rasterization and per-chart pixel dilation verify ordinary and
repaired chart separation; the test does not merely inspect the band's boxes.

## Validation scope

The initial 16-case native fixture passes in optimized Release and with
ASan/UBSan/leak detection. Its replacement atlas object and test use only frozen
publication headers and the root-verified matching Release/instrumented core,
xatlas and Mikk libraries; dirty primary Volume/Brush headers are excluded.

Checks include the actual sphere at scales 1, 2^-10 and 2^-30; exact ordered
positions/normals and welded watertightness; nondegenerate UVs; independent
four-texel charts/padding; strict PaintedMesh construction; repeat determinism;
and independent cgltf parsing of exact exported corners. A connected planar
chart with 41 tiny slivers refuses a 64-pixel atlas atomically with a useful
capacity diagnostic, then succeeds at 512 pixels under the same strict policy.

Logs: `artifacts/tests-atlas-robustness-release-final.log` and
`artifacts/tests-atlas-robustness-sanitize-final.log`. These focused checks are
separate from the subsequent complete integrated application test run.

The additional dense baking regression uses the unchanged default 0.03 m voxel
and 0.18 m radius source from `tests/bake.cpp`, with `unwrap(mesh,64,2)`. It needs
a final nine-column repair band. Exact capacity clamping preserves every face,
normal, requested chart padding and strict paint validity. The expanded
17-case fixture passes Release in
`artifacts/tests-atlas-column-capacity-release.log`; the corresponding whole-core
sanitizer run is pending at this follow-up checkpoint. The true 41-chart
insufficient-capacity refusal at 64 pixels remains covered and unchanged.
