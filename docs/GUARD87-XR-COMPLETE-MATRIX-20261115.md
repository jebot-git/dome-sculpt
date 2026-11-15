# Complete coherent XR validation matrix

All four final-config real-loader synthetic XR workflows pass: **776 checks**.
Every case retains the original 240-second, 6000-frame and RGB/geometry/normal
bounds of 3/12/7. Each starts with its own empty Mesa shader code cache and runs
19 fresh application processes. Scene, configuration and native state remain
fresh for each application. No case shares another case's code cache.

| Case | Checks | Seconds | First-app hits/misses | Later disk hits |
| --- | ---: | ---: | ---: | ---: |
| mtoon-quarter | 194 | 175.953 | 0/27 | 443 |
| mtoon-tiny | 194 | 178.003 | 0/27 | 443 |
| pbr-quarter | 194 | 169.505 | 0/27 | 427 |
| pbr-tiny | 194 | 157.358 | 0/27 | 427 |

All four phase exits and actual coordinator process exits are zero. Complete
reports contain no failed check or error. All inputs remain pinned, all owned
processes have exited and each lease was released. The final read-only audit
rechecks 2616 deduplicated current pins with no conflict or drift. All 76 actual
application PIDs are absent after completion. The sanitizer agent received an
explicit handoff after the final case; no overlapping graphics owner was used.

The source scope is the immutable application/core/producers at `3d164382`,
test-only JSON reporter module at `c54f66f8`, and operational Python coordinator
at `02d17f29`. The reporter overlay has its separate actual dependency, export,
SONAME and current-loader proof; this does not claim a whole application build
from either child. Native 87, producers/font, software and Arc manual controls,
and 208 public desktop GUI checks are separate prerequisites already recorded.
The source assembly draft remains unbuilt and unadmitted.

The original null-reporter failure, cache-disabled MToon timeout after 174
passing checks and outer owning-build tool exit 143 remain preserved failures
or unresolved outer observations. They are not converted into passes. The two
older cache-disabled PBR passes are separate history and are not counted in this
final matrix. The PBR-quarter final outer poll was unavailable at compaction;
its sealed actual coordinator exit, complete report and zero-survivor witness
are retained without inventing an outer-tool exit.

The [receipt](validation/guard87-XR-final-fourcase-20261115.json) pins the four
complete raw evidence capsules: runtime reports, both-eye pixel captures and
probe/sensitivity reports, actual mapped ELF/device certificates, process
observations, cache file hashes and timings. Each archive has an exact member
hash manifest. The current-pin audit and read-only sealing script are included.
This is a scoped synthetic XR validation milestone. Physical-headset testing,
installation, combined-draft admission and the full production goal remain
outside this result.
