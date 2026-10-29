# Persistent per-object deformation and GLB interchange

A `SculptObject` and its `ObjectSurface` carry a
`shared_ptr<const DeformationState>`. The state owns a detached skeleton, sparse
skin weights, stable-topology morph targets, optional humanoid/landmark/vowel
metadata, and separate current pose/morph values. `Scene::set_deformation`
validates the active painted rest mesh and publishes a new immutable value.
Copy a state, edit the copy, then replace it; snapshots and undo scenes retain
the previous state. Object duplicates may share immutable state safely.
`clear_deformation` removes only that object's pointer.

The active painted mesh remains the rest geometry, including vertex splits,
authored normals, UVs, pins and seams. `ObjectSurface::preview_mesh()` evaluates
morphs before skeletal deformation into a separate mesh. It never bakes the
preview back into authoring geometry. Document workers must use this method for
preview aggregation, retain their existing source/generation checks, and publish
only after successful completion. The exporter uses `ObjectSurface::mesh`, not
the evaluated preview.

Each state records the exact rest-position/index identity used by `MorphSet`.
Changed positions, triangle order or seam-driven vertex splits invalidate it.
Material/paint edits and object transforms preserve that identity. Remeshing,
retopology, import replacement or UV reparameterization must explicitly transfer
or discard deformation in the same scene transaction; stale state is rejected
by Scene validation. There is no hidden nearest-index transfer.

Native scene version 4 stores these records inside each object; versions 2/3
migrate with no invented rig data. The deformation record has an independent
version 1. Source identity, dimensions and aggregate authoring budgets are
checked before allocating decoded weight/morph arrays. Limits include 256 bones,
128 targets, 32 influences per vertex, two million rest vertices, two million
triangles and eight million total morph/weight/preview entries across the scene.
Existing 256 MiB file and paint/sculpt budgets remain in force. Draft humanoid
mappings may be incomplete while authoring, but all supplied assignments must
have valid semantics and ancestry.

## GLB representation

Static geometry, per-object PBR values and embedded painted PNGs retain their
existing representation. Deformable objects additionally export:

- Exact rest POSITION/NORMAL/UV/index accessors and named POSITION morph deltas.
  Default mesh weights preserve authored preview values; zero defaults remain
  explicit. `extras.targetNames` preserves morph identities.
- All sparse skin influences, packed into paired JOINTS_n/WEIGHTS_n VEC4 sets.
  Joint components are unsigned 16-bit, weights are float, and unused components
  are zero. Normalization is across all sets; nothing is silently truncated.
- Parent-local joint TRS in glTF quaternion order `[x,y,z,w]`, plus inverse-bind
  MAT4 accessors in rest joint order. Current pose transforms stay separate from
  rest inverse binds. Object TRS is carried by a rig root parenting the joints;
  the skinned mesh is an identity scene root because glTF ignores skinned-node
  ancestor transforms. Unskinned rig surfaces remain children of the object root.
- Complete validated humanoid assignments as `extras.domeSculptHumanoid`, with
  actual exported joint-node indices. An incomplete assignment fails export;
  callers can explicitly remove draft metadata to export ordinary glTF skins.
  Vowel identity bindings use `extras.domeSculptExpressions` on the mesh.

These application extras do not declare `VRMC_vrm`. Model licensing/meta,
spring bones, expression overrides, MToon and other VRM requirements are separate
work. This is a posed glTF snapshot, without animation tracks. Morphs currently
contain position displacements only: glTF retains authored base normals during
morphing, while native geometric previews recalculate normals. Authored normal
morphs and exact cross-viewer lighting equivalence for large deformations remain
future work. The original rest normals and split vertex attributes are preserved.

Normative references and exact official VRM/glTF revisions and hashes are in
[AVATAR.md](AVATAR.md); no new upstream implementation code is copied here.

## Verification

`tests/deformation.cpp` verifies detached immutable state, native scene roundtrip,
rest/preview separation, UV split metadata, stale source rejection and legacy
migration. `tests/gltf.cpp` independently parses the GLB with cgltf, reads all
skin/morph accessors, evaluates inverse binds and joint world transforms against
the native preview under object rotation/scale, checks humanoid references and
preserves exact painted PNG contents. Set `DOME_GLTF_FIXTURE` to an output prefix
to retain `.skin.glb` and `.humanoid.glb` interoperability fixtures. Khronos
`gltf-validator` 2.0.0-dev.3.10 additionally checks those generated artifacts.
