# Certified critical map witnesses after process retirement

The unchanged graphics monitor updates its critical `seen` set before certifying
mapped ELF fingerprints. Its broad sample handler swallows FileNotFoundError and
ProcessLookupError. V2 repairs only the escaped, exact-owned mountinfo EINVAL;
it cannot observe a swallowed ENOENT and therefore does not close this gap.
Both earlier sources and all their scoped controls remain unchanged.

The retained CPU counterfactual uses the actual unchanged monitor and two owned
`/usr/bin/sleep` processes. At the actual covering-mount read boundary, it reaps
the same child after map-inode observation but before the real mountinfo open.
The genuine syscall returns ENOENT. Both old and V2 calls return normally with
`consumer` in `seen` and no completed mapped fingerprint. This is a reproduced
harness defect, not a product or rendering failure. Both children are reaped and
all source/input hashes remain exact.

V3 gives the unchanged monitor a private trial set and records its observations
before the unchanged V2 classifier can roll back an interrupted sample. After a
normal or already-qualified retirement return, it promotes a newly observed
critical name only if its completed fingerprint matches the prelaunch sealed
expected path and SHA, map device/inode, stat device/inode and corresponding full
mount certificate. It checks every candidate before changing the caller's set.
No additional exception type or lifetime condition is excused. Completed
certificates can survive a later interrupted file in the same sample; mere map
observations cannot become witnesses.

Fifteen CPU controls pass. They cover complete live observations; genuine first
and partial ENOENT/EINVAL; EINVAL reaped before the existing classifier; retaining
prior successful witnesses; actual expected-path/hash refusals; and all six
existing live/path/errno/start/ownership error refusals. Partial retirement keeps
the already certified consumer and withholds the uncertified libc observation.
All owned children are reaped, with zero input drift.

The new `run-transaction-v3.py` integration takes critical expectations only from
already sealed prelaunch pins and rejects missing or conflicting providers.
It pins both adapters, records expected ELFs and retains the exact source
scenarios, assertions, timeouts, common lease, actual device/PCI/FD checks and
map identities. Earlier `run.py` and V2 remain immutable. No application, GL or
headset execution has used V3 yet. Fresh whole-build/dependency, runtime-provider,
native87, producer/font, software and actual Arc gates remain prerequisites.
This CPU-only milestone does not admit the candidate or complete the full goal.

The adjacent [receipt](validation/public-monitor-transaction-v3-20261115.json)
records actual hashes and source paths. The [controls](validation/public-monitor-transaction-v3-controls-20261115.json)
and [counterfactual](validation/public-monitor-transaction-v3-counterfactual-20261115.json)
retain original syscall stacks and process identities. The helper, causal
control sources, integration source and exact diff are preserved alongside them.
