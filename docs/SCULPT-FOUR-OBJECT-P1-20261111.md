# Full four-object production workload — 2026-11-11

Private COW-sharing candidate `b6e366fd` passes the original complete four-object
production workload on authenticated Arc A770. The earlier admitted `27f01cad`
failed during cold publication with **Sculpt manifest retained byte limit exceeded**.
This pass preserves the exact original benchmark source, options, native guards
and external 5 GiB RSS envelope. No resource cap or quality setting is raised.

At voxel 0.0125 and 768 × 768 per eye, the actual initial scene contains
1,035,408 triangles, 591,728 vertices and 2,528 chunks. Complete geometry is shared
only between genuine identical COW owners; placement, queries and masks remain
object-specific. Initial retained chunk storage is 228,304,233 bytes. This does
not assert all independently authored fields share storage or fit this budget.

All three repetitions complete: **900 strokes, 360 masks, 6,000 stereo view
pairs, 360 held pairs and six authored placements**, with 42 independent
checkpoints and the unchanged original grouped regression. Exact Cancel,
Undo/Redo, ordered save/load data and stale-generation tests pass. Every timer
has a unique valid sample; pending timers, resets and disjoint observations are
zero. All unchanged view/held rows issue zero buffer calls, allocations,
payloads and dynamic-array uploads.

| Observed timing, milliseconds | Repetition 1 | Repetition 2 | Repetition 3 |
| --- | ---: | ---: | ---: |
| Stereo view pair median | 9.081 | 9.086 | 9.094 |
| Stereo view pair p95 | 10.592 | 10.464 | 10.553 |
| Caller CPU median | 1.156 | 1.153 | 1.164 |
| GPU elapsed median | 8.516 | 8.520 | 8.534 |
| Held stereo pair median | 8.480 | 8.578 | 8.585 |

Total observed wall time is **273.16 seconds**, with external peak RSS
4,941,914,112 bytes. No external guard fires and no protected source/provider
changes. These are uncapped offscreen observations during other CPU development,
not physical headset pacing, isolated GPU busy percentage or a complete matrix.
The workload is untextured sculpting; it does not admit the pending painting
renderer recovery change.

## Exact consumers and evidence

The benchmark is freshly linked from three complete new translation units:
the unchanged benchmark, composed renderer and changed scene-chunk producer.
Its replacement core retains 191 audited current `f47348eb` ABI objects and the
new producer; its render archive replaces the renderer and retains the three
matching palette/polygon objects. All providers are pinned and rechecked.
This is an incremental matched-source composition, not a whole fresh core or
application build. Separate Release/strict-SAN native proof is recorded in
[SCULPT-SHARED-COW-NATIVE-20261111.md](SCULPT-SHARED-COW-NATIVE-20261111.md).

The [sealed receipt](validation/SCULPT-FOUR-OBJECT-P1-20261111.json) pins counts,
history, device witnesses, independent checkpoints and hashes. Raw evidence is
under `artifacts/sustained-four-objects-shared-20261111`; the 12-member archive
`artifacts/sustained-four-objects-shared-evidence-20261111.tar.gz` has SHA-256
`0a8684f26b2d84ff45d8f14bd9790f10eb7e06f10100ddeecb695b8e6989259a`.
Every archived member's content hash was independently verified. Original failed
four-object files remain in their separate namespace.

## Fresh Document reopening remains a separate failure

The benchmark validates loaded native data against saved data. An additional
real `Document.open` probe of its 49 MB accepted file demonstrates that fresh
mesh publication still fails: decoding produces four independent geometry owners
and the 512 MiB manifest limit is exceeded. The corrected probe exits 1 after
122.98 seconds with that exact Document error, peak RSS 1,552,715,776 bytes and no
external guard. No GL renderer is involved. Read the
[separate failure receipt](validation/SCULPT-NATIVE-REOPEN-FAILURE-20261111.json).

Private follow-on `9c521366` adds reader-local sharing of actual fully validated
evaluated COW data only after complete key/value-bit comparison and equal
voxel/band context. It preserves independent layers, masks, histories, native
schema and resource caps; fingerprint candidates are not equality authority.
Work and metadata are bounded. The private pool is scoped to native reading;
Volume decoding passes its actual evaluated storage after complete validation.
Public headers remain byte-exact with `b6e366fd`. An earlier unexecuted friend-
declaration prototype `e3ccf2a0` was superseded: layout compatibility alone would
not justify combining token-different class definitions with reused objects.
The new design replaces both complete Scene and Volume translation units. Its
native and real reopening qualification is pending. Neither this private reader change nor the broader preview composition
is admitted by this workload pass. The full goal remains active and incomplete.
