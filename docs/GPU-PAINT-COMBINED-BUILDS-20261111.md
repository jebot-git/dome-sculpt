# Combined GPU painting candidate: owning builds

Private candidate `775f92b837da757aa5bfb8583e2f426685eade50` combines the current
main authoring/rendering work with the actual GPU active-layer color operation,
shared EGL display lifetime fix and corrected XR Filters presentation. This is
one native operation in the existing FLTK/OpenXR application. It does not complete
the full painting/material/baking objective or replace the document's native
paint, source, history and ordered-save authority.

Both Release and ASan/UBSan profiles pass the complete selected 39-target owning
application/core closure, all 29 selected native/software/legacy suites, and the
27 local plus 373 placed-owner workspace controls. Each profile owns 279 complete
translation-unit objects, 13 archives, 38 executables and one runtime module:
331 artifacts. Five matching translation units were compiled earlier in these
same candidate/profile directories; their object hashes remain unchanged. The
remaining complete build logs retain all 324 steps per profile. No project
providers are borrowed from a different source tree.

All source inputs and all 662 output hashes are independently checked after both
coordinators finish. There is no drift. Both source inventories are identical;
418 canonical toolkit/source/bitmap library checks also match the pinned SDK.
Sanitizer compilation covers every owned translation unit with address/undefined
checks and no disabling compiler override. Leak detection and strict halt options
are enabled, with no suppressions or LSAN_OPTIONS. Registered test properties are
audited too: none override those sanitizer settings.

The three legacy document/controller/painting tests run with their ordinary
argument lists. The explicit `--instrumented-assets` diagnostic profile is not
used. Current sanitizer controller, texture-painting and shared-document cases
pass in 24.55, 24.84 and 47.87 seconds respectively. Earlier ordinary 180-second
failures in the separate painting lane remain preserved in
[the painting lane's qualification receipt](https://github.com/jebot-git/dome-sculpt/blob/f99da4f73ebb8247d05b3ac1e4b2d088f323f10f/docs/validation/GPU-COLOR-LAYER-BLUR-20261111.json)
on its private branch; these new passes do not establish the cause of those
earlier failures or rewrite their outcomes.

Exact inputs, commands, compiler/target hashes, test registrations, original
statuses, logs and SDK identity are recorded in
[validation/GPU-PAINT-COMBINED-BUILDS-20261111.json](validation/GPU-PAINT-COMBINED-BUILDS-20261111.json).
The local 35-member evidence archive is
`artifacts/gpu-paint-combined-build-evidence-20261111.tar.gz`, SHA256
`2e488026337fe04d3c96ebaf878998a0ff07e9fce966c1b853dc4a46d9ca7e8e`.

## What remains open

These profiles explicitly use CPU geometry computation. The software GPU-filter
tests are detached validation and include actual Document refusal of an
unsupported software device. Successful application GPU commands still require
hardware. This receipt does not establish new Arc, desktop menu, actual-loader
XR, compositor, other compute-backend, throughput or physical headset acceptance.
Those current-candidate device and public-interface scenarios are being qualified
separately; the older painting lane's evidence is not relabeled as this build.

The candidate remains private until those integration gates close. Broader typed
source domains, painting tools, material evaluation, baking, production workloads
and all other unfinished original authoring requirements remain required. The
new plain GPU preview/cache and sculpt GPU arena retirement fixes are separate
unqualified work. Controller trays and spatial node editing/history remain
later required phases after basic authoring targets. No system installation or
desktop/headset session restart was performed for these checks.
