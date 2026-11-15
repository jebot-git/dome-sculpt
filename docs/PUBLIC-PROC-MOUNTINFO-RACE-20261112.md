# Public monitor process-exit race, 2026-11-12

The first ad99 desktop pbr-quarter workflow stopped with coordinator status1 and
`OSError(22, Invalid argument)` after39 passing checks. Its ten real public Bake,
Apply, ordered Save, Undo, Redo and fresh-reopen checks passed; the full visible
source workflow was incomplete. This is a preserved failure, not a public pass.
All34 captured process identities were absent in the final cleanup snapshot.

A CPU-only probe invoked the exact unchanged mapping monitor against256 owned
`/usr/bin/sleep .008` processes. Six naturally occurring failures had the same
stack: `elf_fingerprint` called the typed mount-identity helper, which failed
opening the exiting process's `/proc/PID/mountinfo` with EINVAL. Every failing
PID retained its original start identity and was alreadyZ. All children were
reaped; source/helper/provider inputs remained exact. No app or GL ran.

A separate nine-control CPU proof forced retirement immediately between reading
the process namespace and opening mountinfo. The original monitor reproduced
the actual EINVAL syscall. A narrow adapter handled only that exact path/error
for the same owned start identity after it becameZ or was reaped. Six negative
controls retained the original error for live processes, another path, another
errno, mismatched original or later start identities, and unknown ownership.
No new ELF certificate was accepted on an unavailable retired-process read.
The first controlled fixture retired before the namespace read and encountered
the existing FileNotFound handling instead; that diagnostic setup failure is
preserved separately and does not qualify the repair.

The adapter and integration diff are test-harness evidence only. Critical live
ELF/inode/device checks, worker/result assertions, all deadlines, RSS bounds,
common graphics lease and final owned-process cleanup remain unchanged. There
has been no repaired app/GL replay. A separate confirmed typed-UV1 sampling
refusal defect holds public admission pending a new product source and coherent
owning qualification; the monitor repair does not remove that hold. No physical
HMD, performance or full-goal completion claim is made.
