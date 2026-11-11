# GPU layer painting combined with current main

The private candidate `82f527570e9727df12632b6339f7c2cb59afb4c4` combines
main `820743158d81f3cc5cd47d12fd7592ff5c23f78d` with the genuine native GPU
active-color-layer operation from `507d6edaae1abd0e9172b28fe8ace7d600c46131`.
The earlier isolated experiment and its qualification receipt were already in
main; their empty cherry-picks were skipped while preserving main's additional
authored-alpha and qualification notes. The product commit applies cleanly.
This branch is a candidate; it has not been admitted to main or installed.

The candidate retains main's current canonical bitmap helper and per-object
sculpt shader/default-attribute repairs. It adds the typed sparse-layer filter,
document worker/lease acceptance, desktop universal Paint command, XR Filters
page and shared EGL display ownership described in
[GPU-COLOR-LAYER-BLUR.md](GPU-COLOR-LAYER-BLUR.md). The original full layered
painting/material/baking objective and every other unfinished goal remain
required. Controller trays and spatial node editing/history remain later phases.

## Fresh combined-source checks

Both Release and ASan/UBSan configurations succeed with UI, OpenXR and testing
enabled, the current canonical DomeD SDKs and explicit CPU geometry backend.
Fresh small-target builds compile the complete GPU kernel and EGL helper
translation units plus both consumers, with nine Ninja steps per profile.
They do not compile the combined core, renderer, desktop application or XR app.

Each profile then passes 61,442 independent kernel controls and 12 real
shared/borrowed EGL context ownership controls. The actual renderer is system
llvmpipe, LLVM 22.1.8; mapped Mesa/LLVM and sanitizer libraries are hashed.
The largest printed kernel error is `2.49617e-7`, within the unchanged oracle
bound. Release/SAN logs match byte for byte. Sanitizers enable leak detection
and halt on ASan/UBSan errors, with no suppressions or LSAN_OPTIONS; all four
processes exit zero under the original 180-second limit. No tracked input
changed during configuration, compilation or execution.

Exact source, SDK, commands, compilation flags, generated weights, logs,
binaries and mapped-library hashes are recorded in
[validation/PAINT-MAIN-INTEGRATION-20261111.json](validation/PAINT-MAIN-INTEGRATION-20261111.json).
Local original artifacts are retained under
`artifacts/paint-main-build-20261111`. These results establish the small helper
composition only. Software execution is explicitly requested detached validation;
it does not establish hardware execution or a successful native Document command.

The accompanying metadata repair updates the stale generic `source_sha256`
entry for `third_party/desktop/bdf_font.hpp` to its actual canonical bytes,
already correctly identified by `desktop.bitmap_helper`. Every generic source
hash now matches its file. The helper itself is unchanged.

## Remaining integration gates

The separate whole-main Release/SAN build remains on frozen main820; it cannot
qualify this new painting candidate. The new document command still needs its
own complete coherent app/core builds and real desktop/OpenXR-loader workflows,
including ordered Save and exact Undo/Redo. Existing synthetic SpatialFrame
controls are useful native evidence, not actual-loader or headset acceptance.
Dedicated public-menu/palette scenarios are being implemented separately; the
original 81-check GUI and 117-check loader scenarios remain intact.

The painting lane's original ordinary 180-second legacy sanitizer deadline
failures must remain recorded separately from its explicit extended
instrumentation profile. Broader typed source domains/channels, production
throughput, hardware/compiler backends, physical controllers and the remaining
full authoring requirements are not completed by these helper checks.

## Subsequent XR palette correction

Review of the actual shared palette found that Controls sector10 still displayed
the Erase check state after that sector became Filters. Remove that stale
checkbox; the direct Painting Erase checkbox remains unchanged. Give page6 the
explicit title `Brush controls` instead of its generic fallback title.

The complete changed palette translation unit compiles successfully in both
existing configured Release/SAN profiles. This is compile evidence only, not
an application link or rendered/interactive acceptance. Exact commands, source,
object and log hashes are in
[validation/PAINT-FILTER-PALETTE-20261111.json](validation/PAINT-FILTER-PALETTE-20261111.json).
The separately prepared public desktop/loader scenarios still need to run on a
coherent complete candidate. The earlier helper tests remain tied to source82;
this later source change does not relabel them as full-candidate qualification.
