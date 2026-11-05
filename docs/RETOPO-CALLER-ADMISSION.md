# Retopology caller/input admission — Release checkpoint

The current Release checkpoint repairs queue admission across public discrete commands, desktop gestures/history, shared controller frames, workspace Open/Cancel, projection settings, spatial startup/rebase and shutdown/menu cancellation. The runtime parent is main `37c46b9`; subsequent main `c55dc2f` changes metadata only. **Sanitizer, final-source legacy regressions and ordinary app qualification remain pending. This is not main admission or full-goal completion.**

The first public-only checkpoint `014f1f4` is preserved. It repaired Face while leaving the broader routes under audit. Its 4,031 Release checks and old worker controls do not establish final-source sanitizer or app qualification.

## Genuine failures and repair

Old code queued work before the calling input publication could still fail. A refused Face could later create a hidden face. The preserved counter uses a healthy accepted PlaceMode FIFO barrier, not an earlier busy=false snapshot. Eleven additional old-main counters expose actual Open, Begin/Sample/End/Cancel, Undo/Redo, distance and rebase ownership/publication failures. Spatial startup also published rebase and then attempted another fallible error-clear publication; the real old last-boundary counter fails after the first change was already visible.

The new private `RetopoInputScope` snapshots the real input/view/flags before callbacks and stages one pending batch. The worker leaves a pending batch in the deque; only the model-lock-owned caller may append/coalesce its steps. No accepted payload is mutated. Actual queued operations are charged prospectively, retaining 254 ordinary operations and 256 finalizer capacity rather than multiplying capacity by batch size. Publication accepts the batch; exceptions reject/remove it and restore state with nonthrowing moves. No condition wait occurs under the model lock.

One detached worker candidate executes the complete frame. A genuine old EndGesture→Undo counter reaches both edits and a real late worker publication throw, then exposes a hidden Redo entry from the partial End commit. The repaired batch retains the unfinished gesture and its exact history. The earlier held-point attempt without current mesh readiness failed before that gate and is retained as an invalid measurement, not a successful counter.

Workspace opening stages the actual immutable Scene and next session until publication. Workspace cancellation defers retirement; cancellation of accepted asset work defers its cancellation flag. Only `job.accept` owns the file handoff. An ordinary input token has no AssetWork and never dereferences file-owner fields. Spatial begin and rebase share one locked preparation helper and one publication. Epoch retirement preserves actual new-epoch input queued after publication. A terminal Begin/Apply/Cancel consumes its controller frame; existing published hand buttons provide press latches so held simultaneous history cannot immediately target the new namespace.

Permission transitions retain their existing explicit policy of retiring unaccepted tools. `invalidate_object_authority_locked` now retires the whole retopology workspace before generic gesture cancellation, avoiding a redundant Cancel enqueue into a workspace being destroyed. This is not a rewrite or an atomicity claim for the separate permission-transition policy.

## Actual Release evidence

Four complete product TUs—Document, DocumentRetopo, worker errors and interaction—were freshly compiled. The complete current native consumer passes **13,380 checks**, including:

- **710 persistent caller allocation refusals:** 424 public/lifecycle/controller-route boundaries, 58 shutdown/menu cancellation boundaries and 228 normal/held public discrete boundaries. Counts are measured repeatedly on real operations, not assumed from an earlier build.
- Eight explicit calling-thread publication throws; real held Cancel+Face and same-frame End+Undo late-worker publication failures with exact history/retry controls.
- Actual shared controller Apply/Cancel with simultaneous history, held-next-frame suppression, fresh release/press, ordered Apply→Save and exact native reopen.
- 254 immutable queued Move samples plus reserved End, quota refusal with exact caller view, one Undo/Redo, genuine current-epoch producer preservation, source/session replacement and recovery.

All final source/object/archive before/after hashes match. A same-binary provenance replay also passes and records **75 actual mapped file inputs**, three mapping snapshots and **four exact native output pairs**. `/etc/ld.so.cache` is explicitly pinned as loader configuration. The earlier audit's native exit 0 / aggregate exit 1 for that omitted pin remains preserved. There is no GPU, loader/XR or GUI claim in these native controls.

The first tail run stopped at the genuine new source-context gap during source replacement. That failure, all failed binaries, the wrong-page fixture run, both compile-only fixture errors, the dependency-output routing failure and every valid old-code counter remain preserved under ignored `artifacts/caller-proof`. `RETOPO-CALLER-ADMISSION.json` separates those statuses from final native exit 0.

## Exact component boundary

Compiler layout probes give Document 6968/8, EditorState 1896/8, Scene 72/8 and RetopoJob 72/8, unchanged. The private acceptance pointee grows **112→168 bytes**, allocated/accessed by the complete new DocumentRetopo TU; the private scope is 16/8. The outer classes gain no fields, Scene schema, command IDs or new frontend.

The current-main polygon payload differs from the original 9dd baseline, so old C179 polygon providers are not reused. The native link uses the exact qualified whole current-main DocPolygon/PolygonAuthoring/Conversion/PolygonRender objects and matching immutable supports. The link audit proves exclusion of all eight superseded Doc/interaction/polygon archive members. This is a matching component build, not a whole-core/app rebuild or public-header byte-equality claim.

The final recorded new growth is 179,171,328 bytes, within the 176 MiB Release allowance. Source reflinks are recorded separately; the forthcoming total task cap includes them. The hard actual-free floor is 20 GiB. No installation, active-session change, physical headset action or hardware job occurred.

## Remaining qualification

The first finite sanitizer cohort is authorized but not yet run in this receipt. Final-source worker 251/42/97 controls and legacy 1267/1445/26 controls, generated CTest execution and actual ordinary app normal/compact/shared-loader tests are still required. Source CMake registers both caller and input admission consumers; source registration is not a CTest execution claim. Full production admission will preserve these boundaries and all earlier negative evidence.
