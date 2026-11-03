# Owned-SAN ordered Save observation

The actual post-REST key replacement completes correctly, but its authoring plus persistence take longer than the broad fixture's combined 120-second Save observer under instrumentation. A focused replay of the exact original inputs passes all 11 owner, restart, Undo/Redo and entire-original-Scene reference checks with ASan/UBSan/leak detection. Its emitted 13,474,903-byte native file is byte-identical to the file that the original failed run eventually wrote: SHA256 `7fb2f9a4c819009c83aae63a8e07fc800342125275edbcb6bbfb51caf8cdb764`.

The diagnostic native process exits 0 in 597.416444s, with no sanitizer findings, no outer timeout and all 17 input hashes unchanged. Scope: one diagnostic test consumer, exact 9050 headers and eight immutable whole-owned/support SAN archives verified against Root's 18-artifact ledger. It is not a fresh whole-core rebuild or a full 134-check workflow pass. No runtime limits, source/archive quotas, failure assertions, numerical tolerances or sanitizer suppressions changed. The original broad test retains its 120-second observation in this checkpoint; the diagnostic explicitly logs crossing 120 and continues under a separate finite 600-second observation/1200-second outer allowance.

Measured wall/aggregate-process CPU seconds on this host:

| Actual phase | Wall | CPU |
| --- | ---: | ---: |
| Key admission |0.000344|0.000402|
| Save admission, queued immediately after key |0.000070|0.000069|
| Real key factory |73.774430|73.126902|
| Prepared result's following deep validation |18.344207|18.246493|
| Scene save: encode/CBOR/atomic write |39.785824|39.609668|
| Total queued Save future wait |131.905801|—|
| Saved Scene load plus exact complete comparison |129.629870|128.624504|
| Exact whole-project Undo/Redo oracle |123.827844|123.866599|
| Entire original completed Scene comparison |92.403603|91.868249|

Snapshots around admission/publication take 40–80 microseconds. Published-state heartbeats show revision 3/asset_busy=false before Scene save begins; authoring and persistence execute in order on the actual worker. Nearly equal CPU/wall times and observed zero swap distinguish substantial CPU work from a stuck queue or dominant queue starvation. This does not establish worst-case latency or eliminate every concurrency defect.

The two prior failures remain preserved. Root's original CTest workflow is a genuine 1800.15-second timeout. The separately copied identical binary exits 1 at `Union ordered Save timeout at native-post-rest-key-replace`; its outer 7200-second allowance did not expire. That run's stage starts 05:43:51.720606UTC and the requested file appears 05:46:30.171673UTC. The stage includes before-Scene encoding and admission, so that 158.451-second interval is not itself the Save future's wait time. The focused wrapper measurements resolve that ambiguity.

The owning source confirms `save_async` queues Save behind an accepted authoring token. It does not wait for work queued behind itself. `wait_asset` observes the accepted publication's `asset_busy` state, not an empty file queue. `Scene::save` includes `Scene::encode`, whose initial deep validation and serialized retained histories are CPU work.

The evidence supports a narrow fixture correction: keep `future=save_async(path)` FIRST, then observe accepted native completion with the existing 120-second bound, asset publication with the existing 120-second bound, and persistence with its existing 120-second bound. Retain the existing mesh endpoint only after the future, followed by the exact saved-Scene and history/owner/pixel oracles. Do not wait for mesh before persistence to borrow an unrelated budget. Log the phases so a future true starvation/slow stage is distinguishable. This phase-separated correction and a measured finite whole-test SAN allowance require their own actual focused/CPU 134/full SAN 134 qualification; none is claimed by this diagnostic.

`tests/source-union-save-observation.cpp` reuses the original broad fixture functions and its real authoring wrapper. The broad test receives only an optional main-reuse guard here; all its original execution, errors, waits, source/history assertions and independent geometry/pixel oracles remain present. Linker wrappers observe real external factory, snapshot, validation and Scene-save calls. No production file is modified.

Compiler: GCC 16.2.1, `-O1 -g1 -DNDEBUG -fsanitize=address,undefined -fno-omit-frame-pointer`, Intel Core i7-12700, unpinned process. The eight archives come from the completed 9050 whole-owned C/C++ sanitizer build; only this test TU is newly compiled. Canonical bitmap/toolkit runtime paths remain unchanged. No GL context, GPU, physical HMD, user desktop, system install or master-tree edit is involved.

Exact commands, old failure receipts, phases, artifact/input hashes and terminal status are in `docs/validation/UNION-SAN-SAVE-OBSERVATION.json`. Large libraries, binary, source snapshots and native files remain in ignored `artifacts/union-save-observation-9050/artifacts`.
