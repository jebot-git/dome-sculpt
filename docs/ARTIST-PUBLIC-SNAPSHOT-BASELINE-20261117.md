# Public prepared-source fixture: snapshot owner baseline correction

The frozen6c8 producer's first actual run fails its combined source-retention
assertion on MT1/rest. The same executable first passes unchanged native91; all
three ldd checks pass, and no font/publicUI phase is reached. Actual failure,
partial input, source/binaries/old pins and terminal service1 are preserved in
`artifacts/artist-prepared-public-link-v3-20261115/native-public-v3-20261117-v1`.
There is no resource failure, signal or input drift. The common lease is released
and all owned identities are closed.

Read-only production inspection establishes a fixture identity error:
`Scene::snapshot()` (`src/scene.cpp`) calls `PaintedMesh::snapshot()` for each
object. That method explicitly allocates a new COW wrapper, retaining immutable
surface/maps/projection owners. The frozen fixture captures Prepared.painted from
its immutable input Scene, then creates the detached expected snapshot, and
compares that snapshot's paint pointer to the input's pointer after Bake. Those
wrapper addresses are already necessarily different before Bake.

The new test-only child captures the exact paint-wrapper identity **after** the
expected snapshot and **before** Bake. A new pre-Bake assertion establishes the
new wrapper is distinct but the entire encoded three-object Scene and retained
source context are unchanged, and bindings/first recipe keep their exact owners.
The original post-Bake full-source-context, exact paint/binding/first-recipe
owner checks remain; it additionally requires the immutable input's complete
encoding and paint-wrapper identity to remain unchanged. Wrong-Original exclusion,
source IDs, native/history, both-role finite normal decoding and `.035` bounds are
unchanged. No production, renderer, frontend, schema, default or threshold changes.

Compilation/runtime qualification of this child is still pending. The existing
whole candidate using tests6c8 and its native90 cohort remain separate; this
fixture correction does not relabel the original failure or replace the combined
owning build. The full authoring objective remains incomplete.
