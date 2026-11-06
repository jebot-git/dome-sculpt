# Local sculpt coordinates preserve grouped geometry — private Release checkpoint

A genuine regression in the original authenticated local-surface suite exposed a
coordinate bug beyond the earlier25dd qualification. The original CPU sphere is
valid (4336 vertices/8668 triangles). Its first original grouped placement is
valid, but the original second object at worldX1.37 with scale1.25 loses distinct
vertices when transformed into absolute float world positions. The unchanged
Mesh::validate correctly refuses a degenerate triangle. The full native suite
passes72 original controls before its first grouped Document preview fails.
The source sphere, grouping, validation guard and failed outputs are retained.

Generated chunks now keep the actual immutable **local** Mesh as both GPU and
BVH geometry owner. The authored frame is retained separately. No absolute-world
float Mesh is constructed and no triangle is dropped. GPU uniforms apply actual
authored, held and canvas placement, full inverse-transpose normals, reflection
handedness and the transported Cut preview halfspace. Actual local geometry and
BVH ownership can survive authored placement changes as well as live motion.

Nearest queries transform edge differences in binary64 and minimize the real
world affine metric. They do not use a guessed uniform scale or Euclidean nearest
in sheared local coordinates. An outward-rounded interval image provides a
conservative bound for local BVH nodes. Ray queries transform the original ray
in binary64 and retain its world distance parameter. Normals follow per-vertex
inverse-transpose normalization and interpolation, matching the GPU convention.
Reflection retains original local source corner identities and the matching
front-face convention. The actual-vertex query workspace check remains intact.

`geometry_owner()` and its BVH now mean original local geometry; consumers must
use the captured placement for world queries or GPU transforms. Queries and the
renderer are updated accordingly. Framing and selected Cut source construction
already traverse the original source geometry with explicit Scene placement.
Their Document/Cut/app/framing consumers are nevertheless rebuilt as whole
translation units with the matching providers. Existing class, snapshot, hit and
options data layouts do not change: only new nonvirtual query overloads and a
private frame getter are added to the two existing headers.

The bounded optimized Release gate rebuilds eight whole product/readers plus
app/framing and three whole native/GL consumers, then links four new executables.
Actual link traces exclude every corresponding old archive member. All237 prior
protected outputs (233 project objects/archives/ELFs plus4 compiler probes) remain
exact. Sixteen new whole objects/ELFs form the tested composition.

The original full authenticated suite now passes95 controls, including real
asynchronous coalescing, grouped transforms, independently whole-meshed ordered
corners/normals, exact mask/material/local/placed identity handling, allocation
and journal/history controls, retained old snapshots, actual failure-hook witness
and subsequent recovery. The failure hook runs the genuine immutable factory
before injecting the late preparation failure; it is not an unreachable legacy
materializer hook or an exhaustive allocation sweep.

The pipeline native43 and real software-EGL72 controls pass. All six prior
whole-reference RGBA pairs remain byte-exact. A seventh pair verifies the original
grouped source using independently whole-meshed local objects with the renderer's
existing actual placement contract. The original invalid world-Mesh counterexample
still fails validation, while the valid grouped scene now renders. Authored group
motion preserves exact reference pixels with no geometry transfer. This counter
is a correctness property, not a measured speedup. A separate newly compiled
current public-query consumer passes108 oblique scale/aspect/axis/reflection/
winding families against the new complete mesh-processing provider.

The ordinary FLTK app smoke uses the new complete local-placement app ELF,
including its freshly compiled Document, Cut and framing consumers. It renders
1100×744 with74853 actual clay pixels, maps the canonical DomeD libraries and
closes cleanly through its existing product path. The lossless capture pixels
match the earlier visually inspected view. Project, SDK, protected outputs and
sampled runtime ELF hashes have no drift. Source/ABI and exact receipt details:
[SCULPT-LOCAL-PLACEMENT-RELEASE.json](validation/SCULPT-LOCAL-PLACEMENT-RELEASE.json).

This remains private CPU Release/software-EGL/Xvfb qualification. Current39/40
composition, sanitizer, hardware GPU/headset, fullALL and full production
acceptance remain required. Selected Cut/export and authenticated exact legacy
repair world materialization are separate existing coordinate boundaries for
further audit. GPU manifest metadata and changed mask-owner sampling still have
whole-directory/whole-object costs. The sustained production-size CPU/Arc GL/
frame-work gate is planned in
[SCULPT-PIPELINE-MEASUREMENT-PLAN.md](SCULPT-PIPELINE-MEASUREMENT-PLAN.md), not run.
No frame-rate, throughput, bounded total work or full GPU completion is claimed.
