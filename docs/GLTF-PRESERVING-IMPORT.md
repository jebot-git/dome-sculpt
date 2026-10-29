# Authored glTF hierarchy, skin and morph import

`ImportMode::PreserveGltf` is an explicit native import mode that retains the
supported authored glTF payload for later mapping. The default remains
`StaticScene`: selected static mesh instances are world-baked and skin/morph
assets reject explicitly. OBJ behavior is unchanged. Preserving mode is a data
foundation, not a claim that the current Scene, native Skeleton, renderer or
exporter can represent every imported asset without further integration.

```cpp
ImportLimits limits;
limits.mode = ImportMode::PreserveGltf;
auto asset = import_mesh(path, limits);
```

## Identity and coordinates

Preserving mode retains all original nodes, scenes, skins and meshes, including
unused meshes and nodes outside the default scene. Their vector positions remain
original glTF indices. `default_scene == -1` preserves an omitted default rather
than silently choosing another scene. Shared mesh instances reference one stored
mesh; geometry is imported once per source mesh primitive. A valid hierarchy-only
asset can have no renderable primitives.

Nodes retain parent/ordered children, mesh/skin references, authored matrix-vs-TRS
flags, raw translation/quaternion/scale values, local matrices, derived world
matrices and node-level morph weights. Camera/light source indices remain with
explicit payload-omission warnings; camera/light authoring is not implemented.
Each mesh retains primitive references, target names and mesh-level morph weights.
Scene roots and JSON extras are retained. Extras preserve arbitrary JSON values,
including escaped strings and native expression metadata. Names decode JSON
escapes and Unicode surrogate pairs. Resource URIs decode JSON escapes before
percent decoding and canonical confined path resolution.

Primitive `mesh_local` is true. Positions, authored normals, tangents, winding,
morph differences and joint attributes remain in original mesh coordinates.
Reflected or nonuniform node transforms are not baked into this data. The
primitive retains source mesh/primitive indices; `source_node == -1` indicates
shared mesh data without a single owning instance. Its `source_world` is identity;
instance transforms live in the node hierarchy. Missing normals can still be
generated for the display `Mesh`, with `normals_authored == false` recording that
absence. Unreferenced vertices remain present; missing/ambiguous display normals
use a derived face/default fallback without inventing authored normal data. No
default morph or current pose is applied to the stored rest mesh.

## Authored skins are not native Skeleton rest poses

Every `JOINTS_n`/`WEIGHTS_n` set is retained in order, without a four/eight-influence
limit, sorting, truncation, joint compaction or normalization. Metadata records
original joint/weight component types and weight normalization. Sparse accessors
and normalized unsigned byte/short weights decode through the same bounded
accessor path. Joint values index the selected skin's ordered node mapping;
they are not direct native bone indices.

Skins retain their joint-node mappings, optional skeleton-root node and the full
authored inverse-bind accessor, including trailing matrices beyond the joint
count. `inverse_bind_matrices_authored == false` with an empty matrix vector
means that glTF implicitly specifies identity per joint. It does not mean that
inverse binds should be recomputed from current node transforms.

The existing native `Skeleton::inverse_bind()` computes inverse binds from native
rest transforms. An arbitrary glTF skin can have a currently posed hierarchy and
independently authored inverse binds. Copying its current joint transforms into
native rest bones and recomputing inverse binds would change the asset. Keep the
raw matrices and mappings until an explicit compatible representation or generic
glTF deformation path exists. For glTF evaluation, morph differences apply before
skinning and each joint global matrix multiplies its authored inverse bind;
the skinned mesh node's global transform is not an additional mesh transform.
The importer does not silently convert this state to native Skeleton.

Validation requires paired consecutive influence semantics, valid VEC4 component
types, nonnegative finite weights, unique nonzero joint influences per vertex,
valid slots against every skin that instances the mesh, nonzero bound vertices,
unique joint nodes with a common hierarchy root, valid skeleton ancestry and
common-root scene membership. Integer normalized sets must sum exactly to one,
using equivalent unsigned16 integer sums. Floating sums that differ from the
recommended unit sum produce diagnostics while retaining exact authored values.
Unbound influence arrays remain present with an explicit mapping warning.

