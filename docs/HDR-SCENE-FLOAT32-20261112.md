# Full FLOAT scene target qualification — 2026-11-12

Scoped component checkpoint; full application integration, physical headset,
sustained performance and the full production objective remain separate.

## Problem and change

The frozen 291 renderer passes the software HDR281 fixture, but actual Intel Arc
A770 clips a finite 100000 red contribution to HALF range before source-alpha
blending in its private RGBA16F scene target. Actual 128/255 alpha therefore
produces 32864 rather than 50196.1. The original brightness oracle is retained.
The authenticated native 1, device identity, live DRM FDs and mapped providers
remain in `artifacts/hdr-preview-blend-qualification-20261112/arc-v1/`.

Product source `2bf632673ef9ce6fa74cbaa2117e5aa19bacb4eb` uses an actual RGBA32F
scene color texture through blending and resolve. It verifies the allocated
internal format and fails explicitly if full FLOAT allocation is unavailable;
it does not fall back to HALF. Native coefficients, source data, interchange
validation, MToon blend rules and the original HDR281 test are unchanged.

The private requested-store accounting is 20 bytes per pixel with a conservative
four-byte depth slot and 24 bytes with FLOAT depth/stencil. The target pool has a
512 MiB byte bound, retaining the former HALF/depth pool's worst envelope, plus
the existing 16 MiPixel per-view, 32 MiPixel pool and two-target limits. Admission
includes the candidate and accepted target; older cached views may be evicted.
A large simultaneous resize/depth-format transition refuses before allocation
when the accepted front plus candidate cannot fit. A maximum-size individual
view requests 320/384 MiB, and two such simultaneous views cannot fit this bound.
This is an explicit resource limit, not a performance improvement.

Failed replacement allocations preserve the accepted target and restore the
caller's framebuffer. Cleanup consumes a bounded number of GL errors, stopping
on context loss, and rethrows the original failure. The actual failure controls
inject GL_INVALID_VALUE through genuine allocation calls; they do not establish
recovery from a real driver out-of-memory condition. Material statistics retain
their material-only meaning; private target accounting remains separate from
the context material/geometry ledgers. Requested bytes are not a measurement of
physical driver residency or pending driver allocations after deletion.

## Validation

Final Release, strict sanitizer and Arc receipts are recorded in the adjacent
validation manifest. Release uses optimized O3/NDEBUG compilation. Sanitizers
retain leak detection 1, halt/abort on ASan error, halt on UBSan error and no
suppressions. Each profile rebuilds the whole affected render.cpp TU and all
nine consumer TUs, then links them to the exact current 193-member core and
three byte-identical other renderer members. This is an audited incremental
component closure, not a fresh whole application build.

Both Release and strict sanitizer software runs pass all nine suites:
target 56, HDR281, MToon 81, source overlay 44, recovery 828, document buffers 31,
capacity 64, normal-frame 47 and program transitions 17. The paired actual Arc runs
pass all 18 native cases, with target 57, HDR282 and recovery 831 in each profile
because those consumers add hardware identity controls. Every run authenticates
its actual device and 71/73 mapped ELF providers. No source/provider drift or
functional guard failure occurs. Sampled peak RSS is 1,018,060,800 bytes in Release
and 1,828,761,600 bytes with sanitizers, within the original 4/8 GiB envelopes.

The new actual GL target fixture verifies the production deferred target format,
all four 32-bit components, 20/24-byte accounting, low-limit admission/refusal,
exact old pixels and names after injected color/depth errors, LRU reuse, explicit
clear and depth-format transitions. The original HDR281 fixture and seven legacy
functional suites retain their native inputs and image/history oracles.

The newly compiled Release HDR281 object is byte-identical to the entire old 291
Arc-failing object: SHA256
`950e79dd6162e45136e20075e2131b7358b5c51eeb0600ce2e59bb0b2e75ef37`.
The exact new whole target-fixture object linked to the unmodified 291 renderer
and same current core fails native 1 at its actual production RGBA32F assertion.
Its renamed private class cannot replace the production target through weak
symbols. Counterfactual failures remain failures; their receipt success only
authenticates the intended reproduction.

Arc execution uses the common `/tmp/dome-sculpt-arc-device.lock` serially with
public Bake validation. The observational f13 preload forwards real GL queries
and authenticates actual EGLDevice, PCI 0000:03:00.0, vendor/device 8086:56a0,
renderD129, live process FDs, actual saved context-map devices/inodes and
pre/post ELF payload hashes.
Functional 180-second limits (capacity 240 seconds) and Release 4 GiB/SAN 8 GiB RSS
envelopes remain in force. These runs do not establish frame rate or latency.

## Preserved attempts and limits

Frozen 291, cb934 and 354 sources and every existing provider remain intact.
The cb934 preliminary compile was cancelled after review requested bounded GL
error cleanup; its raw compile exit -15 is not a product failure. The 354 native
fixture failed because it assumed retired GL numeric names cannot immediately
be reused. The test-only 2bf child verifies the actual replacement dimensions
while permitting observed genuine name reuse. Its trace demonstrates that the
retired and new texture names are both 3 with the new actual width 40.

The initial 2bf Release receipt failed its final dependency-path assertion because
MMD recorded `tests/../src/render-target.hpp`. A separate read-only completion
receipt resolves those filesystem paths and authenticates all 30 successful
steps, the two affected object owners and zero drift; the raw status 1 is retained.
No compiler flags, literal -D values, source bytes or native checks changed.

The first corrected Arc target run passed native 57 but its observer receipt
failed because synchronous hashing missed the short process's external FD/RSS
sample. The separate v2 runner adds an independent 1 ms live sampler while keeping
the original provider collector, f13 observer, binaries, arguments, oracles,
environment and bounds. It introduces no process pause or GL-query/result change;
all 18 paired v2 receipts pass. The original native 0/observer 1 stays preserved.

First-live full-file hashes were optional observations in the original wrapper:
it rejected mismatches when present and did not require a nonnull hash for every
ELF. Four short sanitizer cases each have 25 missing observations (100 total).
Their code identity is authenticated by strict actual EGL/PCI context and live
FD evidence, exact saved context-map devices/inodes, and matching pre/post payload
hashes. The additional audit uses the established narrow Btrfs covering-mount,
physical-source UUID and separately exact subvolume stat/inode route. It retains
the initial incorrect generic stat/map-device audit failure. It uses current
covering mounts and the direct subprocess's inherited-namespace inference;
it does not claim a historical child mountinfo snapshot or missing live hashes.
No native rerun or original identity-gate weakening was needed.

The final read-only seal authenticates 22,856 input pins, including all 6,872
original historical sanitizer inputs, with zero drift. See
[the validation manifest](validation/HDR-SCENE-FLOAT32-20261112.json).

No installation, default toolkit change, desktop/display-manager restart or
physical-headset session change is part of this checkpoint. Broader application
integration and actual performance remain unqualified by this component proof.
