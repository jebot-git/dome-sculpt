# Scene20 current-colour target authoring integration

This checkpoint extends the qualified rigid/source avatar application with real
current-corner RGBA target preparation and Bake. It uses the existing native
FLTK/OpenXR application and shared document workers. Scene20/material6/polygon5
retain the exact geometry-bound field, basis and prior-source owners across
preparation, Bake, Undo/Redo and Save/reopen. Source and target can carry distinct
fields in the same transaction. Read CURRENT-COLOR-TARGET-WIP.md,
CURRENT-COLOR-DUAL-BAKE.md and JOINT-AUTHORING.md for component boundaries.

Product7566a8f combines the rigid/source application with frozen65b0ae9 and repairs
an obsolete target-exclusion expectation. Test-only5c94862 adds the qualified
dual/joint fixtures and gives the large rigid export oracle600 rather than240
seconds. Runtime/header/asset/third_party bytes remain identical to7566a8f.
No new Source21/22/23 sampling or UV-colour adapters are admitted here. Public merge8cfb53f
has runtime/header/asset/third_party/CMake/test bytes identical to qualified277474f.

The fresh optimized all-target FLTK/OpenXR build passed. The full368-suite Release
run passed in2827.37s. Two subsequently added registered workflow suites pass
separately in35.43s; this is not a single370-suite run. The actual22-part avatar
workflow combines normal preparation, repeated integer UV moves, key creation/
replacement, a linked head pose and rigid descendants with independent source/
target colour Bake and one Scene20 save. It verifies all112 exported material
role images/samplers and independent native/GLB/VRM images. The separate dual
fixture covers sixteen seam/alpha/pose cases with independently owned fields.

Fresh owned C/C++core/support/renderer ASan/UBSan/leak checks initially pass51/52
native suites: the sole failure is the rigid export oracle's240.25s timeout.
The same unchanged binary directly passes all424070 assertions in254.51s with a
600s budget. The corrected registered rigid oracle passes203.15s; the added dual/joint
fixtures separately pass185/70 checks in114.21/142.17s. Together these are3/3
registered suites in459.54s, alongside the initial51/52 run, not a single
54-suite sanitizer run. The separate seven graphics suites pass1358.67s. External system
libraries, a fully instrumented FLTK frontend and a whole-project sanitizer run
are not implied. The component's complete51598-boundary Release denial sweep and
extended sanitizer scope are recorded separately in TARGET-COLOR-SAN-COMPLETION.md.
The original42/44 sanitizer pass plus four exact samebinary finer-range cases
complete all51598 unique boundaries. Preserve the original two timeouts; only
future registered budgets for those shards change600→1200 seconds.

Seven actual Intel Arc A770 batches pass761 checks: source Bake133, post-UV keys52,
actual avatars87, source VRM37, normalUV46, rigid VRM49 and target Bake357. These
are hardware OpenGL consumers using explicit PCI0000:03:00.0, not new OneAPI
compute or physical-headset qualification. Target preparation has a declared
projection error bound: actual256-resolution import views differ by at most one
encoded level, and prepared/baked exports match exactly. Default64 is tested
against its actual linear bound; no universal two-level64-resolution claim is made.

Actual native FLTK target workflow9, compact workflow10 (including real scroll
end), post-UV keys17 and active-unskinned avatar export7 checks pass. Latest
private DomeD d271cf6 native Wayland/universal-menu checks pass8. The actual OpenXR
loader target fixture passes11 with synthetic inputs. The accepted physical
selection interface and saved session remain preserved; no new physical launch,
system installation or active desktop restart occurred.

All170 emitted files pass Khronos with0 errors and1194 unfiltered warnings under
unlimited maxIssues. All71 emitted VRM0 files pass15 official schemas pinned at
94e82dd346fa6cf0337c4421728640e5252dd38e, each Git-blob hash checked. Raw input
variants are excluded from emitted-file claims. Exact commands, proof roots,
source/binary hashes, original failures and terminal results are in the receipt.

Current-field UV/topology and unsupported prepared-normal/artist/UV1 transfers
remain guarded pending real adapters. Pending generic Bake saves still refuse;
completed acceptance and separate ordered workers retain their documented
semantics. Broader rig fitting/rest/reparent, physics, remaining painting and
production/device/backend work stay required. The full goal is active. The newly
requested controller-mounted spatial drawers are required after the basic targets;
existing selection and joystick menus do not satisfy that additional phase.