## Morph channels and defaults

Each primitive retains all authored POSITION, NORMAL and three-component TANGENT
morph difference arrays. Missing channels remain empty and mean implicit zeros;
tangent differences do not contain handedness. Sparse targets preserve implicit
zero base values. Target ordering and `extras.targetNames` remain unchanged.
Mesh weights and overriding node weights remain separate, with absent arrays
distinct from authored zero arrays. Values outside zero-to-one are retained.

A mesh may mix morphed primitives and primitives with implicit zero targets.
The older cgltf 1.15 validator assumes equal explicit target-array lengths. The
importer temporarily supplies empty target descriptors only to its validator
view, then restores original presence. `morph_targets_authored` records authored
presence; implicit targets are retained as empty channel records. No actual
geometry or parser-owned target payload is rewritten.

Unsupported morph channels such as UV/color differences reject explicitly.
Missing matching base channels, wrong types/counts, malformed sparse data and
nonfinite values reject. Morph names are not forced into the native MorphSet's
unique-name conventions; future mapping must handle incompatible identities
explicitly rather than rename or merge them silently.

## Quotas, diagnostics and remaining scope

The existing file/resource/geometry quotas, custom parser allocator, confined
external resources and overflow-safe accessor checks remain active. New owned
hierarchy, influence, morph, inverse-bind, string and extras payloads are charged
to the conservative resource quota before major allocation. URI decoding reserves
space for transient JSON decoding before allocating. Hierarchy depth is bounded
before cgltf validation performs parent-chain walks; traversal and skin-instance
mapping stages each have a bounded work budget (`max_metadata_visits`, default
50 million, maximum 200 million). Cancellation is checked during these stages,
large attribute loops and immediately before asset return. No partial asset is
published. As before, quota accounting is conservative rather than exact process
resident-memory measurement.

Animation import is outside this component. Clip names, channel/sampler counts
and explicit warnings are returned in `omitted_animations`; no animation tracks
or playback interoperability is claimed. Required extensions reject. Optional
extension names and named omission diagnostics remain visible; unsupported
other material shaders, texture transforms/compression, GPU instancing and geometry
compression still reject. Additional UV sets beyond UV1, additional color sets,
unknown/custom vertex attributes, non-triangle primitive modes and other unsupported
semantics retain explicit rejection. Camera/light payload and general extension
round-tripping remain unfinished. This is not complete glTF/VRM support or a full
schema validator.

The authoring integration must persist this payload and supply correct deformation
and material/texture mapping before exposing preserving import as an editable
scene operation. The existing default authoring path intentionally keeps its
explicit static-only gate.

## Self-contained memory GLB and explicit legacy VRM compatibility

`import_glb(span<const uint8_t>, ImportLimits = {}, ImportMode = PreserveGltf)`
parses native GLB/VRM bytes directly. Its explicit third mode argument overrides
`limits.mode`. It enforces byte/allocation limits before copying, validates GLB
magic and uses the same accessor, skin, morph, extension and cancellation paths.
It always forces external resources off and has no filesystem base path. Existing
files in the current directory cannot become dependencies, even when the caller
sets `external_resources=true`. Embedded BIN and data-URI resources are supported;
external buffers/images reject. Returned payloads own all data independently of
caller bytes. No temporary file is created.

Preserving file import also recognizes `.vrm` as a native GLB-magic alias.
Default static import does not silently enable VRM authoring behavior, and JSON
content named `.vrm` rejects. Original GLB bytes can be archived once per asset to
reconstruct this payload after restart without relying on the source path.

Some original CC0 VRoid0.14/UniGLTF1.28 beta assets contain a non-ancestor
`skin.skeleton` hint even though their actual joint hierarchy and inverse binds
are intact. Strict glTF import still rejects this. The explicit opt-in
`ImportLimits.allow_legacy_vrm_skin_hint` applies only in preserving mode with a
parsed top-level `VRM` extension declaring `specVersion="0.0"`; setting it on an
ordinary glTF or absent/other VRM version rejects. A non-ancestor hint remains
unchanged, `ImportedSkin.skeleton_hint_advisory` is set and a named warning is
returned. Joint common roots, scene membership, mappings and authored binds stay
strict. The verified creator adapter opts in only for its hash-verified original
CC0 sources. This is an advisory metadata policy, not a generic malformed glTF
fallback or an implicit rig repair.

