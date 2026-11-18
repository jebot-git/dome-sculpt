# Canonical normal sources: fresh strict sanitizer component

Canonical product `85eafc42` with fixture-only snapshot correction `b11f9d42`
passes four strict native ASan/UBSan suites from a completely fresh core build.
All 193 core members and the selected dependencies/fixtures were rebuilt:
212 genuine compiler dependency owners carry AddressSanitizer and
UndefinedBehaviorSanitizer instrumentation. The actual CMake flags retain the
previously qualified O1/g1, frame-pointer and section settings. No Release or
previous sanitizer object/library payload is copied into this build.

The bounded regular-user build completes zero in 851.199 seconds. Its terminal
properties and journal were captured before stopping Invocation
`3470aba927444b6bb00cdd284a382716`. Raw compiler dependency files, actual commands,
Ninja records, source/tool pins and output hashes are retained.

Runtime passes independently in this order:

| Suite | Actual checks | Runtime seconds |
| --- | ---: | ---: |
| Advanced normal sources, 17 cases | 263495 | 13.454 |
| Prepared sources | 91 | 2.925 |
| Artist sources | 139 | 1.410 |
| Canonical sources, 32 cases | 525092 | 301.920 |

All four ordinary loader checks pass. Every actual test process has live ASan
and UBSan mapping witnesses. Leak detection, halt/abort on sanitizer errors and
UBSan stack traces remain enabled; there is no virtual-address-space cap,
preload, suppression or skipped suite. All 17 advanced and 32 canonical
immediately ordered Save files match their independently saved native endpoints
byte-for-byte. The canonical corpus includes both stored normal roles,
field replacement/Clear, UV/key/nonuniform-skin changes, physical emission,
source preservation, history and cancellation/quota/foreign-owner refusals.

Actual runtime status SHA256 is
`060b39336aa6ff21e2107d1e73e98ac87bf5f1a416b6df9366e7368cbaaaba1e`.
Invocation `96b5dcc1fc2d496fa9d55da1635b7918` completes zero; terminal properties
and journal were captured before stopping it. Peak accounted runtime-unit
memory is 2,931,961,856 bytes. Final input drift and surviving owned-process
lists are empty. The device lease is released and independently observed free.

The [machine receipt](validation/CANONICAL-NORMAL-STRICT-SANITIZE-20261118.json)
and compact archive preserve all actual dependency files, logs, mappings,
plans, source/tool/output hashes, unit captures and the 49 native/ordered file
pairs. They contain no compiled executable/object/static-library payloads.

This qualifies the selected native component corpus. Continuous filtered-detail
quality remains separate: the subsequently discovered low-resolution transfer
defect needs its new versioned fidelity policy and resolution workflow. New
writable/joint source adapters, complete all-target/application sanitizer
qualification, physical devices and the remaining full production requirements
are not admitted by this result.
