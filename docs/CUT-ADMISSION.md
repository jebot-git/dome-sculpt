# Transactional cut Apply admission

This follow-up fixes a pre-queue allocation hole found after the scoped held-cut
publication qualification. Previously `end_cut_locked(true)` assigned a new
committing view before allocating its report, work token and structural permit.
Failure there left the internal draft committing with no queued job. A second
publication in the public `end_cut` wrapper could also fail after a job was
already admitted. Post-helper publication fault tests did not cover either gap.

The report, work token, accepting view, permit and operation closure are now
staged without changing the document. The private cut permit pins both the exact
ready draft and detached accepting view. Queue admission validates that pair;
its existing transaction then installs the accepting view, cancels input,
attaches the work lease and publishes the queued operation together. Allocation
failure restores the exact ready draft and input state and removes any staged
job. There is no nested transaction or replacement draft allocated in a catch.
The public accepting wrapper uses the publication already completed by that
transaction, with no second fallible publication after admission. Cancel retains
its normal feedback publication.

`cut-color-admission` denies every measured normal `operator new` allocation in
the actual public `end_cut(true)` call on the calling thread, persistently until
unwind. An actual earlier Save is gated at file fsync so no detached cut work can
race that scope. The test does not replace preparation with a stub: each failure
retries through the same real controller menu lease, runs the real helper and
writes an ordered Save, then verifies one exact source-retaining Undo.

All 181 measured changed-Apply admission boundaries pass in optimized Release
and a fresh own-core/C/C++-support ASan/UBSan/leak build. At each boundary the
ready view's internal and published shared identities, scene, source/mesh cache,
history/redo/content, held/selection/epoch, source lease, observed menu/input
lease and queue stay exact. Save refuses the unaccepted draft without writing
old output bytes or capturing a nonexistent result. Successful retry persists
the actual accepted held shell. A regression binary built with the frozen
parent's matching old headers and qualified old archives fails the exact internal
draft/lease invariant at boundary 2. The frozen proof tree was not modified.

The fixed product also passes the 157 native/controller checks, actual FLTK
holding workflow (16 checks), real OpenXR loader/software-GL workflow (21 checks),
and 32 selected sanitizer post-helper publication faults (first16/last16 of
10089 measured). These are separate scopes: the new exhaustive 181-boundary
claim is direct changed-cut admission only. Parent `09b6d38` retains its previous
broader union tests; this follow-up does not claim a new all-native/OneAPI run,
recovery-on allocation sweep or universal transaction hardening. Moving-grip
pose+cut history and current-field target Bake remain required work.
