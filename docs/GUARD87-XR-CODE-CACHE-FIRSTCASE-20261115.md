# First XR case with an initially empty case-scoped code cache

Current MToon-quarter completes194 actual real-loader stereo checks in175.953
seconds under the unchanged240-second and6000-frame limits. Outer tool19689 and
the actual phase exit0; all source/input drift and owned survivors are zero.
This is App3d164382, test reporter c54f66f8 and operational coordinator02d17f29.
It does not claim a whole application build from either test child.

The original cache-disabled, fresh-per-App MToon-quarter run remains an actual
240-second timeout after174 passing checks. Its inputs and partial report remain
unchanged; it is not relabeled successful. Read-only timing shows source views
need only3-5 captures per eye and roughly0.07 seconds to drain after the external
gate, while repeated start-to-qualified latency is about8.4 seconds for PBR and
12.9 for MToon. Those are observed boundaries, not exact internal shader timings.

The new operational child changes only shader CODE caching. Each coordinator
case creates its own initially empty cache; its19 fresh App processes share that
code cache under normal Mesa defaults. Every App still has separate fresh scene,
configuration, state, cache and runtime directories; only the explicit Mesa code
cache is shared. No scene, image, framebuffer, native-state or prior-case/user
cache is reused. Application/core/producers, GL shader source and driver ELFs
remain unchanged. All29 original functional check expressions are AST-identical,
including the original four probes per eye and RGB/geometry/normal bounds3/12/7.

Measured evidence establishes the cache behavior. The first actual App starts
before any cache entry exists and reports0 disk hits,27 misses and28 created
files. The18 subsequent distinct App PIDs produce443 disk hits in total. The
original source view creates five additional shader variants; following ordinary
source/control Apps report25 hits and0 misses and finish around8.5-8.8 seconds,
compared with the original roughly13-second MToon cold setup. Complete per-App
statistics, actual cache-file hashes and timings are retained. Negative pixel
controls still execute and reject wrong normal data; cached code is not cached
rendered data.

This first-case milestone is not the final four-case operational matrix. Each
remaining case must execute under the same config with its own initially empty
cache. Earlier cache-disabled PBR passes remain separate historical evidence.
There is no physical-headset, performance, combined-draft or full-goal completion
claim. The [receipt](validation/guard87-XR-codecache-firstcase-20261115.json)
contains exact source IDs, hashes, current status/report/cache proof and the
preserved original timeout/timing and operational diffs.
