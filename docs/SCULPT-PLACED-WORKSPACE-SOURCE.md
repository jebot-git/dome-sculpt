# Placed sculpt owner accounting: source checkpoint

2026-11-06. Child of498fee621e6dd71491a4b3fd0b18ddd53b0902dd. The original
P1 inventory now genuinely cold-publishes258852 triangles,147932 vertices and632
chunks with588739 complete SDF samples. Its independent checkpoint passes, but
the first actual warm-up stroke refuses with a placed-directory working-byte
error. The481787904B RSS peak is an observation, not a replacement admission
bound. Original providers, result/rows and the failed process scope are frozen.
There is no full P1 admission or sustained performance claim.

## Actual ownership scopes

The Scene already reserves its current geometry and complete previous Scene
before asking a placed directory to build. The old directory then reserves the
previous placed report, current geometry and each source chunk again. Its report
also included source geometry in both the full geometry directory and each
placed payload. Those are shared immutable allocations, not copies.

This patch retains full standalone placed reports and guards. New private
placement_bytes methods identify only placement-layer wrappers/BVHs/index nodes;
the full geometry owner remains explicit and is reserved once in aggregate
Scene retention. Nested factory budgets subtract only allocations *outside*
their actual scope. Exact source/query control blocks receive credit; equal
values, another geometry owner or an assumed overlap between different COW
roots do not. Distinct prior/current geometry roots are still fully charged,
as are previous wrappers/BVHs, new wrappers/BVHs and all persistent path-node
requests. The existing conservative cumulative AVL request charge remains.
The same exact-owner scope correction applies when Scene calls a previous
geometry producer or scalar directory. Standalone per-chunk capture still
reserves its full source plus every distinct old/new wrapper/query owner.

No numerical1GiB/512MiB directory cap or16MiB/8MiB chunk cap changes. No data
member, serialized schema, mesh value, affine/query interpretation, global run
order, source sampling, renderer path or cancellation guard changes. Three
whole provider TUs are affected. New private methods append no object fields.
Diagnostic refusals now include the directory phase/live/request/limit.

## Regression source and current evidence

The separate manual native consumer uses a genuine complete clipped SDF.
Default small mode covers visible, hidden and reflected/sheared sibling objects;
actual authored movement sharing real source/BVH owners; standalone exact
one-byte tight working/retained denials and cancellation; actual Document warm
stroke/Undo/Redo and held Cancel; and the real completed capture-return barrier
before a stale-generation replacement. Ordered local corners and normals are
compared bitwise against independent whole extraction; per-chunk affine query
results are compared with an independent whole-geometry BVH.
`--p1-warm-undo` preserves the original .0125 complete single-object source and
requires at least250000 visible triangles. It remains unexecuted.

All three complete providers and the new complete consumer pass O3/NDEBUG
C++20 syntax with Wall/Wextra/Wpedantic and zero diagnostics. Own inputs and632
current root product files rehash without drift. Initial test-only missing
NativeAffineEdit definition and formatting diagnostics are preserved. There are
no objects, executables, native tests, GPU runs or fixed-source P1 results yet.
Read [the receipt](validation/SCULPT-PLACED-WORKSPACE-SOURCE.json) and
[the bounded proposal](SCULPT-PLACED-WORKSPACE-GATE-PLAN.md).

Existing whole-corner/query/frame/history tests remain unchanged and must pass
on rebuilt affected providers. The old cold inventory remains incomplete until
its entire original warm-up/Undo/independent graphics protocol passes. No lower
density fallback, larger cap, installation, headset or full-goal claim follows.
