# Combined cut and sculpt protection verification — 2026-10-30

Application source 4755e62665c55ea6f70aa4cfda04b6c657f951d5 merges feature 55dafa0 /
source 2ec8a5a with main 9a0cfc4 / source 20c132d. Harness-only3e602ae adds the
canonical90s cold-BDF startup allowance; it does not alter product source or
interaction/publication assertions. All 182 product hashes match 4755e62 in
artifacts/cut-sculpt-product-freeze.json. Later handoff edits are documentation.
The full production objective remains incomplete. Prior proof is preserved
separately in VALIDATION-55dafa0.md and VALIDATION-9a0cfc4.md.

Builds/artifacts are ignored local evidence and do not accompany a fresh clone.
Logs below are under artifacts/ unless stated otherwise.

## Release and real-loader XR

All 76 registered Release suites pass across two terminal runs at the same product
source:75 non-XR CTests in 625.60s, then openxr-client in 372.17s. Logs:
tests-cut-sculpt-native.log / -native-detail.log / -native-output.log and
tests-cut-sculpt-xr.log / -xr-detail.log / -xr-output.log.

The final real-loader report build-cut-sculpt/xr-client/report.json has 150 checks,
failed=0, complete=true, status=passed; copied to report-cut-sculpt-xr.json. It
retains upstream 140 and the 10 cut checks. The per-client40-second bound is
unchanged. Actual GL availability drives asynchronous cut/mask readiness; tests
use joystick, ray sliders and pressure, then inspect native geometry/source data.
Cut Apply yields a closed grouped shell/cap at independently expected
x=0.06928203m; unconfirmed standalone shutdown preserves the exact original.
Masks include mirrored pressure positions, four-row origins/view, focus/Cancel,
bulk publication/reopening and exact Undo. Existing input, UV, bake, skin,
retopology, stereo/resource/rebase and save/shutdown coverage remains.

Tracking and graphics are synthetic/software in that harness. This is not
physical HMD/controller/stylus, reconnect or production performance proof.

Cut-authoring now has 68 assertions. New cases cover exact protected Volume v3 /
symmetry retention, ordinary cap sentinels, source-edit isolation, cancellation,
white paint unaffected by the display overlay, new-output overlay cleanup and
Undo. Mask-only fields are excluded from cut/retopo/voxel/bake geometry choices.
The original nearby-surface provenance, mapped paint/channels, pose/group,
invalid/stale/no-op, native/glTF and controller ownership cases remain. Independent
mask regressions require exact constant samples, finite-position rejection,
constant sparse cells and a known affine field. Eight focused suites pass 2.62s
before the final broad runs: tests-cut-sculpt-refined.log.

## Whole-core sanitizers

All 23 selected C/C++ ASan/UBSan/leak suites pass in 320.94s, zero failures:
sculpt geometry/layers, shared document, durable/document recovery, document
assets, project files, scene compatibility, shutdown; mask controller/document/
symmetry/dispatch/transactions; retopology authoring/cancellation; cut authoring/
cancellation; skin authoring, bake, glTF interoperability, atlas robustness and
SourceUV adoption. Logs: tests-cut-sculpt-sanitize.log, -sanitize-detail.log and
-sanitize-output.log; build-cut-sculpt-sanitize.log.

build-sanitize has UI/XR OFF. Core C/C++, native Mikk and xatlas are instrumented
with -fsanitize=address,undefined and frame pointers; Release uses-O1 -g1 -DNDEBUG.
Runs set ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 and
UBSAN_OPTIONS=halt_on_error=1. All affected layout consumers were rebuilt.
No TSAN or new accelerator execution is claimed.

The deterministic shutdown fixture includes all six workers. fsync barriers
exercise accepted cut/retopo cancellation and retry without arbitrary race sleeps
or partial publication. The instrumented mask fixture retains preparation/
post-compute allocation failures and allocation-free rollback assertions.

## Native FLTK and BDF

All 125 GUI checks pass. Every process exits0 and each report has failed 0:

| Flow | Checks | Log prefix menu-cut-sculpt- |
| --- | ---: | --- |
| Masks/symmetry | 17 | mask.log |
| Masks, compact2× BDF | 5 | mask-compact.log |
| Plane cut | 12 | cut.log |
| Plane cut, compact2× BDF | 6 | cut-compact.log |
| Retopology | 12 | retopo.log |
| Retopology, compact2× BDF | 6 | retopo-compact.log |
| General menus | 53 | general.log |
| Skin | 11 | skin.log |
| Unicode | 3 | unicode.log |

