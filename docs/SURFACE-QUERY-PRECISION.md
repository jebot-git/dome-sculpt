# Nearest-surface precision repair

Nearest-surface queries now retain valid thin-face interiors and their actual
source barycentrics. The old kernel throws `Ill-conditioned closest triangle`
for the valid face `(0,0,0), (1,0,0), (1,1e-10,0)` when querying its interior.
This affects a shared geometry primitive used by projection, weight transfer,
baking and remeshing.

One complete `mesh-processing.cpp` provider changes. Double cross-product plane
projection avoids the cancelling Gram determinant. Exterior queries compare
closed segments using their in-plane distances, so a large common off-plane
distance cannot turn different thin-face regions into a winding-dependent tie.
Filtering and returned hits share the actual computed barycentrics instead of
reconstructing them from rounded positions. Bounds and range squares also use
double differences/products before rounding. Public interfaces, layouts and
scene formats are unchanged.

Each Release/sanitizer profile passes the original counterexample and complete
native mesh-processing, remesh, rig and bake consumers. The mesh suite includes
54 thin-face scale/aspect/axis/winding families, edge/vertex/filter/range checks,
and sub-float-range distance bounds. An independent oblique-plane oracle adds
108 scale/aspect/axis/reflection/winding families per profile. Its permanent
`surface-query-oblique` target also passes actual registered CTest 1/1 per
profile using manually built matching executables. No sanitizer diagnostic or
protected-input drift is reported.

All consumers use a freshly compiled whole provider and ABI-compatible c179
support. Link maps exclude the old mesh-processing archive member. This is
component qualification, not a fresh whole-project build, application GUI test,
accelerator test or physical headset test. No installation is performed.

The initial negative counter, a harness filename collision, and the first
revised kernel's exterior-region failure are preserved. The collision overwrote
its own intermediate provider object with a consumer object; that failed build
is not qualified. Both earlier source bodies were restored and checked against
their recorded hashes. The final successful cohorts retain their distinct
whole provider objects and consumer executables. See
[the receipt](validation/SURFACE-QUERY-PRECISION.json) for exact inputs and logs.

The full production goal remains active. This repair does not close the separate
polygon paint/normal transport, controller transaction, texture-set or broader
authoring/device requirements.
