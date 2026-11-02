# Affine import continuation — private, unqualified

Base085d353, tree artifacts/import-source-chain-085d353. The full goal remains
active; this checkpoint is parked for the requested human headset test/pause.
There are no live jobs or GPU reservations. No push/install/HMD work occurred.

Implemented hybrid route selection: existing positive-uniform similarity imports
keep their old native graph and format versions. If any selected node needs
nonuniform/reflected/full affine placement, the ENTIRE selected original graph
uses shared immutable CreatorSourceModel/SourceNodePlacement authority, with
identity native editing deltas. Original local matrices, bytes/resources,
instances, material and skin/morph IDs are retained. Singular world inversion
refuses before publication. This avoids disabling existing similarity-only
OBJ→polygon/bake workflows merely because source authority is newly marked.

Actual import/Scene/export runs821 independent cgltf assertions, including
nonuniform reflected root plus rotated child, authored/implicit IBM, anisotropic
current joint pose, both instances and exact native restart. Sheared bind-space
export uses the existing bounded verified two-TRS factorization, retaining a
separate first/last rig-chain node so skinned detachment restores the full chain.
No original IBM or source local is rewritten. Existing five initial native
suites passed; subsequent combined run is7/8, with options-controller included.

DO NOT publish this as qualified full affine import. New actual skin-normal pixel
qualification fails54 initially. The independent consumer had hardcodedCCW for
all nodes, contrary to glTF mesh-instantiation global-determinant winding and
pinned SampleRenderer renderer.js1712–1718. Correcting that TEST-only omission
reduces the residual to18; tolerance2 is unchanged. Baseline skin pixels pass.
Next: independently establish reflected tangent/bitangent handedness and normals
before altering product or shared reference again. The suspected raw tangent.w
world-space omission has NOT been changed. Preserve both negative logs.

Primary evidence for the narrow reference front-face correction:
https://github.com/KhronosGroup/glTF/blob/main/specification/2.0/Specification.adoc#meshes
https://github.com/KhronosGroup/glTF-Sample-Renderer/blob/6e8a3d1aaa6f07e9f1310a488453e21325677786/source/Renderer/renderer.js#L1712-L1718

After pixel correctness: source/current-pose materials/normal/alpha/skin
interoperability, full original-chain FLTK import +ordered Save/Undo, fresh whole
core/support sanitizers and actual device proof. Material0/no-material, full
UV1 source sampling (including overlapping UV0), no-map authored UV authority,
remaining FBX skin/morph cases and metric authoring remain required. Existing
explicit guards stay until their actual adapters/consumer proofs; animation
tracks remain excluded with diagnostics under the agreed scope.

Qualified contact work is a SEPARATE leaf257f368:57 actual controller checks,
51 real native-thread denial assertions, own-core sanitizer9/9 and loader10+10
software/A770. That proof remains unchanged by this import WIP.
