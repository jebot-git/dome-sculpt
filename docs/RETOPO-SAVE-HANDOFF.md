# Immediate retopology Apply and ordered Save — private component

The affine checkpoint exposed a real ordering gap: Apply returned while its
independent retopology worker still had to publish AssetWork. An immediate Save
could therefore refuse. Holding only the later atlas stage did not fix that gap.
The original failed invocation remains unchanged in the affine receipt.

Apply now stages an accepted Scene operation on the existing file queue before
returning. A small private RetopoJob payload carries Pending/Accepted/Rejected
admission and the exact independent worker result. The file operation waits on
the existing retopology condition variable outside the model lock. It waits only
on an independent worker, never a later file-queue entry. Cancellation/replacement/
shutdown notify that predicate. No product sleep, timer, polling fallback, Scene
field or serialized authority is added. Unchanged Apply retains its old no-op
history semantics. Queue failure revokes the staged input node and restores the
exact draft; there is no throwing view publication after acceptance commits.

The existing `deque<RetopoJob>` field remains in place. The nested element gains
a private shared handoff; actual public type sizes/alignments equal main10944:
Document6968/8, EditorState1896/8, Scene72/8, RetopoView144/8 and workspace392/8.
Every actual element accessor in document-retopo.cpp is rebuilt, together with
Document constructor/destructor instantiations and the cancellation TU. The two
complete qualified topology objects are reused with unchanged source/header
closure. Immutable mainc179 archives are only read; no archive is rewritten.

Matching Release and ASAN/UBSAN each pass1445 checks:858 immediate-handoff controls
(including all63 measured persistent caller-side admission allocation denials),
394 prior affine checks and62+131 original authoring/edge checks. Actual immediate
Save is queued while the input worker is held **before real finalization and
unwrap**. Cancel, invalid/injected worker exceptions, replacement, accepted-Save
shutdown, no-op history, real file-queue capacity and all failed-admission retries
are checked.21 actual native files and5intentional prior-file sentinels are
byte-identical across profiles. The cumulative fixture counter prints1252 at the
affine phase (858+394); this is not1252additional affine checks.

Exact source/commands/hashes, layout probes and scopes are in
`validation/RETOPO-SAVE-HANDOFF.json`. New registered target:
`dome-sculpt-retopo-save-handoff-test` / `retopology-save-handoff`. Optional component
proof composes the complete test TUs into one executable to fit the coordinator's
1.5GiB allocation cap. The earlier failed composed executable is preserved in
its verified53,208,183-byte zstd archive with exact restoration metadata; all
failure logs/source provenance remain. No suppressions are used.

This is not fresh whole-application, FLTK/XR client, GPU or full regression proof.
The63prefixes cover calling-thread input/file admission, not every downstream
atlas/file-worker allocator. Actual external avatar graphs, larger production
fixtures and other authoring requirements remain. Root's triangle-remesh work
and the separate CATS/texture-set gaps are still real implementation work.
