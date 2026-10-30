# Cancellable native UV atlas work

The existing three-argument `unwrap(mesh, resolution, padding)` ABI and its numeric output remain unchanged. A new four-argument overload accepts an optional cancellation predicate. All preparation and atlas mutation operate on a detached result; cancellation, callback exceptions and vendor failure leave the input geometry, UVs, seams, pins and ordering exact.

The predicate runs at bounded host-loop checkpoints and xatlas progress callbacks, which can arrive from vendor worker threads. Calls are serialized per operation. Exceptions are captured inside the noexcept vendor callback and rethrown on the caller after vendor tasks have unwound. The stack callback context outlives xatlas destruction and outstanding AddMesh tasks. Independent concurrent operations never share cancellation state.

Cancellation is cooperative. A single vendor chart iteration can run between progress callbacks; this API does not promise a hard latency bound or forcefully terminate xatlas threads. Empty predicates do not install vendor callbacks. The polygon authoring worker will use the overload for detached conversion; this checkpoint alone does not implement desktop or XR workflow controls.

Fresh CPU Release checks passed both suites in 15.25 seconds: 12 cancellation checks and the existing 17 atlas robustness checks. Fresh complete application core/support C and C++ ASan/UBSan with leak checking passed the same two suites in 66.88 seconds. Tests observe actual chart generation, packing and output progress through a test-only linker wrapper, exercise cancellation before work/mid-chart/mid-pack/late output, typed and non-standard predicate exceptions, task-join safety, exact retry, and two concurrent contexts with independent outcomes. The original three-argument and empty-overload meshes and atlas diagnostics compare exactly.

Evidence: `artifacts/tests-atlas-cancel-release.log`, `artifacts/tests-atlas-cancel-sanitize.log`, and the atlas cancellation source manifest. No hardware execution or full editor workflow is claimed by this CPU atlas checkpoint.
