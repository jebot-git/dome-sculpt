# Source color preparation integration

This private integration combines published `8ad6d30` with the source-color
checkpoint `8d8f8d3`. It retains physical object holding, edit-authority leases,
deferred weight-stroke release, the shared 24-page registry, import options,
flat-normal source adoption and both projection-policy flags. It extends the
existing native FLTK/OpenXR application. It has not been installed or tested in
a physical headset.

Read [COLOR-PREPARATION.md](COLOR-PREPARATION.md) for the data model and remaining
requirements. The older `COLOR-PREPARATION-CHECKPOINT.json` describes only the
frozen standalone `8d8f8d3` tree and its older font runtime. It is historical
evidence, not a qualification of this union.

## Native dialog correction

The actual 640×360, double-size-font workflow exposed a native scrolling bug:
an off-screen custom slider could consume a wheel event intended for its scroll
group and change the color tolerance. Bake sliders now consume wheel input only
inside both their own bounds and their enclosing scroll viewport. Fractional
values use the configured step; one wheel step over the visible tolerance changes
it by 0.001. Integer and logarithmic controls retain their existing semantics.

The native fixture waits for changed, settled content pixels before checking the
scrolled result. It actually prepares and bakes the target, reaches the complete
quality report, confirms pinned actions remain visible and checks that scrolling
never changes a hidden tolerance. The normal-size fixture separately verifies
the visible tolerance's wheel step. Both workflows verify exact source retention
and global Undo; the normal-size workflow also verifies Redo.

## Evidence boundaries

The coherent union at `59c412f` passed all 166 CPU suites and all 167 OneAPI
suites, including actual loader/controller/stereo workflows. Thirteen direct
actual A770 graphical fixtures passed: flat-normal import, actual normal/color
PBR and dual MToon/PBR VRM exports, fractional-layer preparation, held-object
rendering, frame/UV/GLB/VRM consumers, MToon, buffer reuse, ordinary large-scene
capacity and native PCF text. These runs preceded the wheel correction and the
small bitmap-alias refresh.

The final product source `8031163` differs only in the bake slider/dialog fixture,
the canonical alias helper and its test, and documentation/provenance. Core,
geometry, worker, Scene and material-shader inputs are unchanged. Both final
all-target Release builds completed. Its focused 16-suite rerun passed, including
the actual color-policy OpenXR workflow. Final native dialog checks pass 9 normal
and 9 compact assertions; the existing projection dialog passes 7. The private
current DomeD Wayland universal-menu workflow passes 8 assertions.

Current native PCF/alias checks pass with software GL and the actual A770. The
canonical helper is byte-identical to DomeD `3024fb6`; the 23-patch FLTK and native
BDF/PCF runtime inputs are byte-unchanged from `eee23b7`. Actual client/compositor
library maps record the private FreeType/Cairo/FLTK and client HarfBuzz paths.
Use the **lib** prefixes in [TOOLKIT.md](TOOLKIT.md), rather than the historical
22-patch lib64 artifacts. No Pango application frontend is introduced.

The final 27 exported GLB/VRM fixtures again pass the pinned Khronos Validator
with zero errors, and 9 VRMs pass the 15 pinned official VRM0 schema blobs. The
dependency audit finds 382 tracked local compiler dependencies and no untracked
source/header dependency; generated build/protocol files are counted separately.

The final combined sanitizer run passes all 34 selected suites in 408.50 seconds
with AddressSanitizer, UndefinedBehaviorSanitizer and leak detection, without
suppressions. The core, renderer and bundled xatlas/MikkTSpace/ufbx sources are
instrumented; this CPU/software-GL build has UI disabled and XR enabled. The
initial all-target sanitizer build was deliberately stopped after the core was
complete, then only the affected and regression fixture targets were built.
That interruption was not a product or test failure. Native wheel behavior is
qualified by the separate actual FLTK workflows, not by claiming an instrumented
desktop frontend.

The exact source/binary/log receipt is
[COLOR-INTEGRATION-CHECKPOINT.json](validation/COLOR-INTEGRATION-CHECKPOINT.json).
It distinguishes historical full-run results from final focused runs and records
current binary hashes without inventing hashes for older overwritten binaries.

## Remaining work

The successful actual-normal fixtures use the declared conservative new-bake
capture: computed Mikk exactly matches the retained PBR frame, directions are
constant per triangle, direction morph deltas are zero and each triangle has one
rigid joint. This does not prove curved artist directions or mixed-joint normal
conversion. Old baked payloads are never relabelled. The next required stage is a
separately versioned new-bake interpretation in the retained original frame,
with exact PBR/MToon interpolation and independent deforming consumers.

UV1 source-map painting, actual COLOR_n/TEXCOORD_n morph channels, a current-color
adapter for arbitrary polygon topology edits and a standalone preconversion
restore action after reopening remain required. Complete raw origins and native
save/reopen do not substitute for those workflows. Scene12 polygon integration
must compose the origin graphs, exact authority witnesses and retained PBR/image/
tile/deformation budgets. The separate JSON/import allocation-safety checkpoint
is not implicitly included merely because normal sanitizer runs pass.

The source-color projection and bake computation here use the existing CPU
workers. Actual A770 evidence qualifies native rendering and independent export
consumers; it is not evidence of an accelerated bake kernel. Additional eligible
accelerator workloads remain part of the full objective.
