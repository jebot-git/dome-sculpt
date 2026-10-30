# Cancellable native UV atlas work

The existing three-argument `unwrap(mesh, resolution, padding)` ABI and its numeric output remain unchanged. A new four-argument overload accepts an optional cancellation predicate. All preparation and atlas mutation operate on a detached result; cancellation, callback exceptions and vendor failure leave the input geometry, UVs, seams, pins and ordering exact.

The predicate runs at bounded host-loop checkpoints and xatlas progress callbacks, which can arrive from vendor worker threads. Calls are serialized per operation. Exceptions are captured inside the noexcept vendor callback and rethrown on the caller after vendor tasks have unwound. The stack callback context outlives xatlas destruction and outstanding AddMesh tasks. Independent concurrent operations never share cancellation state.

Cancellation is cooperative. A single vendor chart iteration can run between progress callbacks; this API does not promise a hard latency bound or forcefully terminate xatlas threads. Empty predicates do not install vendor callbacks. The polygon authoring worker will use the overload for detached conversion; this checkpoint alone does not implement desktop or XR workflow controls.

Fresh CPU Release checks passed both suites in 15.25 seconds: 12 cancellation checks and the existing 17 atlas robustness checks. Fresh complete application core/support C and C++ ASan/UBSan with leak checking passed the same two suites in 66.88 seconds. Tests observe actual chart generation, packing and output progress through a test-only linker wrapper, exercise cancellation before work/mid-chart/mid-pack/late output, typed and non-standard predicate exceptions, task-join safety, exact retry, and two concurrent contexts with independent outcomes. The original three-argument and empty-overload meshes and atlas diagnostics compare exactly.

Evidence: `artifacts/tests-atlas-cancel-release.log`, `artifacts/tests-atlas-cancel-sanitize.log`, and the atlas cancellation source manifest. No hardware execution or full editor workflow is claimed by this CPU atlas checkpoint.

The flat-normal import integration replaces the original fixture's polygon-cube
dependency with an explicit native six-face cube. Its13 checks retain every
cancellation oracle and add actual missing-normal/missing-UV authoring import:
the shared predicate cancels from a real xatlas chart callback and the caller
Scene stays exact. CPU/OneAPI and complete-own-core/support sanitizer checks pass.
See GLTF-FLAT-NORMALS.md and its scoped receipt; other worker callers still need
the overload, and no hard vendor-iteration latency bound is claimed.

## Authoring-worker adoption

Scene atlasing, retopology publication and both plane-cut shell/cap atlases now
forward their existing cancellation predicate to the four-argument overload.
Painting-mesh preparation registers a transactional AssetWork token, exposes the
existing native/spatial Cancel action and forwards that token, shutdown and
edit-authority invalidation through extraction, normal conditioning and atlas
work. Cancellation clears busy/token state without adding history; successful
retry retains a single Undo/Redo operation.

The new authoring fixture cancels from actual vendor chart callbacks, including
separately the cut shell and cap. Its 18 assertions preserve original Scene,
paint/volume owners and topology, retry every path, and exercise actual Document
Cancel while its ordered worker is inside chart generation. The same fixture
fails against the previous call sites, demonstrating the uncovered behavior.
Six Release suites pass (11.95 seconds), including shared Document, cut,
retopology, snapshot export and persistent native-stroke allocation regressions.
Whole-core sanitizer qualification of this followup is recorded separately by
its integration receipt; the preceding import-safety proof is not relabeled.

Accepted snapshot exports continue draining when the application closes. They
must not inherit a global shutdown predicate that silently cancels already
accepted output. Explicit export-job cancellation remains required work, along
with cooperative vendor iteration latency/performance qualification. The legacy
three-argument API and its default numerical behavior remain available.
