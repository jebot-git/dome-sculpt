# Public XR Save acknowledgment — 2026-11-11

The restored baseline application now completes all eight texture-resolution XR
workflows with the stronger public-image fixture: **117 checks in Release and
117 under ASan/UBSan/LSan**, with clean application exits and unchanged inputs.
All 16 cross-profile input/final native file pairs are byte-identical. These are
actual OpenXR-loader, released OpenGL-image and native-file checks with synthetic
controllers, not physical-headset acceptance or completion of the full goal.

Product source is `b8c469ce875eb2542c220300c3c63edbf1526570`: restored `03f8edf1`
plus the current canonical BDF helper. The application was not patched by this
work. The fixture changes are `296d8433` and calibration correction `17cd3b66`.
The latest canonical toolkit is DomeD `acc2b70a`, built into
`artifacts/sdk-20261111/toolkit-{release,sanitize}`. The standalone fixture DSO
was compiled in full for each profile, using that profile's configuration.

| Owning ELF | SHA256 |
| --- | --- |
| Release application | `9bf85d3bbc3418951703ad1aabb0d55a467d36d9ed621ede664007b6c930b244` |
| Sanitized application | `38978d63d7248629ae34969bfd3fd6a8af0ae168222926d42ef6c41f42d13f8e` |
| Release improved runtime | `5eee15b0e543b06089c5866f42f4904d9de2c7fcec2cb605a7e242c00ea82422` |
| Sanitized improved runtime | `4e36b2ff613c2fdb8b1c2e9267baca5ba9a51de3c0bb109ccf582caefb7a9d86` |

## What the fixture observes

The old ROOT gate accepted its 19-pixel title signature for three rendered
frames. It did not establish that Save was selected or that the menu represented
the requested history endpoint. The replacement retains that exact title check
and also requires the public Save6 accent and the Redo5 label's enabled state:
disabled after Apply/Redo, enabled after Undo. It observes the closed palette
after each Save press before advancing to the existing durable-native gate.
Visible ROOT with an incorrect selection/history state is not clicked again.

The probes come from `src/palette.cpp`: the 640px palette spans .6m; radial cells
have radius220 and size110x40, Save6 center(100,320), Redo5 center(129,210).
Displayed sRGB is inspected after the real framebuffer transfer. The fixture
samples Save's text-free left interior, the Redo glyphs, and four empty margins
below the selection toolbar. ROOT recognition remains learned from the initial
released image. No private product observer, wrapped submit, synchronous worker
wait, direct dispatcher call or extra success marker was introduced.

For sanitized extra256, actual ROOT acknowledgments were physical41–43,
152–155 and239–241; each shows Save selected, with Redo disabled/enabled/disabled.
Closed-menu acknowledgments were47–50,157–159 and243–246. Empty margins supplied
324/324 background samples; selected Save supplied45/45 accent samples. The
subsequent accepted256, undone128 and redone256 Saves match the complete native
oracles, including all sibling data. The compact receipt contains the values.

All original 99 checks, eight modes, exact native/history/FLOAT/layer/sibling
oracles, original **6000-frame ceiling and 240-second per-case deadline** remain.
There are 18 additional public UI checks. First Save-attempt/outcome images and
bounded periodic waiting-gate images are retained. The complete matched suites
took111.75s Release and263.82s sanitizer; the latter is the sum of eight separate
cases, not a changed per-case limit.

## Original failures and limits of the diagnosis

The November6 failure remains preserved in
`artifacts/integration-03f8-sanitize-seal/control-evidence.tar.zst`; no historical
receipt or source was overwritten. A new original-fixture sanitized extra256
attempt first failed before XR because the retained private Mesa depended on
the now-missing `libLLVM.so.19.1`. That startup failure is separate from Redo.

Using current system Mesa26.2.5/LLVM22.1.8, the original fixture reproduced gate73:
accepted and undone Saves passed; runtime input recorded Redo180, menu182 and
Save186, but the file remained byte-identical to original128 until6000 frames.
The initial ROOT69 capture still highlighted Clay0; it does not establish the
selection at the later Save press. Teardown additionally exposed the existing
112-byte/two-allocation Mesa rtasm JIT leak. A clear-only EGL probe had passed
because it did not exercise this JIT path. That probe is not claimed as full-app
sanitizer qualification.

An isolated improved-fixture sanitized extra256 run on that **same current
system Mesa/LLVM22 route and identical application ELF** completes all three
exact native Saves and six public UI checks in48.39s. Its overall exit still
fails on the independent112-byte driver leak; it is not a sanitizer pass. The
improved Release fixture also passes all eight cases/117 checks on that route.

The original fixture also **passes** a fresh same-recovered-driver extra256
control: 17 checks,267 frames, clean exit,59.21s. Therefore the original failure
is timing/environment-sensitive; these observations do not prove a fixture-only
cause or a general application input-loss bug. They justify the stronger public
acknowledgment and establish its current complete functional result.

The first improved Release calibration was deliberately stopped after its new
background probe incorrectly intersected the existing top toolbar (162/324
samples). The failed calibration and guarded SIGTERM record are retained. The
correction samples empty margins and was rebuilt before the successful suites.

## Recovered dependency closure and retained evidence

The passing matched route uses retained, actually patched private Mesa26.2.5 EGL
and Gallium ELFs with a privately extracted **signed** Fedora
`llvm19-libs-19.1.7-18.fc44.x86_64` package from the configured Nobara repository.
Package SHA256 is
`0449fcdf119a271d785727e95d6d87cdae75ce35dc1c375ae26bcd51113b3b52`;
the loaded LLVM library is
`b4bc4658b1ad3d53bd8c9d8d507cf7bc985bb13bb08e0814390c7070e267639a`.
RPM header signature and payload digest verify. Package license is
`Apache-2.0 WITH LLVM-exception OR NCSA`; the shipped license remains in the
extracted package. Mesa source/patch provenance remains in the preexisting
`artifacts/mesa-xr-leak-26.2.5/proof` records. Current support-library mappings
are pinned; historical environment equivalence is not claimed.

No package was installed, no system/display session was restarted, and no leak
suppression was added. Sanitized runs retain `detect_leaks=1`, halt/abort on
error and UBSan stack traces; `LSAN_OPTIONS` is unset.

Persistent raw outputs, native files, images, process maps, scripts, failure and
success logs are under `artifacts/xr-recovery-20261111`. Start with
`matched-xr-success.json`, `saved-redo-diagnosis.json` and `runtime-recovery.json`.
Each stage pins its app/runtime/inspector/fixtures and actual mapped ELF closure;
all before/after input checks pass. Fixture compilation provenance is in
`artifacts/xr-save-ack-20261111/artifacts/fixture-build-proof-current.json`.
The portable summary is [validation/XR-SAVED-REDO-20261111.json](validation/XR-SAVED-REDO-20261111.json).

Main product admission, separate current reader SAN and GUI qualification,
broader interoperability/accelerators, physical acceptance and the remaining
original requirements are not completed by this fixture milestone.
