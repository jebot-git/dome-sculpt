# Worker failure publication under allocation denial

The input and file workers now retain bounded error notices when reporting a
failure cannot allocate. Previously each worker caught the original exception,
then terminated when copying the editor state or error text threw again.
Actual worker-thread denial reproduced SIGABRT (-6) in controller motion,
generic source-validation failure, and accepted export failure before commit.
The original negatives and byte-matched 63df/4cf archive manifest remain under
`artifacts/baseline-*`; no injected site was disabled to make them pass.

Each worker owns one optional notice containing the original exception_ptr,
generation, previous publication owner, bounded metadata and an existing promise.
No Scene, mesh or image copy is retained in this fallback. Failed UI publication
retries with a 25ms wait on that same worker. A busy file job cannot delay input
recovery. With no pending notice, both workers retain their indefinite idle wait.
The CV predicate flags use their respective mutexes, avoiding lost wakeups.

Nonallocating asset/UV/export bookkeeping precedes error formatting. Interrupted
held motion is cancelled on recovery only while its generation, object and edit
authority still match the failed publication; authoritative source/history stay
unchanged. Old notices cannot cancel a newer actor or overwrite a replacement
document. Retopo/cut/import view repair is deferred where it needs allocation.
Normal error publication completes before its future becomes ready. If that
publication cannot allocate, the original exception still reaches the promise
immediately and the bounded notice retries UI status later. An allocation
failure during delivery retains that promise and applies backpressure before
more file work. Accepted snapshots and export tokens keep their original drain,
generation and atomic-writer semantics. Late export failure retires counters
without applying the old document's error to the replacement.

The diagnostic shown in the UI is bounded UTF-8 (512 bytes); malformed exception
text gets a fixed message. Futures retain the original exception independently
of this display bound. The most recent pending failure per worker is retained;
this is status recovery, not a persistent error log.

Release qualification currently passes seven actual worker modes, 51 assertions:
controller failure/re-grip, file validation failure/retry/exact Undo, accepted
export promise/output preservation, controller recovery during blocked held work,
and generation replacement for input, generic file and accepted export work.
The replacement export case verifies pending_exports returns to zero without an
old-generation error. Denial is persistent on the named C++ worker thread for
60ms, including error reporting and unwinding. Reported denied-allocation counts
are retry attempts, not exhaustive unique-site counts or an OS-wide OOM test.
All seven existing authoring/recovery/export/held/native regressions also pass:
14 selected CTest suites total in 9.90s. The first fully instrumented run passed
13/14 and exposed a readiness race in the existing document-export assertion:
the exception future could wake before normal native status publication. Its
assertion is unchanged; a narrow ordering correction passes eight focused Release
suites (7.95s), and the complete sanitizer gate is running again. No sanitizer
completion is claimed at this source checkpoint.

This is isolated from the affine/Scene13 work and adds no Scene/archive fields,
new runtime thread, system dependency, installation or display-session changes.
Private Document layout changes require coherent rebuilding by consumers.

Native gesture, mesh, UV, retopo and other worker catch-publication paths are
outside this narrow input/file qualification and remain in the OOM audit scope.
