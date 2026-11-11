# Public GPU color blur workflow qualification, 2026-11-11

The unchanged 775 application passes all four new public desktop cases and both
real-loader XR cases. Test code is isolated from product sources. This qualifies
the native GPU color blur interaction and persistence scope; it does not complete
the painting goal, install a build or test physical-headset usability.

| Actual case | Checks | Seconds |
|---|---:|---:|
| release desktop normal |10|14.70|
| release desktop compact |10|13.23|
| sanitize desktop normal |10|31.39|
| sanitize desktop compact |10|31.31|
| release real-loader XR |39|12.85|
| sanitize real-loader XR |39|40.34|

Desktop uses the actual exported DomeD universal Paint command, ordinary Save,
standard Ctrl+Z/Ctrl+Shift+Z and a fresh application reopen. Actual whole native
scenes reach the independently pixel-oracle-checked accepted/original/redone
endpoints. Each source-defined complete Canvas changes visibly after Blur and
is byte-exact after Undo, Redo and fresh reopen. Only the source 28 px status region
is outside that Canvas comparison. The private Xvfb case does not show the
shell's global menu or establish native-Wayland/compositor behavior. It requests
Save as the next public action; it does not force a busy-worker timing window.
The separate original native controls prove accepted-pending Save ordering.

XR uses the ordinary app, genuine OpenXR loader, real Arc GL images and synthetic
controller/pose APIs. Each eye first recognizes the source-font ROOT9 bitmap and
pose, then exact page-title and checkbox shapes. Actual Painting/Erase becomes
checked, Filters remains unmarked while Erase is on, and Blur publishes one exact
global history step. Each accepted/undone/redone Save also requires selected ROOT
Save, correct public Undo/Redo availability and palette disappearance. Frames,
stereo image acquisition/release, session cleanup and native fresh reopen pass.
R/S final native files are byte-exact with each other and the independently
checked expected file. Release uses 164 frames and SAN 393, well inside unchanged
6000/240 limits. All relevant font/stencil patterns have zero pixel mismatches.

Current 775 original kernel 61444/shared-context 12/native 139458 hardware controls
also pass in each profile on the fresh owning whole39-target app/core providers.
Actual renderer, EGLDevice/DRM/PCI/i915, liveFD and mapped-ELF witnesses authenticate
Arc. The external measurement tool returns the original GL query pointer, passes
through non-EGL GLX calls and records each genuine EGL query separately. Neither
app callbacks nor private document state or an observer success marker authorize
UI progress. All 331 owned providers per profile remain unchanged. SAN uses leak
detection 1 and strict UB checks with no suppressions; original cohorts, desktop
and XR app processes all close cleanly.

The sourceFont oracle uses audited palette-text/session-appearance objects,
actual default Unifont 17.0.04 BDF, source 640 px placement and GL_NEAREST projection.
Painting 154 ink texels predict 13 left/10 right, exactly matching the failing earlier
images; no observed minimum was baked into the validator. Root/source shape and
full 8x8 checkbox/4x4 fill/absent-stencil recognition replace the alias-sensitive
new coarse title/fill conditions. Checked and unchecked differ by one observable
pixel in each 320 px eye. Existing ROOT/history/closure controls and old 81/117
source scripts remain unchanged. Those old scopes are not reexecuted or renamed
as part of these new 10/39 controls. Their own earlier evidence retains its scope.

Original failures remain: missing Scene-level PBR pairing; source-color conversion
after physical-emission authoring, both before EGL; a native 57853 pass followed by
an incorrect coordinator report-format assertion; and the first full XR run's
original 6000-frame failure at its new title criterion. A separate 48-frame capture
is diagnostic only. Corrected source pairing/order, public Flow 1 binding and
source-backed visual oracles preserve native validation and every PBR/FLOAT/
hidden/source/rig obligation. Source-color preparation on already authored
physical-emission paint remains a real full-goal adapter limitation.

The companion JSON pins all current statuses, exact source/binary/observer/font
inputs, original failures, immutable root providers, independent root audits,
actual images and read-only comparisons. Persistent evidence is
`artifacts/paint-workflow-dso-build-20261111`. No further GPU contexts are planned
for this scope; all device leases are released. The full painting/material/baking,
resident-cache/source-domain and physical-controller objectives remain open.
