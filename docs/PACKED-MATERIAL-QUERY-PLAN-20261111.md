# Packed material-query composition plan

Base f21cb91c, isolated work/packed-material-query-20261111. The proven renderer
transaction remains unchanged. The original4.8M and separate9.6M native publishers
fail before rendering because one global SurfaceQuery inherits a per-mesh2M
vertex/triangle bound. Preserve those standalone bounds and all native draw data.

The private PackedMaterialQuery pimpl lives beside the existing native Bvh kernel
in mesh-processing.cpp. MaterialSurfaceQuery's private Impl uses it. No public
header/layout changes are planned. Small admitted inputs delegate the unchanged
SurfaceQuery. Formerly unsupported aggregates retain their full original Mesh
for global UV/color/paint/material authority, including unused vertices, while
contiguous bounded triangle runs use compact referenced geometry internally.

Before copying a large source, validate every original attribute, pin/seam flag,
index and face, authoring bound and cancellation with the old geometric meaning.
Plan sorted referenced-vertex lists one part at a time; no whole-source-sized
remap array. Bound own retained storage512MiB and retained+peak build768MiB,
128parts,262144triangles/part and4194304total BVH nodes. Forecast full original
optional payload arrays, part Meshes, exactly reserved BVH nodes/order arrays,
metadata and current planning/build scratch, including caller-reserved linear
material attribute metadata. Existing immutable external assets and driver heap
are not claimed as this adapter's owned allocations. Fixed source/part limits
must fail prospectively, and partial/cancelled construction publishes nothing.

Use the same native closest/ray/normal math and binary64 result metric. One
visited counter spans all parts, preserving default2M legacy closest and explicit
65536 filtered/typed limits. Callbacks/results map to global original triangle
IDs. MaterialVisibility continues to use the original Mesh. Aggregate exact ties
choose earliest global primitive/draw order, as approved for GL_LESS; existing
small SurfaceQuery tie behavior stays unchanged. Mixed sculpt hits retain their
separate chunk namespace and shared mixed-scene work counter. No picking is
omitted and no fake packed SDF, cap raising or silent CPU-query fallback is used.

Required controls: original4.8M/count/indexed6.48M input and separate9.6M/>256MiB
real driver gate with picking/contact, existing240s/R4GiB/S8GiB guards; independent
small/aggregate IDs, barycentrics, alpha/UV/color, near-metric and exact edge/tie
cases; all facades/exclusions, one shared work budget, cancellation, malformed
unused data and prospective retained/build/node/part refusals. Fresh owning two
whole product TUs and matched callers precede runtime evidence. Parent frozen
build/main/providers must remain untouched.
