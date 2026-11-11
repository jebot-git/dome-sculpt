# Combined owning build and faithful color observer — 2026-11-11

Frozen private source `a904a2d644eca7885e38e51528057790232e8418` completed
both fresh Release and strict ASAN/LSAN/UBSAN builds. Each started with zero
project objects and built all 63 selected owning targets, including the actual
FLTK/OpenXR application, core, renderer and test consumers. Each closure has
381 authenticated outputs, including 305 project objects. Both configurations
use the canonical DomeD `acc2b70` toolkit; all 418 SDK pins and all source pins
remained unchanged. Build times were 883.36s and 1754.29s respectively.

**The candidate is not admitted to main.** The selected CTest run passed 48/53
in each profile, with the same five failures. The original commands, failures,
logs and binaries remain separate from subsequent repairs. Required workspace
checks, which CTest failure prevented the coordinator from reaching, subsequently
passed 27 chunk and 373 placed-owner controls in each owning profile. These
supplemental executions did not repeat already completed positive tests.

| Original failing gate | Finding and current disposition |
| --- | --- |
| `authenticated-local-surface` | Compared CPU render images which are now intentionally empty for the GPU field. Test-only `e81ac048` uses complete native/public-preview oracles and authentic immutable GPU publication; Release/SAN each pass 96 controls. |
| `document-render-capacity` | Real product failure: one public SurfaceQuery rejects a packed aggregate above its standalone 2M vertex/triangle bounds. A bounded private query composition is being implemented; no capacity pass is claimed. |
| `document-render-buffer-reuse` | Expected the retired flat SDF representation. Private test-only adaptation checks actual chunk/scalar uploads, zero unchanged traffic, geometry ownership and real paint Undo/Redo; focused Release/SAN each pass 31. |
| `source-color-render` | Used an obsolete padded vertex-byte forecast. Private test-only correction retains the original image/native oracles; its focused Release run passes 100. |
| `source-extra-uv-render` | Used the obsolete 72-byte forecast instead of the actual native arrays plus extra UV payload. Private test-only correction retains the original oracles; its focused Release run passes 55. |

Read [renderer recovery](RENDERER-FRAME-RECOVERY-20261111.md) for the separate
focused renderer evidence, precise fixture-retention limits and the genuine
capacity failure. Neither correcting observations nor passing the build resolves
the query admission failure. Original 4.8M-vertex and indexed inputs remain
required, alongside a distinct actual above-256MiB buffer test. Standalone
SurfaceQuery limits, rendering data, picking and resource bounds stay intact.

The root-owned color observer change is strictly in
`tests/local-surface-integration.cpp`. It retains the original actual worker
paint action, geometry identity, allocation-failure and recovery assertions.
Before/after complete native scenes must differ; both explicit public CPU
preview oracles must contain exactly 64×64 RGBA pixels and differ. The actual
render snapshots must publish distinct sealed contribution owners while their
render-only CPU color images remain empty. Thus an empty-image comparison is
replaced with independent evidence of real paint and the current GPU route.

For `e81ac048`, one complete test translation unit was compiled and freshly
linked per profile against that profile's complete owning `a904a2d6` providers.
Product source/header/assets/third-party bytes are unchanged. These are focused
test-only consumer builds, not a rerun or repair of the frozen whole CTest run.
Release passed 96 controls in 0.816s; strict sanitizer passed 96 in 7.027s with
`detect_leaks=1`, ASAN/UBSAN halt-on-error and no leak suppressions. All provider
hashes remained unchanged. No GPU, hardware-renderer or headset qualification
is claimed by this native test.

The [machine receipt](validation/OWNING-BUILD-OBSERVER-20261111.json) records
exact source, commands, provider hashes and binary/log hashes. Its content-verified
archive retains original CTest failures, whole-build terminal receipts,
supplemental workspace receipts, original/corrected test sources and the narrow
consumer compile/link/run evidence. Generated owning binaries remain local.

Actual current-closure software/Arc image and public desktop/XR workflows are
qualified separately. Final query integration, necessary owning consumers and
capacity/picking checks, production workload qualification and the complete
remaining authoring/interoperability/accelerator matrix remain required. The
full original goal and later controller trays/spatial node editing/history
remain active and incomplete. No installation or physical session change occurs.
