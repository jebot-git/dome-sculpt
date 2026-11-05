# Physical package allocation-fault closure — 2026-11-05

The frozen private source `1f9fd48501aeba4db77eabb0e86679efad7923bf`, using matching
`5df649d` owning37/physical4 support, now completes all 128 disjoint partitions
in both Release and sanitizer profiles. Each profile covers **90,261 persistent
allocation-denial prefixes plus one successful endpoint**. Every shard measures
three stable uninjected baselines and verifies unchanged input, binary, package,
complete Scene rollback, successful recovery and actual mapped ELF identities.
The 28 runner controls cover gaps, overlap, missing/mixed/stray receipts and other
invalid aggregation. A second aggregate replay on each exact frozen source passes.

The native child limit remains 3,500 seconds and CTest limit 3,600 seconds.
Maximum partition durations are 13.394 seconds Release and 159.911 seconds SAN;
summed native partition durations are 783.483 and 10,348.154 seconds respectively.
Counts are calibrated at the actual equal-length case paths: three repetitions
of 90,261 each profile. The shorter original measurement path gave 90,257 and is
not substituted for these path-sensitive allocation counts. Source fixture SHA
is `c88483b9a417561a97be80f7cbe471fe4a1aea83bbcb7813713b1530d0d0095a`;
baseline package SHA is `bbda94a4db56ace79ee83622bc8af54fab39cb5cccf6c66078916197eafc4a1b`.

The original whole-test SAN invocation on `f36570c` remains an actual CTest
failure at 3,600.04 seconds. Its baseline 90,259 and progress through prefix 11,750
do not become exhaustive coverage. This new runner preserves that failure and
proves the full native failure/recovery behavior within separately bounded jobs.

The integrated `93c1fad` candidate independently passes its complete physical
contracts, three measurements and 40 explicit early/middle/final smoke points per
profile. Those 40 points are not a second exhaustive sweep of that candidate.
This closure does not admit owning37/physical4 to main, establish full regression
coverage, or complete the production goal. No headset launch, installation or
session restart was performed.

Exact profile totals, per-part receipt hashes, source/binary hashes, preserved
failure and aggregate replay results are in
[the validation receipt](validation/PHYSICAL-PACKAGE-FAULT-CLOSURE.json).
