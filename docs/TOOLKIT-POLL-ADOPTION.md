# Canonical27 picker and poll SDK adoption

The final canonical toolkit fixes three concrete native-picker failures:
compact windows outside the work area, Paste inheriting a stale Enter event,
and clipboard metadata/ownership errors. The upstream poll backend also removes
the observed display-descriptor1101 select overflow. The canonical code pin is
`6fa2624f89ddb105e482326995daec0bdbfd6643`; qualification metadata is `341152b`.
All165 public include files and existing exported symbols remain exact.

Sculpt's portable guard/provenance change is commit
`276853746e77a133cfb6f32123e14da6aa5b2692`, only CMakeLists.txt and
third_party/sources.json. Desktop configure requires installed patch25/26/27
checksums and the actual poll runtime profile. It verifies the retained private
configuration, builder and combined patch digest, then resolves every imported
FLTK library within that profile's prefix and checks its bytes. Eight configure
controls pass: old24/25/26, false poll profile, actual OFF configuration, stale
imported libraries and changed library bytes all refuse specifically; final
SDK configures. These are configuration checks, not build evidence.

Final immutable toolkit prefixes are:

- `/home/blux/de-xr/DomeD/artifacts/fltk-poll-final/artifacts/toolkit-poll-final-release`
- `/home/blux/de-xr/DomeD/artifacts/fltk-poll-final/artifacts/toolkit-poll-final-sanitize`

The matching unchanged bitmap prefix is
`/home/blux/de-xr/DomeD/artifacts/cairo-shm-drain-f797/artifacts/bitmap-runtime`.
Its actual source remains53b0fca, reverified byte-identical at the new pin. The
sanitizer toolkit instruments owned toolkit/libdecor C/C++; the external bitmap
stack retains its separate scope. No sanitizer suppression is used.

This qualification copies exact completed coherent c092 optimized and owned
ASan/UBSan applications, inspector/helpers and all app static-link support.
It changes the dynamically loaded SDK and records actual process maps, with
identical public headers. It does not claim a fresh Sculpt build or introduce
product-source/API changes. Old binaries and prefixes remain immutable.

Four actual private-X11 universal-menu runs pass: normal and640×360/font32,
Release and sanitizer, each15 named checks and clean exit. They exercise real
editable export policy, Cancel/Close, native Unicode directory navigation,
filename input, accepted worker export, native package Open as unsaved project,
subsequent Save and exact original/source preservation. The filename harness
uses real menu SelectAll/Paste/Copy, requires clipboard ownership to transfer to
the actual application X-client base, then compares exact copied text. Paste
must keep the chooser open; actual Return on proven filename focus navigates or
accepts. No guessed filename/OK pixel coordinates remain.

Two native Wayland runs pass9 named assertions each, preserving image-chooser
Cancel/Return, projection paint, ordered Save, exact Undo/Redo and clean close.
The final reports contain9, correcting earlier11/22 progress shorthand; strict
readiness/Return assertions also execute. All four X11 cohorts preserve3,892
input hashes each; both Wayland cohorts preserve3,893 each. CMake/test bytes are
exact to those cohorts. Provenance JSON prose/source pin was finalized afterward
and is explicitly separated in the receipt; its final form passes configure.

Retained negatives include the old oversized picker, sender-owned false ACK,
old25/26 strict owner failures, old26's206-byte/four-allocation clean-exit leak,
and both select-based high-FD sanitizer controls. Earlier exploratory passes
are not relabeled final runtime proof. The final canonical toolkit separately
passes12 protocol/picker commands,49 clipboard checks per profile and7 genuine
high-FD checks per profile, with all1,523 inputs unchanged per command.

The machine receipt is
[validation/TOOLKIT-POLL-ADOPTION.json](validation/TOOLKIT-POLL-ADOPTION.json).
It records exact commands, outputs, SDK profiles, signed private-Xvfb provenance,
ABI and copied-runtime manifests. Testing used private buses/namespaces and a
nested canonical compositor/software GL. No user desktop, physical headset,
system installation, display-manager or session was changed. Root owns later
Sculpt main adoption and its own current-public runtime qualification.
