# Retired-process critical witness rollback, v2

Root review found a concrete flaw in the first CPU-qualified adapter: the shared
monitor adds critical inode observations to `seen` before ELF fingerprint and
mount-identity certification. If a process exits during certification, merely
classifying its exact mountinfo EINVAL can leave a new uncertified witness in
that set. The prior label that witnesses were unchanged was not established.
The original v1 files and evidence remain preserved; v1 never ran a repaired
application or GL case.

The new adapter snapshots `seen` before calling the unchanged monitor. Only when
EINVAL names the exact owned `/proc/PID/mountinfo`, original and monitor start
identities match, and the process is absent or the same-startZ/X, it restores the
exact prior `seen`. It retains completed mapped ELF fingerprints: the monitor
stores each only after full fingerprint/mount certification returns. All other
errors and live, reused or unknown process identities still fail. Original
critical/device checks, scenario assertions, deadlines, RSS and cleanup remain.

Ten CPU controls pass with source/helper inputs exact and all children reaped.
They retain the original actual-syscall counterfactual, handledZ and reaped cases,
and six live/path/errno/start/ownership refusal controls. The added control uses
the real unchanged monitor: it completes a certified `/usr/bin/sleep` ELF
fingerprint, adds the consumer critical observation, then encounters exact
mountinfo EINVAL after the same process becomesZ. V2 retains that completed
fingerprint, restores only `prior-certified`, and removes the interrupted new
consumer witness. The adjacent JSON retains the exact stack and read ledger.

This is a CPU-only harness proof. No app or GL replay has run with v2, and the
separate confirmed typed missing-logicalUV1 product hold remains. A new qualified
frozen product source and coherent owning runtime admission are required before
public desktop or real-loader XR continuation. No physical HMD, performance or
full-goal completion claim is made.
