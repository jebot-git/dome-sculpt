# xatlas worker shutdown

The pinned MIT xatlas source remains based on upstream
`f700c7790aaa030e794b52ba7791a05c085faf0c`. Its local shutdown patch acquires
an individual worker's predicate mutex before publishing its final wakeup and
notifying. Chart generation, packing and numerical output are unchanged.

A real repeated current-color cut allocation sweep hung in `xatlas::Destroy`:
the main thread was joining a worker while all remaining workers waited on
condition variables. Their predicate was an atomic flag, but the destructor
could publish and notify between the predicate's false test and registration
with the platform wait. An atomic flag alone does not close that gap.

`xatlas-shutdown-handshake` intercepts the actual standard-library wait before
platform registration while the worker still holds its mutex. It releases the
worker only when the destructor requests that same mutex. Notification before
that request fails immediately. The unchanged dependency fails deterministically
with exit 3 and the message “Atlas shutdown notifies without acquiring the worker
predicate mutex”. The patched dependency passes 32 independently forked
handshakes. The regression contains no production test hook or timing-dependent
success condition; its 30-second timeout detects a hang only.

On 2026-11-02, a fresh optimized standalone dependency build passed the fixture
in 0.04 seconds. Fresh C++ xatlas and fixture ASan/UBSan instrumentation with leak
detection passed in 0.38 seconds. This is scoped dependency synchronization proof,
not a full application, geometry-numerics, universal heap-exhaustion or cut
workflow qualification. The separate cut preparation allocation sweep remains
part of its own milestone.
