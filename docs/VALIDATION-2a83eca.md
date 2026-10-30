# Paged production integration validation — 2026-10-30

Product `ee6f539` = `803479d` + paging `a135f48`. Verified integration checkpoint
`2135d8d` preserves published `27b14d9`; subsequent publication is documentation
only. All300 source hashes and50 log hashes in local ignored manifest
`artifacts/paged-production-manifest-2135d8d.json` were independently matched
before publication, together with 12 GUI, 4 XR report and 30 binary hashes.
The parent independently reran paged-storage, paged-volume and paged-replay:
3/3 pass in 13.50s. Its local receipt is
`artifacts/paged-root-publication-review.json`. Prior exact
validation remains in VALIDATION-27b14d9.md and earlier archives.

The full methods, compiler/runtime/device configuration, per-flow counts and
initial failures are in PAGED-PRODUCTION-INTEGRATION.md; storage correctness and
paired performance method are in PAGED-SCULPT-STORAGE.md. This is a milestone,
not proof of the full original production objective.

| Scope | Result |
| --- | --- |
| CPU Release complete native/UI/render | 91/91,171.60s |
| CPU real-loader synthetic XR | 150+17,2/2,227.62s,explicit exit0 |
| Arc OneAPI selected native suites | 27/27,16.49s |
| Arc OneAPI real-loader synthetic XR | 150+17,2/2,258.40s,exit0 |
| Actual Arc GL | Document22,buffers24,MToon81,masks38,visibility25 |
| Native GUI and actual DomeD Wayland | 151 assertions,12 flows |
| Full C/C++ core/xatlas/Mikk/UI/render ASan/UBSan/leaks | 91/91,674.80s at7c95227 |
| Later test-only private-font configuration reload | native1/1,26.06s;sanitizer1/1,71.31s |
| Separate paging foundation CPU and whole-core sanitizer | 74/74 each;120.80s and634.65s |
| Allocation fault boundaries | 12,920 acrossStamp/End/Undo/Redo/recomposition |
| Three paired old/new numeric-state comparisons | all25 hashes exact in every trial |

Authoritative final logs are `tests-paged-production-xr-final.log` and
`tests-paged-production-sanitize-final.log` in the parent artifacts directory.
Earlier `tests-paged-production-sanitize.log` remains the initial89/91 failure;
it is not the final run. The two leaks were standalone Fontconfig lifecycle
fixtures: balanced test ownership fixed them without product-helper changes or
suppressions. Reloading the private exported font config was a later fixture
correction; its focused proof is distinguished above from the complete run.
The initial CPU XR wrapper143 is retained separately; the status-controlled
final invocation completed and returned0. No signal explanation was invented.

Actual compute identifies Intel Arc A770:184 independent endpoints,224 batches/
445088 samples,max error8.9407e-08,explicit missing-GPU rejection. Synthetic XR
uses synthetic tracking and softwareGL; actual Arc render fixtures independently
verify hardware. ROCm hardware and CUDA remain unverified as in REQUIREMENTS.md.
No fresh physical headset/controller/stylus evidence is claimed.

All source builds are regular-user optimized Release using canonical
DomeD282467e/FLTK1.4.5+22 patches with explicit lib64 runtime. Sanitizer builds
instrument both C and C++ matching archives. No ABI-mixed archive/header tests,
system installation or live desktop/session restarts are part of this milestone.
Artifacts remain local and ignored; a fresh clone contains source and methods.