Scripts were copied unchanged to artifacts/cut-sculpt-ui/tests and its build
symlink targets build-cut-sculpt, not an older executable. Reports/captures are
under artifacts/cut-sculpt-ui/artifacts. Optional uv orchestration uses dbus-next,
python-xlib and pillow with private HOME/bus/Xvfb. Runtime remains native C++.
The mask harness now matches the existing90s cold startup allowance; interactions
keep 30s. Actual exported availability/check/radio state, modal ownership,
Cancel-default/Close/Ctrl+W, text/UV/draft/global history, chooser callbacks,
host loss/return, pressure feedback, publication and retained data are exercised.

Visually inspected current mask-options.png and mask-options-end.png at 600x320
with 2× BDF: readable wrapped text, scrollable content and fixed Cancel/Apply without
overlap. painted-protection.png shows actual pressure feedback on retained
geometry. Compact cut controls and retopo/skin/menu reports retain their complete
independent assertions. No new actual DomeD Wayland-host run is claimed; its
older9+8 checks remain scoped to source 20c132d in the archived validation.

All 30 source_sha256 entries match third_party/sources.json and the BDF helper
matches canonical ../qimgvr/fltk/bdf_font.hpp. Actual linkage in
linkage-cut-sculpt.log resolves artifacts/toolkit-authoring/lib FLTK with no
Qt/GTK/Pango frontend. Canonical toolkit 282467e / 22 patches is unchanged.

## Actual Intel graphics

Three additional processes pass with LIBGL_ALWAYS_SOFTWARE=0 on
Mesa Intel(R) Graphics (ADL-N). MToon 81 and protection/mirror 38 checks pass their
--hardware guards, which reject llvmpipe/softpipe. The complete spatial render
fixture passes, including actual clipped plane/cap/menu, cached retopo transforms,
BDF palette, canonical normals and AO. Logs:
tests-cut-sculpt-hardware-{mtoon,mask,spatial}.log. Captures:
gpu-cut-sculpt-hardware-*.ppm. This proves local OpenGL rendering, not Arc/ONEAPI
compute or physical XR input/tracking.

## Diagnostics and limits

Initial build-cut-sculpt.log completes; the first five focused suites pass 4/5.
The added exact protected-cut oracle exposed real uniform-mask drift:
cut-mask-probe.log records 0.7499998212..0.7500000596 for global 0.75, exact -1 caps
and matching attribute counts. No assertion was loosened. Nested interpolation
plus the validated uniform shortcut correct the math. An intermediate compile
used CowMap::empty, which is absent; supported size()==0 corrects the mistake.
build-cut-sculpt-refined.log retains that failure, refined2.log is successful.
GCC15's unchanged write_glb vector-path warning remains in the initial full build;
current sanitizer glTF/authoring cases pass, with no new warning in edited code.

Initial mask GUI launches used an incorrect compact environment flag, both chose
normal flow/output, and timed out at 30s before window discovery. Their logs are
menu-cut-sculpt-mask-initial.log / mask-compact-initial.log, with the raced output
preserved as menu-sculpt-mask-misconfigured. Correct flag
DOME_SCULPT_MASK_COMPACT=1 selects the independent compact flow; only cold startup
was aligned to the existing90s budget. Final 17+5 checks preserve all data oracles.

The first full XR attempt, concurrent with sanitizer/hardware checks, completed
102 checks without assertion failures but timed out its cut client after 40s.
Its incremental report had56 completed frames, enabled preview/Apply and a real
slider sample; no worker deadlock was inferred. The timed-out process was
confirmed terminal before retry. Failed logs/details/output and all reports are
preserved as tests-cut-sculpt-xr*-concurrent-failed* and
artifacts/xr-cut-sculpt-concurrent-failed. The final complete isolated replay
passes 150 with the same40-second deadline and unchanged product/oracles.
This comparison does not replace profiling or certify cold/software latency.

Cuts remain active-object operations. Group-wide repeated cuts, general Booleans,
stronger contours, source compaction/GPU caching, durable recipes, quality,
complete material/skin/morph transfer and draft recovery remain unfinished.
Static output uses 512-pixel PBR atlases while originals retain their full data.
Production local meshing/storage/resolution and every remaining REQUIREMENTS row
stay open; no complete Divine Cut or full application claim follows here.

All build/test/app handles were checked terminal; final inventory found none.
No system installation, session restart, fresh external Khronos validation,
physical XR/input-device or new OneAPI/ROCm/CUDA execution occurred. No local
icpx/sycl-ls/hipcc/nvcc was found in PATH, standard prefixes or local tool/artifact
roots. Prior Arc/ONEAPI/HIP results remain scoped to their archived sources.
