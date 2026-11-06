# Combined sculpt and material Release milestone

The private `work/sculpt-authoring-77e` candidate at `d1f40159` combines
local-coordinate sculpt chunks, owning polygon fields and ordinary texture sets
on main `44fe3742`. This is a qualified Release checkpoint within the full
production objective. Its product changes have not been merged into main.

The fresh optimized build owns 282 project objects, thin archives and executables
across 19 selected targets, including the ordinary application and its reachable
core, support, renderer and OpenXR providers. All 15 selected native suites pass.
Integration exposed a real picking regression: a non-null but empty sculpt chunk
directory incorrectly classified packed-only geometry as mixed. The correction
preserves typed queries when chunks actually exist. Original failures and the
affected original outputs remain preserved.

The same immutable renderer consumers pass 51 existing polygon, 537 independent
mixed-material, 72 sculpt-chunk and 73 direct-upload controls on both software
OpenGL and authenticated Intel Arc A770. Authentication records the actual EGL
device, PCI03/renderD129, vendor/device IDs and open render-node descriptors.
An actual UHD run is refused. Eight balanced RGBA image pairs differ by at most
two byte values, within the unchanged tolerance of six; alpha is exact.

Actual ordinary FLTK/universal-menu workflows pass 19 polygon and 51 texture-set
checks, including history, cancellation, ordered Save, clean close and a fresh
process reopen. These runs use private X11 displays. They do not establish new
compact, Wayland or physical-headset acceptance. Source, SDK, owned outputs and
observed mapped libraries remain unchanged through the recorded checks.

The pinned official Khronos validator reports zero errors, five warnings and
two informational issues across the five actual current GLB exports. Issues are
unfiltered. The `NODE_SKINNED_MESH_NON_ROOT` warnings remain relevant to broader
independent transform/deformation qualification.

A fresh matching ASan/UBSan build is running; no sanitizer pass is claimed by
this milestone. Current combined OpenXR/compositor/physical workflows, sustained
production-size performance and the remaining requirements still need evidence.
The newer sourceful texture-set persistence and all-channel resolution changes
remain separate agent branches awaiting runtime qualification. Controller
spatial drawers remain required after the basic targets.

See [the receipt](validation/SCULPT-AUTHORING-RELEASE.json) for exact raw evidence
paths and hashes. Current Release outputs and Arc images are RAM-backed under
`/tmp`; persistent receipts and logs do not make those raw outputs reboot durable.
Preserve them explicitly before cleanup. No installation, headset launch or
desktop-session restart occurred. The full goal remains active and incomplete.
