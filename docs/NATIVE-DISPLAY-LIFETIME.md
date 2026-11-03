# Native display and EGL lifetime integration

This extends published `bd2bcde` with the canonical DomeD `f797e0f` toolkit and
bitmap cleanup stack, an explicit process-owned desktop display connection, and
the previously qualified exception-safe windowless EGL owner. It preserves the
native FLTK desktop/OpenXR application, current authoring union, Scene/archive
formats and public class data layouts. The full production goal is incomplete.
Exact commands, durations, captured-input digests, runtime maps and negatives are
in [the receipt](validation/NATIVE-DISPLAY-LIFETIME.json).

The desktop display owner is constructed after the Fontconfig owner and before
appearance initialization and App. Destruction therefore drains workers, menu
callbacks, widgets and GL contexts before closing the toolkit connection, then
releases fonts. Headless and standalone-XR routes do not construct this owner.
The platform header stays in its implementation TU so Xlib macros do not escape
into editor types. Partial EGL constructor failure uses the same nonthrowing
cleanup as ordinary destruction and does not unbind an unrelated current context.
Read [NATIVE-GRAPHICS-LIFETIME.md](NATIVE-GRAPHICS-LIFETIME.md) for its31-check vendor-API
fault fixture, original-source negative and unchanged-layout evidence.

The selected FLTK1.4.5 must retain the canonical process-cleanup patch, and the
bitmap manifest must name the corrected Cairo source-cache patch. All three
FreeType/HarfBuzz/Cairo package prefixes must match. Configuration also rejects
cached absolute bitmap-library paths outside that prefix: live pkg-config
metadata can change while CMake retains earlier same-SONAME paths. The generated
launcher prepends both selected library directories ahead of inherited overrides.
See [TOOLKIT.md](TOOLKIT.md) for preparation and cache-switch instructions.

The matching toolkit is built against the new bitmap SDK, including a fully
instrumented FLTK/libdecor counterpart. The external bitmap libraries are
optimized Release; they are not claimed as instrumented. All153 public bitmap
headers are identical to the preceding SDK. Canonical component proofs in
[DomeD](https://github.com/jebot-git/DomeD/commit/f797e0f1d43e7be5a06128ca7b0b0b0259fefabd)
retain the old Cairo leak negative and exact unchanged Xlib pixel hashes.

## Qualified scopes

- Coherent owned CPU and O1 ASan/UBSan/leak native app/core/support/render/XR
  builds plus13 selected lifecycle/control/render targets pass. Unchanged cached
  TUs from the initial own builds are retained; these are not all-target builds
  or clean rebuilds of every TU at each iteration. All1,461 tracked inputs remain
  exact within each final build.
- Affected Release10/10 passes in21.59s; whole-owned sanitizer10/10 in73.94s.
  These include the actual desktop smoke/error-unwind/context-lifetime consumer,
  deterministic EGL31, selection/Unicode/spatial/rig-guide rendering, navigation
  and rig controls. Each captures1,533 unchanged source/library/binary inputs.
- Actual native Wayland, private latest-source DomeD and universal-menu controls
  pass11 Release and11 sanitizer checks in22.93s/32.66s. Live process maps prove
  the selected toolkit and bitmap libraries; all1,525/1,537 inputs are exact.
- Generated launchers pass two actual desktop capture/error checks and native
  version execution per profile despite inherited old toolkit/bitmap paths.
  Loader initialization records prove new FLTK/Cairo/FreeType/HarfBuzz selection.
- Native OneAPI/CUDA/ROCm application components compile, link and report their
  actual backend. These freshly compile app/main, display and graphics TUs against
  immutable matching `eec7753` owned backend/core/frontend support; no public data
  layout changed. They are not fresh whole-vendor/all-target rebuilds or positive
  AMD/NVIDIA hardware tests.
- The resulting actual OneAPI app passes strict A770/PCI03 GL+compute synthetic-XR7
  in26.34s, recording the production paint kernel and9 bounded output readbacks.
  All1,526 inputs are exact. Tracking is synthetic; no physical headset was used.

The final app/build evidence is source `7481d63`; `69ca191` changes only the native
Wayland Python observer, independently qualified by the final11+11 checks. The
publication appends documentation/receipts and corrects provenance prose, without
changing runtime, headers, build logic or test code from those qualified sources.

## Preserved negatives and remaining work

One initial Release Wayland test compared a pending compositor capture immediately
after closing the modal and failed its no-motion pixel guard. The unchanged-app
diagnostic passed11. The final observer requires fresh captures to settle before
retaining/comparing a frame and saves those images. In its Release modal-return
observation82,905 pixels change across fresh captures before matching the original
viewport exactly. The pixel threshold8, zero changed-pixel no-motion criterion,
positive movement checks and actual input sequence are unchanged. No navigation
or edit-authority implementation was changed to obtain that pass.

Initial Xlib-header compile failure, the deliberate sanitizer-build interruption
on that diagnosis, old toolkit/bitmap refusal, cached-library refusal, intermediate
RPATH-cycle configurations and vendor-link script error remain recorded locally.
No failed run is relabeled as a later pass. The earlier full CPU532/533 and its
separately qualified guide fix retain their original scopes; no full533/533 claim
is made here.

Release standalone-XR exit/Save/info3 passes. The whole-owned sanitizer app and
instrumented toolkit still complete16 frames and destroy the synthetic session,
then report112 bytes in two inherited Mesa allocations. Leak detection remains
on without suppression. The current-program unbind diagnostic did not fix that
report and introduced no production shader change. Physical device loss,
sustained performance, positive AMD/NVIDIA execution and all unfinished authoring/
interoperability rows in REQUIREMENTS.md remain required. Controller drawers are
still a later required phase. No system installation, active-session restart or
headset launch occurred.
