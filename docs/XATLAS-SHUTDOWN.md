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

## Native thread construction failures

A follow-up trace of the real cut fault at owned allocation 7288 showed a
second issue after the predicate synchronization fix. `TaskScheduler` uses raw
`Array<Worker>` storage: if a later `std::thread` construction threw, the already
started worker prefix was not joined before that storage unwound. The context
and placement storage also lacked cleanup. An orphan could subsequently wait
on reused worker storage and interfere with another atlas's shutdown.

The local constructor patch joins every initialized worker before unwinding,
handles a constructed `Worker` whose thread never started, and frees failed
thread/scheduler placement storage and the enclosing context. There are no new
public fields or APIs and no chart/packing arithmetic changes.

`xatlas-construction-allocation` observes the actual first worker entering wait
before persistently denying the next thread-state allocation. The unchanged
453a532 dependency, linked intentionally to the final fixture, fails at boundary
2 with “Failed Atlas construction left a live native worker”. The test requires
all actual native worker tasks to disappear, then performs an exact successful
retry after each failure. A bounded wait accommodates kernel `/proc` task-entry
removal after `pthread_join`; successful cleanup requires the actual task count,
never elapsed time. The old leaked worker remains present until the failure
bound.

On this 20-thread machine all 19 measured native thread-state allocation
boundaries fail, unwind, join, and retry correctly. Both standalone dependency
suites passed 30 consecutive optimized repetitions in 3.75 seconds and five
fresh xatlas/fixture ASan/UBSan/leak repetitions in 3.03 seconds. These probes
intercept C++ thread-state `operator new`, not xatlas's C allocation callbacks or
OS-wide resource failures. Full cut allocation/publication qualification remains
a separate requirement; the dependency probes alone do not establish it.
