# Native glTF authoring import

The full generic import objective remains active. The first implementation
replaces Document's blanket color/tangent/UV1/texture/alpha refusal with a
background, source-retaining adoption adapter. It is a useful subset, not full
scene/skin/extension import. Unsupported cases fail before publication.

## Data and coordinate audit

`ImportMode::PreserveGltf` retains unique mesh primitives in mesh coordinates,
all node local/world matrices, node/mesh target weights, scene roots, source
indices, all joint/weight sets and authored inverse-bind accessors including
trailing matrices. StaticScene bakes coordinates and cannot be the production
adoption path. `CreatorAppearance` currently archives embedded GLB bytes and
maps native vertices to exact original accessor indices. Retained normal frames
carry ordered corner/source IDs and NORMAL/TANGENT morph deltas. Static COLOR_0
is separate immutable linear RGBA source data, not texture pixels.

The current native group transform is a positive uniform similarity. Native
Skeleton recomputes inverse(rest) and skips skinning when pose is empty. These
cannot represent arbitrary glTF node transforms or authored bind spaces. The
next private foundation introduces explicit bone-aligned inverse binds, exact
imported affine authority separate from native editing deltas, and Scene11
feature discrimination. The color preparation actor independently owns Scene10;
the combined writer must use the maximum required version, including dormant
and retained origin/receipt deformation, not only current visible rigs.

The glTF skin formula is sum(weight * jointGlobal * inverseBind); a skinned
primitive ignores its instantiating mesh node's global transform, and missing
inverseBindMatrices means identity, not inverse(rest). Native rendering, frame
transport, picking, editing and export must use the same convention. Node target
weights override mesh weights. See [glTF 2.0 skinning and morph specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html).

## First implemented stage

`prepare_authoring_import` prepares a detached Scene. `Document::import_asset`
uses it inside the existing ordered file/asset worker and publication/history
boundary; admission, generations and cancellation infrastructure stay intact.
It adopts the selected source scene, its node hierarchy and mesh instances as
native groups and independently editable objects. Each instance retains distinct
source-node identity and shares the immutable original asset/images. Geometry
is not baked into world coordinates. Current admitted node matrices must be
representable positive similarities, checked by reconstructing their columns.

Embedded PNG bytes/pixels, sampler wrap/filter modes, scalar PBR values, unlit,
explicit Opaque/Mask/Blend and cutoff/double-sided states are retained. Original
UV1 attributes, linear COLOR_0, authored N/T/W and POSITION/NORMAL/TANGENT morph
channels remain mapped to original accessors. Node morph defaults apply per
instance. Original UV0 is not silently rewritten. Missing UV0 creates an atlas
and remaps every native source/POSITION channel by exact ordered corners;
coincident coordinates never establish source identity. Original raw GLB and
material JSON remain immutable and are serialized through the existing archive.

Original NORMAL absence now generates per-triangle base and morph-key normals,
with supplied tangents/displacements ignored for derived shading. Original raw
absence and attributes remain unchanged; current split vertices retain exact
source/corner identities. UV0 normal textures generate original-basis Mikk frames;
no-normal-texture sources keep genuine N-only frames. See GLTF-FLAT-NORMALS.md for
independent accessors/graphics/native/controller qualification. UV1 paint-source
sampling and other unsupported material/transform cases remain required.

Textured original UVs use SourceAuthored painting. The current no-map codec
cannot authorize SourceAuthored; untextured inputs therefore require Strict
writable UVs or a valid generated atlas. Legal overlapping/repeating untextured
UVs require the explicit Scene11 source-UV authority marker below, rather than
emitting an unreadable record or silently changing raw UVs. OBJ keeps its
existing static path and rejects unsupported texture adoption without dropping
maps.

The native quaternion conversion explicitly bridges mesh math WXYZ and
OpenBrush XYZW storage. Both direct original node-world position checks and
independent raw-file pixels are needed: native round-trip tests alone can agree
with an incorrectly converted transform. The independent test consumer now
correctly applies node.weights before mesh.weights, which earlier generated
native exports had not exercised.

The subsequent [authored-bind foundation](GLTF-AUTHORED-BINDS.md) adds actual
source skin adoption within the current group admission limits, with explicit
bind/rest/frame/export/Scene11 evidence and remaining full-affine guards.

## Required remaining implementation

- Arbitrary nonuniform/reflected affine hierarchy, with exact original node IDs,
  source authority and independent native similarity edits. Rendering alone is
  insufficient: physical painting, material-aware picking, normal/TBN parity,
  negative-determinant culling, export, Undo and remesh conversion must agree.
- All valid skin instances and ancestor nodes, authored and implicit-identity
  inverse binds, exact rest and posed results, complete influence groups,
  nonuniform/reflected joint transforms and shared joint-node pose authority.
  Conventional inverse-rest bindings alone do not complete skin import.
- Exact `.gltf` root/resource bundles and canonical embedded adapter transport,
  with bounded confined resolution and immutable raw bytes. Preserve absence of
  a primitive material as actual default-material identity0/optional metadata.
- Bounded JPEG decoding as well as PNG, exact encoded source resources,
  per-role PBR UV1 sampling through painting/compositing/picking/quality and
  export, and supported texture-transform semantics. Existing source paint maps
  accept UV0 only, so texture UV1 is still explicitly guarded.
- Plain no-map SourceAuthored persistence under explicit Scene11 authority;
  arbitrary admitted native morph defaults and generated/missing direction
  policies; COLOR/UV morph attributes remain a separate full-scope requirement
  from the completed static COLOR_0 path.
- Required extensions and source scene/instance editing/interoperability beyond
  this stage. Raw preservation and explicit diagnostics do not imply complete
  extension rendering. Animation tracks and final scene rendering remain outside
  scope, with explicit omission diagnostics.

## First-stage validation

Fresh regular-user Release CPU/FLTK builds use canonical DomeD282467e toolkit.
The native fixture tests detached caller/source immutability, independent node
world positions, two instances, exact bytes/images/samplers/materials/static
attributes, all three implemented morph channels, original accessor reimport,
actual native restart, generated-UV remaps/restart, staged cancellation, one
Undo/Redo and background cancellation/retry. Existing document-assets and
mesh-import suites remain covered. A separate parsed-file cgltf/PNG/GLSL
consumer compares the actual imported Document's pixels against original GLB:
Opaque/Mask/Blend at rest and nonzero P/N/T morph, twelve checks with zero pixel
error on software EGL. This validates source adoption, not a manually built
native fixture or only a newly exported file.

The native GUI fixture uses the actual FLTK Import chooser, DomeD universal
menu, Save and one-step Undo/Redo on a private bus/display; input bytes remain
unchanged. Scoped ASan/UBSan/leak coverage instruments the changed helper,
Document-assets, Paint and native test translation units against matching
Release support. It does not claim fresh whole-core sanitizer coverage or a
physical headset test. Full affine/bind/raw-resource stage proofs are separate.
