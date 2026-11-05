# Native frame factory and codec allocation failures

The actual native36 Create, Replace, Scene encoder and Scene decoder retain
their complete caller/source state under persistent allocation denial on a real
small painted, skinned and keyed input. This milestone adds one native C++ test
and four portable CTest registrations. Product code, public/private layouts,
serialization and production resource limits are unchanged.

The original optimized sweep completes every measured prefix of all four real
operations. Wholly owned O1 ASan/UBSan coverage is composed from the original
completed Create/Replace phases and the same frozen selector binary's disjoint
encoder/decoder ranges:

| Operation | Release C++ | Release malloc | Sanitized C++ | Sanitized malloc |
| --- | ---: | ---: | ---: | ---: |
| Create | 3458 | 437 | 3458 | 460 |
| Replace | 2687 | 608 | 2687 | 640 |
| Encode | 7544 | 608 | 7544 | 640 |
| Decode | 4555 | 646 | 4555 | 680 |
| Total | 18244 | 2299 | 18244 | 2420 |

Every reached prefix refuses, preserves the entire caller Scene, immutable
seed and wire, and then permits an exact real retry. Thirteen copies of each
saved Create/Replace endpoint across the original and range cohorts are
byte-identical. All 52 compiled parent support/config inputs remain exact;
no ASan, UBSan or leak diagnostic occurs. The new test TUs use their own exact
headers and completed native36 owning core/support, without old-layout
substitution or a fresh whole-project build claim.

The original aggregate sanitizer run times out at900s after completing Create
and Replace. Both later whole-mode encoder and decoder attempts also exhaust
their900s limits; no unprinted partial prefix coverage is claimed. Those three
failures remain in the receipt. Four encoder C++ ranges and three decoder C++
ranges, each at most2048 prefixes, plus separate malloc modes complete with the
same binary and unchanged inner assertions. The longest completed range takes
297.63s. This is composed coverage, not a green original sanitizer invocation.

CTest now registers each natural operation separately. The optimized wall
budget remains900s; address/undefined sanitizer configurations use a finite
2400s whole-test budget. This budget-only change follows the completed proof
boundary. The measurement bound remains32768 allocation calls. No longer
whole-mode rerun is claimed just to replace the recorded timeouts.

Native static `malloc` wrapping is disjoint from C++ `operator new` denial;
the latter uses `__real_malloc`. This does not cover every C allocator family,
dynamic-library allocation, arbitrary input/cache state, Document admission or
worker publication. Preparation and caller Scene copying precede the tracked
operation. Existing worker/GUI/controller evidence remains separate.

Reproduce from a matching canonical UI/XR build:

```
cmake --build build --target dome-sculpt-native-frame-faults-test
ctest --test-dir build -R '^native-frame-(create|replace|encode|decode)-faults$' --output-on-failure
```

The test also accepts `OUTPUT MODE cpp|c FIRST LAST`, with modes0..3 corresponding
to the table, to retain exact finite range ledgers. Omitting the range measures
and exercises the entire selected allocator. Omitting all selectors runs all
four operations.

The canonical UI/XR registration configures correctly in both profiles. The
inherited frontend-off VRM palette `PkgConfig::RENDERFONTS` CMake failure is
preserved separately; this milestone does not claim to repair it. Read
[the receipt](validation/NATIVE-FRAME-FAULTS.json) for complete hashes, commands,
range/time ledgers, saved endpoints and exact remaining scope. Canonical key/UV
retained-grid composition, broader authoring domains and main/full-goal admission
remain separate required work.
