# Prospective decoded EXR package budget

This narrow private follow-up to41b5864 enforces the existing producer's128MiB
aggregate decoded ten-float field budget in both the shared private writer and
untrusted package reader. No public layout, numerical cap or serialized format
changes. Valid old output bytes remain unchanged.

The actual old-reader negative is retained. Four genuine constant physical
background owners, with correctly rebound package/native/core digests, supplied
309,628 encoded EXR bytes. The reader performed four40MiB pixel allocations
(160MiB decoded work) and returned the exact native Scene. This demonstrates a
missing aggregate-work forecast, not unbounded peak memory: old decoding was
sequential and already had per-image/count/encoded bounds.

The reader now uses the existing low-level bounded Core header preflight to
measure every field before decoding any pixels. Header storage remains bounded
at1MiB; individual dimensions/channels/compression/working limits remain intact.
It rejects the same over-budget input before any40MiB pixel allocation. A mixed
resolution below-limit case still restores exactly with its one expected large
allocation. The private writer mirrors the existing public preparation forecast
before native/EXR serialization, preserving the original owner on refusal.

Release and whole-owned sanitizer matched-component runs each pass3/3 suites:
4 resource checks,14 independent codec checks and32 package checks. Exactly two
complete runtime TUs replace their original members in eight byte-verified
immutable41b support archives; three complete consumer TUs are linked with the
same profile flags. Link maps and before/after1534-input ledgers are retained.
External OpenEXR/zlib are system libraries, not instrumented. This is not a fresh
whole-app/core build, full regression, GPU or GUI qualification.

The existing three package GLBs remain byte-identical to41b; new resource fixture
files are also byte-identical between Release and sanitizer profiles. The old
expected exit1 and exact original consumer source are preserved separately.
Read validation/HDR-EXR-AGGREGATE-BUDGET.json for hashes and exact commands/scopes.
The generic native-authoring container remains the next separate implementation;
this patch does not admit arbitrary changed surfaces or Source4 portable output.
