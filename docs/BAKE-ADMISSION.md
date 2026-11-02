# Atomic native Bake admission

Bake previously queued and published accepted asset work, then changed its
configuration and performed a second fallible publication. If that second
publication ran out of memory, the caller reported failure with a job already in
the queue and changed internal/public state.

A private plain-data `BakeAdmission` now carries source/settings into the existing
queue transaction. That transaction installs the configuration and accepted work
in one publication. The caller performs no fallible publication afterward. There
is no nested transaction, callback, FileJob data member or relaxed scene-edit
permission; the public Document object layout is unchanged. The private queue
signature changes, so all affected consumers are rebuilt coherently.

## Exact validation

This isolated branch is based on public `04d3f4a`, without Scene20 or target-color
work. Fresh optimized regular-user Release compiles the complete core/support.
Three selected suites pass in3.85 seconds: all90 measured caller allocations,
existing controller color policy and the actual colored native Bake/UV/restart
workflow. A separately fresh own-core/C/C++ support ASan/UBSan build with leak
detection passes the same three suites in39.16 seconds, including every one of
those90 persistent caller-thread allocation failures and real full-helper retry.
External system libraries are not claimed instrumented.

The fixture uses actual imported original-accessor assets, source images,
authored frames, skin, morphs and editable paint, with previous history/redo.
An earlier real Save is gated at its actual file `fsync`, preventing the asset
worker from consuming the tested queue prematurely. Each failure checks exact
Scene, published state/mesh, content/next-content/revision, history/redo,
source/control/input/work leases, Bake configuration and empty queue. The same
observed controller menu lease then starts exactly one real Bake. Pending generic
Save remains explicitly refused; after completion the accepted result saves and
one Undo restores the entire source state. No production injection hook is added.

The matching old public04 headers and frozen archives from `vrm-atlas-ec` produce
an intentional negative:110 measured caller allocations, then failure at91 changes
publication/busy state with one job queued after `bad_alloc`. The frozen product
and its proof files were read only. The new90-boundary result keeps every original
assertion and reflects removal of the redundant publication.

This is direct native `bake_from_object` caller admission. It does not claim
input-thread failure injection, generic queued Save support, all asset transaction
OOM coverage, a new Scene20 qualification, hardware execution, installation or a
physical headset test. The existing spatial input loop can still echo publication
after non-import commands; that broader input-thread scope is separate.
