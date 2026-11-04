# Source4 UV2 geometric keys: native worker and independent pixels

Private test-only child of b6d402b, 2026-11-04. A genuinely imported triangle has
independent writable atlas UV0, original UV0/UV1/UV2 and a normal image on UV2.
Its original tangent accessor is absent; the actual importer owns the generated
UV2 normal/tangent frames. Native Replace applies nontrivial shear/scale and
out-of-plane displacement to the existing key. Immediately queued Save, restart,
one Undo/Redo and complete first-source Restore preserve exact paint/source data.
The contribution overlay remains pristine; no painted-standard-export admission
or image-role rewrite is inferred from geometry-key authoring.

Release, owned ASAN/UBSAN/leak checking and actual PCI03 Intel Arc A770 each pass
45 assertions and nine full-image comparisons over three mixed-joint poses and
morph weights0/.35/1. Actual Document mesh snapshots and its GLB file worker feed
a persistent, independently parsed cgltf/GL shader consumer. Every comparison
has maximum byte error0 at the unchanged bound4, with visible coverage above128.
Choosing wrong normal UV0 gives92 error; the authored key differs from its
untouched original by255. Scene and immutable surface identities remain unchanged
by rendering/export. A forced llvmpipe `--hardware` run refuses exactly as required.

Scope: the complete new test TU was compiled against copied, matching coherent
owned Release or whole-owned sanitizer static archives. All product/header/asset
bytes match that parent. This is not a fresh whole app build, new API/layout/schema,
physical headset test or a new VRM/MToon/real-avatar image proof. The generator
only gains a no-main test inclusion guard; its original main remains unchanged.
New CMake registration is conditional on testing and the real renderer target.
Configuration-only checks show599 tests here, and no test target for testing-off
headless configuration. They are not compilation proof of those configurations.

All27 actual worker GLBs plus three constructed input GLBs pass pinned Khronos
2.0.0-dev.3.10 with maxIssues0: zero errors,33 warnings,102 infos and30 hints.
The native outputs contribute27 non-root-skinned-mesh warnings; the constructed
inputs contribute three alpha-cutoff and three generated-tangent warnings.
All11 file cohorts (input, nine worker GLBs and keyed native Scene) are byte-exact
across Release/sanitizer/Arc. No warning is filtered or relabeled.

See [the receipt](validation/SOURCE4-KEY-EXPORT-PIXELS.json) for commands, source,
archive/binary/output hashes, exact component scope and initial nested-main macro
compile failure. The broader native139 workflow/SAN continuation and next
coherent application union remain independent; all unfinished original/additional
requirements and deferred controller drawers remain active. No system install,
active session, physical headset or default driver changed.
