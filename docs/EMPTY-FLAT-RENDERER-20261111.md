# Empty packed renderer selection — scoped qualification

Private source `82257f3b` avoids unused flat VBO/EBO/VAO allocations when an
accepted snapshot contains only sculpt chunks or no geometry. Empty publication
remains transactional: a late resolve failure preserves the old packed frame,
resources, pixels and counters; packed/sculpt/empty retries remain exact.
The store ledger skips fence creation only when it has no active **or retired**
nodes. Active or retired zero-byte names still require their latest-use fence.

The first `25c9e5d6` change exposed a valid public-input regression: an empty mesh
with one retained `SurfaceRange(object=1, first=0, count=0)` attempted a core-profile
draw without a VAO and produced the actual unchanged GL error 1282. Final 822
submits no primitive for zero-count ranges, after all original material/source
and range validation. A zero-count range with invalid offset1 still refuses.

The same fresh public-header probe object and current-ABI core produce old e993
exit 0, intermediate 25c exit 1, and final 822 exit 0. Old/final logs are byte-identical.
The probe is a software Release control; it does not independently cover invalid
offsets or deferred transactions. Those controls belong to the final root
renderer-recovery fixture. The independent final source review found no remaining
blocker. No guard was removed to accept malformed ranges.

Each profile rebuilds all five actual Ninja-dependent whole TUs after the private
ledger header changes: render.cpp, current-preview GPU, overlay GPU, store-ledger
and renderer-recovery. One render member is replaced in its four-member archive;
three unchanged member payloads are verified. Eight matched consumers are freshly
linked with read-only current e993 core/support/SDK providers. Public headers,
layout, schema and defaults are unchanged. This is an affected-owner component
closure, not a fresh whole build or application qualification.

| Final software cases | Release | Strict sanitizer |
|---|---:|---:|
| Source paint overlay | 44 | 44 |
| Sparse GPU overlay | 42 | 42 |
| Buffer reuse | 24 | 24 |
| Original capacity / indexed picking | 64 | 64 |
| Separate 9.6M / >256 MiB store | 63 | 63 |
| Actual Document buffer reuse | 31 | 31 |
| Current-grid GPU/reference/cache/resources | 98,815 | 98,815 |
| Typed store ledger | 94 | 94 |
| Accepted/Pending recovery | 828 | 828 |

All nine software cases pass in each profile. Four additional cases per profile
pass on authenticated Arc A770/PCI03/i915, with actual EGL device, open DRM FD and
mapped ELF witnesses: ledger 96, recovery 831, Document buffer 31 and large-buffer 63.
Every mapped ELF is pinned before launch and checked afterward. Native/driver
assertions and original deadlines/RSS bounds remain unchanged: native 180 s,
capacity 240 s, Release 4 GiB/sanitizer 8 GiB. All source/provider/output checks are
exact; strict ASan leak detection 1 and UBSan report no diagnostics or suppression.

The first Arc ledger native test passed 96 checks but its measurement coordinator
exited 1 after missing the fast live FD while hashing DSOs. That raw observer
failure is preserved and supplies no hardware pass authority. V2 pre-pins the
same-profile authenticated runtime ELFs, then samples live FD/maps/RSS cheaply;
all mapped ELFs must match pre-pins and posthashes. No native or device predicate
was weakened. All eight final Arc cases pass with that measurement correction.

Old e993 complete-product counterfactuals exit 1 at both intended new assertions;
their coordinator 0 means expected negatives, not successful native runs. Initial
25c retains nine successful Release software cases and a successful sanitizer
build, with **no sanitizer runtime claim**. Its zero-range failure remains intact.
No earlier source, status, log or failure is replaced by final results.

This milestone does **not** claim a speedup, performance/90Hz acceptance,
physical headset, whole application, new HDR ABI integration, or a rerun of the
196 public workflows/full P1. Those public/P1 results belong to e993. The new HDR
combination and full owning application qualification remain required. The full
goal stays active and incomplete; no installation or session restart occurs.

The new integration branch merges exact main `6e431f9d` into frozen822. Its product,
header, test, asset, third-party and CMake/build-source bytes remain identical to
822; metadata preserves the distinction between the flat ledger's 1 GiB default,
the sculpt arena's 512 MiB bound and the capacity test's 512 MiB endpoint envelope.
No main merge is claimed here.

[The scoped receipt](validation/EMPTY-FLAT-RENDERER-20261111.json) records every
actual case, command, provider boundary, failure, independent probe and verified
lossless metadata/source/log archive. Original files and provider binaries remain
untouched. All qualification jobs and Arc leases are closed.
