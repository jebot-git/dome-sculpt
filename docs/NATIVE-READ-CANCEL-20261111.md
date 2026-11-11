# Owning native reader cancellation — 2026-11-11

The focused Release and ASan/UBSan cohorts pass **92 cancellation controls and
17 native geometry-scope controls per profile**. Leak detection is enabled,
with no suppression. This validates the four changed reader translation units
of `9c44723d`; it does not qualify a fresh complete application or GPU workflow.
The full original production objective remains incomplete.

[The receipt](validation/NATIVE-READ-CANCEL-20261111.json) records commands,
actual source/ELF hashes, scopes, prior failures and persistent evidence paths.
Tests are sealed at `f4f5a63e`; this lane changes only tests/CMake and these docs.
Public headers and production source remain exactly `9c44723d`.

## What the tests exercise

The actual fixture contains two native Scene objects with 1,024 sculpt samples
each. Volume/Scene/file/payload cancellation must throw, publish no partial
result, preserve the complete input, and permit an exact encoded retry. File
bytes also remain exact. Payload controls cover genuine native CBOR and an
independently zlib-compressed copy through the production restore path.

Actual complete callback counts are Volume 25, Scene 67, file 72, uncompressed
payload 74 and compressed payload 76. Selected first/middle/final boundaries
retain strict known cancellation messages. A separate payload stop at
`complete_count - 1` requires **exactly `Project loading cancelled`**, with no
outer ambient scope. It therefore proves the new decoded-Scene callback path,
before the preexisting final HDR check. This is not an exhaustive prefix sweep.

Other controls cover owned temporary callbacks, callable-copy `bad_alloc`
before TLS publication, parent authority through false/empty inner readers,
callback exception/retry, a real nested decode without callback self-recursion,
and simultaneous readers with independent cancellation. The copy exception is
an intentionally throwing callable, not an exhaustive global allocator test.

Isolated actual COW maps exercise cancellation during pool hashing and full
equality. The caller's owner remains unchanged at every checkpoint; only a
complete retry may publish the equal retained storage. The original 17 tests
also preserve nested/read-exception/thread pool restoration and independence.

## Composition and provenance

The immutable 9c core supplies 192 whole members. Four complete product TUs were
compiled against this child: `scene.cpp`, `volume.cpp`, `scene-file.cpp` and
`emission-package.cpp`. Both final test TUs are fresh. Thin-core membership and
link maps exclude all four corresponding original members. The remaining
product/header/asset 717-file closure is byte-exact 9c. Both definitions using
the changed private pool layout are rebuilt; no old class tokens are borrowed.

Release reuses its four exact closed first-attempt product objects. Sanitizer
uses four freshly compiled closed-v2 objects. Final v3 recompiles both tests;
that provenance is explicit in each status. There are no archive copies or
shared build mutations. Fresh headless CPU configuration succeeds and lists
both registered native tests; that is configuration evidence, not a fresh
headless build. The new target is `dome-sculpt-native-read-cancel-test`, with
CTest name `native-read-cancel` and a 180-second native deadline.

The final same-ELF runtime audit repeats both tests/profile, observes 25 actual
mapped ELF files per Release consumer and 27 per sanitizer consumer, and
verifies their bytes before/after. These are native CPU tests, with no graphics
context, Arc lease, GUI, headset or installation claim.

## Preserved failures and controls

The first Release attempt at `f3d4eb3e` exits 1 because its test expected only the
Project message at early HDR preflight. The production reader correctly used
its existing `Native HDR export/extraction cancelled` message. The test fix
recognizes those two exact stage-specific messages for payload controls only;
it does not accept arbitrary exceptions. Source, object, ELF and log evidence
from that failure remains intact.

Intermediate `d7670f7d` R/S 86+17 passes remain separate from final 92+17 results.
A real counterfactual build substitutes the entire original 9c emission-package
TU, with the same final test and other three current TUs. It exits 1 at missing
callback propagation into native decoding; the negative-control coordinator
exits 0. Its actual private include dependencies match current headers.

Independent review found no product defect in callback ownership, TLS parent
checks, reentry reset or compare-before-alias behavior. Existing limits and
validation remain unchanged. CBOR parsing still has before/after checks; this
receipt does not claim SAX cancellation, every codec's prompt interruption,
full worker history/Save qualification, or production-wide admission.
