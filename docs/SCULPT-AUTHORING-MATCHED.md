# Matched sculpt and material qualification

Private candidate `d1f40159` now passes the same selected checks in optimized
Release and ASan/UBSan builds. Each profile owns 252 project objects, 11 thin
archives and 19 executables, including the ordinary application and reachable
core, support, rendering and OpenXR providers. All 564 binary inputs are
independently rehashed. No sanitizer suppression is used.

Per profile, the completed checks are:

- 15 selected native suites;
- 733 software OpenGL controls;
- 70 ordinary FLTK/universal-menu editing, history, cancellation, Save and
  fresh-process reopen checks on private X11 displays;
- 103 actual-loader OpenXR checks with synthetic controllers, covering the
  remeshing Apply, Cancel, focus, reference-space and Undo routes.

The same Release consumers also pass all 733 graphics controls on authenticated
Arc A770 hardware. That is separate from the software sanitizer runs and does
not establish new compute or physical-headset coverage.

Across profiles, 64 native/graphics artifacts, 20 XR artifacts and both GUI
native project endpoints are byte-identical. All four GUI viewport/dialog images
are pixel-identical. Two whole-window PNGs differ only in the status bar's Saved
path text because their output directories differ; the original PNG differences
remain recorded. Five actual GLB exports have zero Khronos errors, five warnings
and two informational issues, without filtering. The sanitizer exports contain
the same bytes.

The initial sanitizer build completed compilation but stopped at the workspace
reserve guard during final links. After verified archival of closed older
objects and withdrawal of a duplicate backup copy, an unchanged-input
continuation completed the nine remaining executables. All 273 prior whole
inputs, compiler/linker flags, source and SDK bytes stayed unchanged. Original
failures are preserved. The 237 archived older authoring objects can be restored
with `artifacts/closed-authoring-77e-20261106/restore.py INDEX`; restore them before
auditing links against that older cohort. Its source, thin indexes and standalone
executables remain available. Current Release and sanitizer providers remain raw.

This does **not** yet admit the product branch into main. Review found independent
VRM1 normal, Shift and masked-Rim expansions inherited through earlier private
composition. Their source matches historically qualified components, but the
current sculpt/material checks do not execute those VRM routes. Current union
regression is required before admission; historical component evidence is not
relabelled as current coverage.

The first production-size sculpt inventory also exposed a real limitation:
the cold P1 scene at voxel size 0.0125 is rejected by the existing working-byte
guard before publishing an accepted mesh. No production triangle count or timing
is claimed. A separate branch is replacing repeated initial index path copying
with bounded sorted construction and proving local scratch lifetimes, while
keeping the existing memory limits.

Sourceful texture-set persistence/export and resolution controls remain newer,
separate branches awaiting combined runtime tests. Full production performance,
broader authoring/avatar/interoperability/backend/device requirements and
controller drawers remain open. The full goal stays active.

See [the matched receipt](validation/SCULPT-AUTHORING-MATCHED.json). The verified
full Release backup and current raw builds are RAM-backed; durable restaging
remains required before cleanup. No installation or physical headset launch is
part of this checkpoint.
