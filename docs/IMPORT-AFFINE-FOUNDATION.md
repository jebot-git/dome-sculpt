# Native affine import foundation

This isolated continuation starts from published 8ad6d305. It implements native
math and immutable source-hierarchy helpers. It does not remove the current
similarity-only authoring import guard, add Scene fields, or claim full affine
editing/rendering/export is integrated.

## Concrete implementation

`include/dome/affine.hpp` and `src/affine.cpp` provide `AffineTransform`:
validated immutable column-major double matrices, ordered composition, inverse,
point/direction transformation, raw cofactor and inverse-transpose normal
transformation, reflection orientation, and checked conversion to float GPU
matrices. There is no normalization, Gram-Schmidt step or implicit determinant
sign fold: the declared source material/frame convention owns those operations.
Nonfinite/projective input, arithmetic overflow and nonzero float underflow are
explicit errors. Singular forward transforms/cofactors remain representable;
inverse operations refuse singular or nearly dependent axes. Column scales are
factored separately, so orthogonal anisotropy is not falsely called singular.

`src/import-affine.hpp/.cpp` provide `detail::ImportedAffineHierarchy`. It retains
a strong immutable ImportedAsset owner, exact selected source node IDs, parent
slots and original local float matrices. Selected work is bounded to 128 nodes,
with cancellation between nodes and an atomic detached constructor. Repeated
nodes, bad parent/child witnesses, out-of-range children and invalid original
local matrices refuse. Computed world shear is allowed. `local_chain()` returns
original local matrices and source IDs without decomposing a world matrix;
`inverse_world()`, `normal_world()` and `orientation_world()` preserve that
factorization. This also retains normal transport when extreme anisotropy makes
flattened world axes nearly parallel. Source metadata outside the selected tree
is retained by the original asset; this helper is not a replacement parser or a
claim to revalidate every unselected node.

The new `dome-sculpt-affine` static target has no UI, GPU runtime or codec.
Core links it; standalone tests compile only the foundation and the real small
sculpt-storage accounting support required by ImportedAsset header destructors.
When the separately qualified JSON target is present, affine consumers inherit
its matching forward/full headers too. No copied/mock storage or JSON code is
introduced here.

## Independent proof

Fresh optimized Release and fresh ASan/UBSan/leak builds pass both tests:

- 74 analytic/diverse affine assertions: sheared reflected point/inverse,
  independent transformed-edge cross products vs cofactors, raw normal values,
  normal/edge orthogonality, both inverse orders, extreme-scale individual
  component accuracy, singularity, conditioning, finite arithmetic/GPU boundary and uncertain determinant sign refusal.
- 56 source hierarchy assertions: original local bit patterns including signed
  zero, noncontiguous source IDs distinct from selected slots, genuine world
  shear from valid local TRS, 128-node chains, reflected orientation, reverse
  inverses, retained source lifetime, invalid graph/quotas, singular and extreme
  factorized normal transport. Every measured constructor/chain allocation
  boundary (6) is denied persistently; every cancellation boundary (4) refuses,
  and retry returns the exact source chain.

This is scoped native foundation qualification, not a whole-core build or a
posed-pixel/GUI test. The existing import guard remains unchanged. No device or
installation work was performed.

## Integration that must precede guard removal

1. Add immutable imported-node/source-instance placement authority, with explicit
   source node IDs and exact original local edges. Native editing deltas must
   have a declared coordinate space. Under affine parents, a world-space grip
   similarity conjugates to a general local affine; it cannot be squeezed into
   today's single CanvasTransform. Preserve the required factors or a correctly
   versioned editable authority rather than silently projecting back to TRS.
2. Add Scene/ObjectSurface effective-matrix queries, leaving similarity queries
   explicit for consumers that genuinely require similarity. Extend recursive
   current/dormant source and PolygonOrigin retention. Public Scene 11 readers
   ignore unknown group fields, so new affine placement needs the reserved
   Scene 13 feature gate beyond Polygon 12; do not silently expand Scene 11.
3. Migrate worker positions, source/canonical N/T and reflection/facing behavior,
   per-range held transforms, strong geometry cache dependencies, source-aware
   picking, overlay geometry and editing diagnostics together. Respect each
   versioned frame's normalization/interpolation policy.
4. Replace inverse-rotation/uniform-scale ray and brush assumptions. A normalized
   world ray transformed by inverse(A) changes direction length; convert hit and
   maximum distances accordingly. World brush distances in local coordinates
   use the full quadratic metric L-transpose*L, with conservative support bounds,
   not radius divided by one guessed scale. Painting, rig/weight/UV/retopo and
   explicit remesh/voxel conversion need coherent coordinate/metric policies.
5. Preserve the original local graph on export, node/instance/joint identities,
   exact authored or implicit-identity IBMs, morph directions and source frame
   authority. Do not emit a sheared inverse world as one glTF node. Skinned-node
   placement and joint-global*IBM must remain consistent across rest/current
   poses; native wrapper edits must affect the right joint graph as well.
6. Preserve one transactional Undo, cancelled/stale worker rejection, raw source
   receipts and retry under persistent allocation failures. Qualify independent
   source-vs-native rest/posed accessors and pixels, reflected/nonuniform/gripped
   paths, save/restart, actual native GUI/XR options, and export consumers before
   enabling general authoring adoption.

These requirements also leave FBX skin/morph conversion, default-material
identity, general PBR UV1/overlapping untextured UV authority and further required
import cases open. They are not completed by matrix helpers.

## Primary semantics

The glTF specification requires each local node matrix to be TRS-decomposable;
valid local chains may yield world shear. Skinning depends on joint globals and
inverse binds, with the skinned mesh node transform excluded. The original
source graph therefore remains the transport authority. References:
[glTF transformations](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#transformations),
[glTF joint hierarchy](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#joint-hierarchy),
and the [primary AsciiDoc source](https://github.com/KhronosGroup/glTF/blob/main/specification/2.0/Specification.adoc).
The new implementation is original native C++; no third-party source was copied.
