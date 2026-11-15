# Explicit test reporter overlay and first real-loader stereo case

The first real-loader Source8 attempt passes41 checks, including actual public
controller Bake/Save/UndoRedo, exact native reopen and stereo ownership, then
fails in the test runtime's first independent source-view report. A new JSON eye
slot is null; calling value(frames,0) on it raises type_error306 during release.
The actual App logs the loader error. The original runtime.json contains
source8_view{0:null}; old outputs and the actual1 remain intact.

Source c54f66f8 changes only tests/source8-public-runtime.cpp: initialize that null
diagnostic slot to Json::object before counting actual released frames. It does
not alter product code, headers, ABI, wire, source inputs, the scenario, pixel
oracles, image capture, actions or budgets. The frozen application/core/producers
remain exact source3d164382. A distinct explicit test-runtime overlay is used;
there is no claim that an application from the child source was whole-built.

The complete current test TU compiles successfully with literal flags/defines
unchanged. Its379 genuine -MD/-MF dependencies match prebuild physical input
expectations and all child headers are byte-identical. A new shared link has41
actual pre-pinned linker dependencies, identical SONAME/NEEDED and all186 dynamic
export names/types. All29 actual loader providers match the current preflight.
The fresh module is58d18b30; its separate overlay admission isdb8780b5. Original
1768 owning outputs stay untouched and pinned; the new module path/hash is added
with conflict checks, never written over the original runtime expectation.

Two proof-harness failures remain separate: a first lexical tests/../include key
lookup after actual compile/link0, and a later parser consuming GNUld's phony
rules before any new link. Physical dependency identities and the first genuine
GNUld dependency rule close those proofs; the final new shared link exits0.
No compiler dependency or historical input hash is fabricated.

With this explicit App3d/TestRuntimec54 boundary, PBR-quarter completes194 actual
checks in174.07 seconds on authenticated Arc hardware. Outer tool51706 and the
phase both exit0, with zero input drift and owned survivors. Nineteen actual App
processes cover public authoring/history and18 independent both-eye source/view
sessions. All four probes in each eye retain correct<=3, geometry>=12 and
wrong-normal>=7 checks plus non-clipped equality controls. Real loader frame,
image, swapchain, worker and session ownership balances under the unchanged
240-second and6000-frame ceilings. There is no physical-headset or performance
claim. Other XR cases remain independent pending qualifications.

The [receipt](validation/guard87-XR-reporter-overlay-firstcase-20261115.json)
preserves original failures, full compile/link/ABI proofs, actual per-view runtime
reports, map/DRM observations and independent per-eye RGB/sensitivity results.
This milestone does not admit a new mainruntime or complete the production goal.
