# Native conformal UV solve and explicit stitching

The detached `uv-solve.hpp` API implements least-squares conformal mapping in
native C++20. It minimizes triangle-local Cauchy–Riemann residuals with area
weighting, eliminates exact hard-pin variables and solves sparse normal equations
using deterministic diagonally preconditioned conjugate gradients. Matrix-vector
products walk six-coefficient rows; no dense matrix or external solver/runtime
is introduced. The actual normal-equation residual is recomputed before success.

The implementation follows the mathematical LSCM method of Lévy, Petitjean, Ray
and Maillot, *Least Squares Conformal Maps for Automatic Texture Atlas Generation*
(2002), DOI [10.1145/566570.566590](https://doi.org/10.1145/566570.566590).
The primary [paper](https://www.cs.tau.ac.il/~dcor/Graphics/papers/s2002_lscm.pdf)
and already pinned xatlas/OpenNL local-frame/complex-equation implementation were
inspected. xatlas commit `f700c7790aaa030e794b52ba7791a05c085faf0c`, native
`third_party/xatlas/xatlas.cpp` SHA256
`0ed0283aad005c94738cb0cc4612dba264379d29dea5b3c9b242f2d4752d5df4`, is MIT;
its embedded OpenNL component retains Bruno Lévy's 2004–2010 BSD notice in that
file. No OpenNL runtime or copied matrix/solver code is added: the matrix-free
solver, hard-variable elimination, remaps, selection and explicit stitch handling
are original native code. Existing xatlas/OpenNL notices remain intact.

Blender's official [unwrap documentation](https://docs.blender.org/manual/en/latest/modeling/meshes/editing/uv.html)
and [pin workflow](https://docs.blender.org/manual/en/latest/modeling/meshes/uv/editing.html)
provide workflow references only. This is not Blender's ABF/SLIM implementation.
Automatic atlas creation/packing remains the separately integrated xatlas path.

Every original face/corner stays in order. Corner-fan disjoint sets follow
explicit cuts and, by default, existing UV discontinuities. Required cuts append
UV copies without merging original vertices, geometric positions, normals,
source pin bytes or unused vertices. The complete output→source vertex array and
corner→output array allow native skin/morph/source attributes to follow every
split exactly. A solve requiring no split keeps the original rest topology.
Hard pins are copied bit-for-bit; incompatible duplicate pins reject stitching.
Underpinned charts get deterministic farthest-boundary anchors. Seed faces select
entire charts; unselected UV islands remain exact.

Stitching requires explicit outgoing corner-edge pairs with opposite endpoints
within a finite caller-supplied tolerance. Only those pairs join and clear seam
flags. This is UV continuity, not an implicit global geometric weld. Joined
charts then conformally reflow. If pins conflict or geometry does not match,
callers receive an actionable error without a partially changed mesh.

Closed charts require seams. Nonmanifold/inconsistently oriented edges,
ill-conditioned triangles, singular pin arrangements, exhausted iteration/work/
memory limits, collapsed faces and triangle foldovers reject. LSCM does not
promise a bijective atlas for arbitrary organic surfaces or global overlap-free
packing: charts with holes/complex curvature may need additional cuts or another
parameterization method. Deliberately invalid authored geometry is not silently
repaired. UVs may leave the unit tile; packing or a separate UV validation step
is required before paint transfer/publication. Hard pins are never silently
clamped or moved for packing.

The default bounds are 200,000 input/output vertices, 400,000 triangles, 2,000 CG
iterations, 200 million accounted row/adjacency operations and 256 MiB conservative
estimated peak storage. All bounds are explicit and validated. Matrix-free work
and chart traversals check cancellation. Geometry, solve and resampling belong
on the guarded document worker, never on desktop input/model/XR/render threads.
Callers must compare source identity/revision before publishing, remap every
source-dependent attribute and perform separate atlas/paint validation.

Validation includes an independently known 2:1 planar rectangle, a bent
developable prism strip checked against intrinsic triangle angles and chord
lengths, exact tiny/pinned charts, explicit split/stitch remaps, and independent
blended skin/morph evaluation before/after splitting. Finite explicit pairing,
conflicting/coincident pins, disconnected selection, closed charts with and
without valid cuts, exhausted convergence and resource/cancellation failures
have fixtures.

The actual CC0 VRoid female body material (4,360 triangles/five charts) converges
under default work bounds. Independent mean triangle angle error decreases from
0.125868 to 0.022915 radians. An audit of all 19 female parts solved 18; the source
tie reports a nonmanifold edge rather than being silently repaired. The large
original MakeHuman first chart produces foldovers and correctly rejects; it
needs additional cuts or another parameterization. This is explicit scope, not
an arbitrary-character or production-atlas completion claim.
