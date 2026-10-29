# Worker shutdown and condition-variable handshakes

A combined XR run timed out after its runtime had finished all frames and
released the session. A focused replay completed, so simply extending the timeout
would conceal an intermittent lifecycle defect. The Document stop flag was
atomic, but shutdown set it and notified geometry/input/UV waiters without their
predicate mutexes. A worker could check false, receive the notification before
registering with the condition variable, then sleep forever while shutdown joined.

`request_worker_stop` sets the atomic flag for in-flight jobs and acquires each
waiter's mutex before notifying. A waiter already between its predicate and wait
must register/release that mutex before shutdown can notify it. Both normal
destruction and partial-construction cleanup use this handshake. File stopping
still occurs after input joins so accepted ordered file jobs can drain. Async
Open completion similarly synchronizes its file-worker wake notification.

The native `worker-shutdown` fixture wraps the actual condition-variable wait,
notify and mutex-lock calls. All four document workers pause while holding their
predicate mutex immediately before platform wait registration. Old code reaches
three notifications first; fixed code reaches a predicate-mutex request first.
The fixture releases the workers only after that rendezvous. It uses a bounded
child process to report the old deadlock without leaving a worker alive. No
sleep attempts to win the worker race, and no XR timeout is relaxed.

The pre-fix coherent executable fails with a blocked worker; the fixed one exits
normally. File, recovery and the unchanged full bake regression also pass. The
whole-core sanitizer and integrated XR suites are separate required checks; see
VALIDATION.md for their final source and results.
