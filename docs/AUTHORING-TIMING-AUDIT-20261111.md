# Read-only authoring timing audit

The current e993 P1 passes the complete original correctness workload, all 42
checkpoints and all 6000 GPU timers. **That is not a performance pass.** Observed
view and GPU tails worsen against the earlier b6 result; no causal attribution
or equality of CPU/GPU clocks is established here. This audit runs no application,
GPU, compiler or benchmark and changes no source, provider, assertion or limit.

| Actual observed repetitions 0 / 1 / 2 | Earlier b6 | Current e993 |
|---|---|---|
| View-pair wall p50, ms | 9.081 / 9.086 / 9.094 | 9.590 / 9.595 / 9.448 |
| View-pair wall p95, ms | 10.592 / 10.464 / 10.553 | 11.311 / 11.303 / 10.993 |
| Two-eye GPU elapsed p95, ms | 9.259 / 9.308 / 9.184 | 12.280 / 12.620 / 12.661 |
| Overall elapsed, s | 273.16 | 300.18 |

GPU mean increases roughly 8.56 to 9.14 ms, while p95 rises roughly 9.25 to
12.52 ms. Minima also fall and both ghost/non-ghost categories widen. Neither
these distributions nor correctness flags identify a shader bottleneck. GPU
elapsed brackets the two eye draws; it is not register-pressure, clock or busy
percentage telemetry. Overall elapsed also includes cold start, excluded
checkpoints and native history. Capture/update timings are nested, not additive.

The actual old link receipt confirms a **fresh b6 renderer TU**, not an old 27f
renderer substituted underneath a newer source tree. b6 already contains the
f473/current-grid preview path. The two render.cpp GLSL literal blocks are
byte-identical (967 and 6492 bytes), as are the complete shader-generator header
`render-mtoon.hpp` and PBR specialization helper. No new shader-source branch
exists between these actual baseline and current renderers. This does not prove
identical driver program binaries or execution clocks.

[The force seal](https://github.com/jebot-git/dome-sculpt/blob/e9931f7286f75746af47d52f2d901b731510cbb5/src/render.cpp#L1228)
runs only when `mesh != cached_`. Unchanged views merely mark the ledger's CPU
use epoch; they neither collect it nor force-seal it. The collector's fence poll
uses a [zero timeout](https://github.com/jebot-git/dome-sculpt/blob/e9931f7286f75746af47d52f2d901b731510cbb5/src/render-store-ledger.hpp#L138).
No new glFinish, nonzero client wait or memory barrier was found in that path.
[Document::frame](https://github.com/jebot-git/dome-sculpt/blob/e9931f7286f75746af47d52f2d901b731510cbb5/src/document.cpp#L185)
loads the same published surface, and
[pick_object](https://github.com/jebot-git/dome-sculpt/blob/e9931f7286f75746af47d52f2d901b731510cbb5/src/document-interaction.cpp#L191)
is a read-only query. A per-view force-flush explanation is therefore unsupported.

The entire sculpt GPU arena source is byte-identical. Eight actual core member
payloads are independently equal across the old/current archives, authenticated
against the old historical link pins: sculpt-mask, local-surface,
sculpt-surface-chunks, sculpt-scene-chunks, sculpt-placed-chunk,
sculpt-placed-chunks, sculpt-chunk-masks and mtoon. Their full hashes are in the
receipt. The observed capture/update CPU means nevertheless move roughly
58/57 to 65/64 ms, and view caller CPU median roughly 1.15 to 1.5 ms. No new mask
reconciliation implementation in those objects explains that change. New native
read/recompose polling is outside the measured stroke/mask path.

There is real **changed-snapshot** overhead worth a bounded future experiment:
[prepare_flat](https://github.com/jebot-git/dome-sculpt/blob/e9931f7286f75746af47d52f2d901b731510cbb5/src/render.cpp#L215)
creates zero-byte packed vertex/index stores and a new VAO even for an empty
packed Mesh, or a new VAO on reuse. Changed-frame publication then inserts the
[material ledger fence/flush](https://github.com/jebot-git/dome-sculpt/blob/e9931f7286f75746af47d52f2d901b731510cbb5/src/render-store-ledger.hpp#L89).
Earlier b6 reused fixed flat names. Benchmark
[buffer counters](https://github.com/jebot-git/dome-sculpt/blob/e9931f7286f75746af47d52f2d901b731510cbb5/tests/sculpt-sustained.cpp#L236)
ignore zero-byte Data/SubData and do not count VAO/fence/flush calls: equal
positive-byte traffic does not rule out this work. It cannot on its own explain
steady unchanged-view tails.

A possible later optimization is an explicit transactional empty-flat state,
plus omitting the shared-ledger seal only when neither domain owns any active
or retired store. It must retain packed-to-empty transitions, accurate accepted
statistics, actual-store latest-use fences/debt, arena Pending rollback/commit
and failure/retry behavior. **It is not implemented or demonstrated as a timing
fix; no recovery, resource accounting or current-field functionality is removed.**

Both records report the same options, initial scene, environment, Arc A770/DG2,
GL4.6 and Mesa26.2.5. The old receipt explicitly permits concurrent CPU development
and records no host-load samples. Current host load is about 7 on 20 logical
CPUs. Neither run records CPU/GPU frequencies, power, thermals or CPU affinity;
other GPU-client activity is not established. Equal clocks, equal contention,
thermal slowdown and a particular root cause therefore cannot be inferred.

A separately cleared paired old/current original workload with clock/core and
GPU frequency/power telemetry would distinguish these hypotheses. No rerun is
performed here. Functional source integration can be considered separately;
the observed production-performance tails and the full authoring goal remain
open work.

[The machine receipt](validation/AUTHORING-TIMING-AUDIT-20261111.json) contains
actual shader/helper/hot-member hashes, immutable raw result/link/status hashes,
per-repetition timings and exact source line references. It does not retag either
historical result or claim a cause, equal clocks or a performance pass.
