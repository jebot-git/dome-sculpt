# Owned native emission EXR codec

The private `da6a8e1` repair replaces the shared native-emission high-level
OpenEXR C++ writer and reader with owned Core contexts and pipelines. Package1,
package2 and package3 all use this route. A per-call aligned allocation list owns
successful Core allocations through partial initialization, compression, decode,
cleanup, exceptions and cancellation. Callbacks return errors without unwinding
C code; context and pipeline RAII runs before the final owning arena cleanup.
The installed Core uses per-context libdeflate `_ex` allocation callbacks.

The original package3 sanitizer sweep completed its functional assertions but
leaked 5,736 bytes in 142 system Header/OutputFile allocations. That failed run
and its exact hashes remain preserved. No suppression, failure disabling or
budget increase is used. This child closes the reproduced failure with two
rebuilt codec units and matched immutable callers: every measured C++ denial
prefix (3,868) rolls back exactly in Release and sanitizer builds, with clean
leak detection. Separate deterministic null-return sweeps cover all 42 writer,
40 decoder and 32 header allocator requests in the measured multichunk fixture.
These are complementary scopes, not an exhaustive process-wide malloc claim.

Fresh wholly owned application/core/render/support builds each pass 227 steps;
all 12 selected runtime cases pass in both optimized Release and O1
ASan/UBSan/leak profiles. The actual source snapshot is unchanged. All 170 common
R/S GLBs match the functional parent; the 92 successful files match its captured
unfiltered Khronos cohort, including six historical package1/2 controls. No new
hardware, full-suite, Wayland or fresh desktop-GUI qualification is inferred.
The older package3 native/compact GUI positives keep their original scope.

See [the complete receipt](validation/NATIVE-PAINT32-EXR-OWNED.json). The
functional package3 receipt remains historical evidence with its original leak
blocker; this repair is the follow-on ownership proof.

For the normal34/native32 integration, preserve actual canonical attributes/UV,
baked-normal coverage/filter metadata, genuine-normal-frame export guards and
all Scene35/material17 capability/pool/budget checks. This patch adds no public
layout or Scene version. Private emission structs still require matching export
consumers. Package3 calls the actual `Scene::decode`, rather than admitting
formats by their numeric outer version. Keep the root's newer page registry.

Same-object physical emission with canonical normal/key/UV/deformation owners
remains a required adapter. The current bare-native guard must not silently
admit newly appended canonical owners. Root's new skin-only finite export proof
also does not establish that physical combination. Typed native MToon float and
general retained physical-domain workflows remain required after this milestone.