Preserving mode additionally retains `ImportedMaterial.unlit` with a named
renderer-mapping diagnostic. Static PBR import still rejects unlit materials.
This allows legacy VRM/MToon sources that declare `KHR_materials_unlit` to retain
their authored semantics; it does not claim PBR equivalence or implement MToon
rendering. Required-extension gates remain unchanged.

## Primary specification and implementation provenance

Rules were checked against the official Khronos glTF 2.0 specification, pinned
repository commit `3cd6f59b277ed7f6540dbabdacc7841a5fdc0ded`,
`specification/2.0/Specification.adoc`, SHA-256
`b4c436a38c3ec374c47b8522b7c9e14c68890454a5bfe7d2b84642760e5d079b`.
Relevant sections are Scenes/Transformations, Accessors/Sparse Accessors,
Meshes/Morph Targets, Skinned Mesh Attributes, Skins/Joint Hierarchy and
Instantiation. Specification reference only; no document text or new upstream
runtime is vendored. Source:
<https://github.com/KhronosGroup/glTF/blob/3cd6f59b277ed7f6540dbabdacc7841a5fdc0ded/specification/2.0/Specification.adoc>.
The registry rendering is <https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html>.

Parsing still uses the existing pinned cgltf 1.15 MIT header, with its original
notice/hash documented in `IMPORT-AND-MESH.md`; no vendored source changes were
made. JSON decoding uses the existing nlohmann/json dependency to preserve
surrogate pairs and arbitrary extras correctly. New native integration code is
GPL-3.0-or-later. The central manifest should record this runtime use and the
specification reference; central files are maintained by the integration owner.

## Native verification

The suite keeps all original static/OBJ checks and exports an actual posed native
five-joint, five-influence/two-set skin and morph GLB through the same native
export path exercised in `tests/gltf.cpp`. It reimports rest coordinates,
hierarchy, exact authored inverse binds, all influence slots, morph defaults/names
and expression extras. Additional fixtures exercise reflected/shared matrix/TRS
instances, all three morph channels, sparse targets/weights/indices, mixed implicit
targets, normalized byte/short influences, trailing/implicit inverse binds,
multiple skin-instance mappings, Unicode/URI escaping, hierarchy-only assets,
animation diagnostics and malformed ranges/types/weights/mappings/hierarchies.
These are native interoperability fixtures, not Blender/device/UI validation.

```sh
c++ -std=c++20 -O2 -Wall -Wextra -Wpedantic -Iinclude -Ithird_party/cgltf -Ithird_party/openbrush tests/mesh-import.cpp src/import.cpp src/mesh-processing.cpp build/libdome-sculpt-core.a build/libxatlas.a -lpng -lz -pthread -o /tmp/dome-import-test
/tmp/dome-import-test
```

The focused milestone passes 78 native assertions in optimized Release and in
AddressSanitizer/UndefinedBehaviorSanitizer with leak detection. The sanitizer
build instruments `import.cpp`, `mesh-processing.cpp` and this suite, linked to
the existing Release core archive; it does not instrument every export/deformation
translation unit. Logs are `artifacts/mesh-import.log` and
`artifacts/mesh-import-sanitizers.log`.

The follow-up memory/legacy milestone passes 97 native Release and
ASan/UBSan/leak assertions. Pure memory import also passes the actual creator
reference assets: female SHA-256
`1995f174f6c8f2211f3c56c9e5fe4f9daf27ee5e648f129479582a259238149b`
(87 nodes), male SHA-256
`ab9af434a229283c192546486f9e6eb9cd855bf7d8d45b5037ac211938cb903c`
(71 nodes). Each retains 2 skins, 19 primitives, 25 images, 15 unlit materials and
one unchanged advisory skeleton hint under explicit legacy opt-in. Asset origins
and CC0 terms are recorded in `assets/humanoid/vroid/SOURCES.json`. The actual
source smoke log is `artifacts/vrm-memory-import.log`.
