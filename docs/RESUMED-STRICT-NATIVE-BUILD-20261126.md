# Fresh strict native build — 2026-11-26

Production source `05f5d1ccfa4bb5b97d25e4a425a2fd9497b0d8c9` now has a fresh
native-only sanitizer build after the earlier temporary artifacts disappeared.
No old objects or executable results are reused. Supported UI/XR-off configuration
builds the complete197-owner core, rendering/support libraries and13 selected
fixture executables in988.818 seconds, with229 actual complete compiler owners.
All effective optimization flags are O1; every owned compiler command enables
ASan+UBSan and frame pointers. Actual compiler depfiles cover1224 input paths;
all archive payloads and selected links are independently checked with no errors.

A separate owning follow-up compiles the complete source-map regression fixture
and links it against unchanged parent archives:230 unique current compiler owners
and14 executables. The parent audit remains unchanged, with a separate follow-up
receipt. There is no Release or earlier-layout object substitution. The real
blur-table derived input and generator command are recorded separately.

The compiler driver/source/header inputs were observed before these builds;
cc1/cc1plus/collect2 supplemental observations occurred during the parent build
and are explicitly not represented as prospective observations for already
completed compiles. The [receipt and raw evidence](validation/resumed-strict-native-build-20261126.json)
retain commands, raw MDs, actual objects/archive identities, link metadata,
configuration and terminal state. The original driver PID is independently absent.

The selected native/software runtime is a separate gate. It retains original
datasets and case/RSS limits, with no AS cap for sanitizers. The advanced17-family
2048 workflow has an additional timing-only complete fixture TU on the same
production core (`91a90a67`); it changes no checks, data or product behavior.
Neither compilation qualifies an App, desktop, XR, physical headset or the full
production objective. Fresh canonical sanitizer FLTK is qualified separately in
[the toolkit record](CURRENT-TOOLKIT-20261126.md).
