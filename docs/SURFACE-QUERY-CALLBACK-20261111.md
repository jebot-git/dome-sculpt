# Copied surface-query callback lifetime — 2026-11-11

The copied `SurfaceQuery` constructor retained its build callback inside the
completed BVH, although queries never invoke that callback. An actual native
weak-owner regression confirms that a captured 1 MiB payload remains alive after
the caller releases it. The narrow fix clears that callback after construction,
matching the existing immutable-query constructor.

Product commit `f4f165f2` changes only that constructor body in
`src/mesh-processing.cpp`. Public headers, layout, schema, query math, validation
and budgets are unchanged. [The receipt](validation/SURFACE-QUERY-CALLBACK-20261111.json)
records exact commands, hashes, provider scope and preserved failures.

Release and strict ASan/UBSan each pass 23 lifetime/cancellation/numeric controls,
108 unchanged oblique families and 40 material-visibility controls. Leak detection
is 1, with no suppression. Actual old core prints `retained_after_build=1` and
exits 1 at the lifetime assertion; the fixed core prints 0 and passes. Both call
the construction callback 22 times. The old negative and first test-only Vec3
comparison compile failure remain intact; no failure is relabelled as success.

The tests include actual MaterialSurfaceQuery ownership, exact binary64 query
results versus callback-free queries, an independent barycentric oracle,
defensive geometry copying, active per-query cancellation, and constructor
cancellation/throw/retry boundaries. The issue is unnecessary retention of
reachable callback captures, not a demonstrated actual Document ownership cycle
or unreachable allocation leak.

Each profile compiles the whole changed mesh-processing TU and three test TUs,
then links an audited 192-member thin core excluding the original member. The
remaining 191 whole core objects/support come from immutable current a904 owning
providers. Source/header dependencies and inputs/outputs remain exact. This is
a focused native component proof, not a fresh whole application, graphical,
large-query, physical-device, headset or full-goal acceptance result.
