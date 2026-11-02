# Preserve skinned surface placement during export

Export previously detached each skinned surface as an identity scene root.
That preserved joint-driven positions but discarded the original mesh-node
global transform. Reflected single-sided surfaces could disappear; mapped
normal handedness also depends on the retained instantiation convention.
The fix keeps both ordinary and shared-rig surfaces under their original
object/group parent. Joint matrices, inverse binds, indices and all vertex and
morph attributes are unchanged. Valid multi-node bind-space factoring remains.

The [glTF mesh-instantiation rules](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#instantiation)
describe determinant-based winding; its skinned-primitive rules compute vertex
positions from joints and inverse binds. Preserving the original hierarchy
retains both authorities without inventing a replacement reflection transform.

An authored-material regression uses the actual `Document::export_asset`
worker, independently parses both original and emitted files, and compares
their images. It covers both determinant signs, nonuniform/sheared source
chains, authored and implicit inverse binds, normal mapping, N/T/POSITION
morphs, single/double-sided materials, two original instances and a same-group
duplicate that exercises rig sharing. Every emitted mesh's complete world
matrix is compared with its independent original-node matrix. The former
identity-root assertions in the older GLB test now verify complete object,
nested-group and shared-rig world bases, retaining the independent joint/IBM
posed-position oracle.

The matching published `0d2ffbb` negative passes eight positive-determinant
image cases, then the reflected single-sided case gives native error0 and
worker-export error255. Its exact old core/render archives, link map, source
fixture and inputs remain under artifacts. A preliminary eager-composite
`Scene::surfaces()` probe gave error29 from texture resampling; that separate
route is retained as diagnostic evidence and is not the causal export-worker
negative. No tolerance was weakened.

Final optimized Release passes nine selected suites in4.58s. The same product
also passes the actual female/male CC0 VRM suite in103.62s; that earlier batch
had only the obsolete identity-root assertion failure. A fresh fully
instrumented own C/C++ core, renderer and support build passes nine selected
ASan/UBSan/leak suites in17.86s. External Mesa, FLTK, image and LAPACK libraries
remain uninstrumented. The finalized 192-assertion graphical fixture passes
software and actual Arc A770: all32 images error0 at unchanged tolerance2 and
all retained world matrices error0. Exact flags, source and log hashes are in
validation/SKINNED-SURFACE-HIERARCHY.json.

This is a narrow export fix on the published affine baseline. It contains no
Scene17/default-material or Scene18/UV1 changes. Their private adapters and the
remaining full glTF/VRM production scope remain required. No physical headset,
installation or active-session changes were performed.
