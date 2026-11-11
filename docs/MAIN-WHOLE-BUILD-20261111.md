# Integrated main: complete owning build closures

Source `820743158d81f3cc5cd47d12fd7592ff5c23f78d` passes fresh optimized Release
and ASan/UBSan builds of the selected 32-target application/core closure.
Each profile compiles 269 complete translation units and produces 312 owned
artifacts: 269 objects, 11 archives, 31 executables and one runtime module.
Both original complete build logs retain all 314 Ninja steps. No project objects
are borrowed from an earlier source tree.

Each profile passes the application version check, all 22 selected native suites,
27 local chunk workspace controls and 373 placed-owner workspace controls.
The sanitizer run enables leak detection and halts on ASan/UBSan errors, without
suppressions or LSAN_OPTIONS. Every owned sanitizer translation unit has the
ASan/UBSan compiler flags and no disabling override. These executions exit zero
without sanitizer reports. Every source and all 624 recorded output hashes are
rechecked after both jobs finish; there is no drift.

Exact commands, limits, source/SDK/log/binary hashes and retained originals are in
[validation/MAIN-WHOLE-BUILD-20261111.json](validation/MAIN-WHOLE-BUILD-20261111.json).
The 23-member local evidence archive includes both complete build logs, original
status records, compiler databases, configurations, source inventory and the
coordinator/sealer. It is retained at
`artifacts/main-whole-build-evidence-20261111.tar.gz` with SHA256
`f4d0df88278e08a87ce4c4ca18616958bce64b4d24e380077c1a5d7c39294c3a`.

Both configurations explicitly select CPU geometry computation with FLTK UI,
OpenXR and testing enabled. Release uses O3; sanitizer compilation uses O1/g1,
frame pointers and strict address/undefined checks. This does not establish
OneAPI/CUDA/ROCm qualification. The current canonical DomeD SDKs are unchanged:
a fresh fetch confirms local and remote main at `acc2b70a`; 418 pinned toolkit,
header, source and bitmap-library checks match. External bitmap libraries retain
their previously recorded platform instrumentation policy; this is not a claim
that every system library was rebuilt with sanitizers.

A subsequent metadata correction aligns the generic `source_sha256` entry for
`third_party/desktop/bdf_font.hpp` with the unchanged canonical helper bytes,
already correctly recorded by `desktop.bitmap_helper`. All 33 generic source
entries now match their actual files. The build evidence above remains tied to
`82074315`; this metadata edit does not claim another build or change any compiled
helper, runtime or header.

## Scope and remaining integration

This closes the fresh owning-build gate for main820. It does not rerun or broaden
the separately recorded graphics, GUI, actual-loader XR, interoperability or
performance evidence. The full P1 baseline preserves correctness but still has
unacceptable observed XR frame costs. Its optimized successor, broader production
and accelerator work, and physical acceptance remain required. Earlier failures
and driver teardown issues remain documented rather than relabeled as passes.

The genuine GPU painting implementation is composed separately with main in
candidate `775f92b8`, including its shared EGL lifetime prerequisite and the
Filters checkbox correction. Its complete Release 39-target closure, 29 selected
suites and 27/373 workspaces also pass, but its sanitizer build and new public
desktop/OpenXR workflows remain in progress. This main820 proof does not qualify
that candidate. Ordered GPU arena batching also remains private pending its
resource-lifetime and actual-device gates.

The full original authoring objective remains active and incomplete. Controller
trays and spatial node editing/history remain required later phases after the
basic targets. No installation, running desktop restart or physical headset
launch was performed for this build milestone.
