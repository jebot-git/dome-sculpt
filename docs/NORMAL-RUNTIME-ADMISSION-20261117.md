# Qualified normal-source runtime integration

Main now accepts genuine original artist normals and ordinary prepared-v1
normals as sources for another Bake. This extends the existing native document,
worker and FLTK/OpenXR controls. It preserves separate PBR/MToon frame roles,
original sampling coordinates, source selection, cancellation and exact history.
The newer advanced Rasterize/RetainSource adapter remains a separate candidate.

The merge preserves both histories. All **1719 non-document files** match
`f1e193c7` exactly; only `docs/`, `AGENTS.md` and `README.md` are excluded from
that comparison. Production is `1bc13125`. The owning Release and sanitizer
builds use frozen `090f009e`; public fixture `f1e193c7` has its own complete-TU,
fresh-link and actual application evidence, with runtime/font/scenario sources
explicitly `6c8f042c`. This is not a claim of a whole-f1 owning build.

Actual qualification:

- [Optimized Release owning build](NATIVE90-RELEASE-OWNING-20261117.md): 1036
  complete owners, including 291 freshly compiled and 745 authenticated reused
  objects; 737 fresh links, three generators and 1776 outputs.
- [Release native runtime](NATIVE90-RELEASE-ACTUAL-20261117.md): all 90 selected
  suites, ordinary version and workspace/raw-source controls pass.
- [Strict sanitizer owning build](NATIVE90-SANITIZE-OWNING-20261117.md) and
  [runtime](NATIVE90-SANITIZE-ACTUAL-20261117.md): all 90 suites pass with
  ASan/UBSan, leak detection and no suppressions, plus version,
  workspace27/373 and raw90159 controls. Final input drift and owned-process
  lists are empty; actual units exit successfully and are stopped after capture.
- [Whole application workflows](WHOLE090F-PUBLIC-UI64-XR260-20261117.md): 64
  desktop and 260 authenticated Arc synthetic-XR checks pass, including actual
  source choice, Bake, immediate Save, history, reopen and negative controls.
- [Native Wayland/DomeD](WHOLE090F-WAYLAND-GPU-ACTUAL-20261117.md): 14 actual
  controls, visible canvas and Arc GPU checks pass through the current canonical
  compositor. Its nested diagnostic presentation includes a copy.
- [Independent normal consumers](ARTIST-NORMAL-GRAPHICS-20261115.md): separate
  software and actual Arc profiles each pass 24 cases and 318 image witnesses.

The [machine admission](validation/NORMAL-RUNTIME-ADMISSION-20261117.json)
binds actual status hashes, terminal properties, source roles and the precommit
body comparison. Earlier executor, fixture and observer failures remain separate
records. No old failure is relabeled as a success.

This is an incremental functional integration milestone. Physical headset and
native DRM/zero-copy acceptance, system installation, the advanced normal-source
families and every unfinished production requirement remain outstanding. The
full goal stays active; see [current progress](PROGRESS-ASSESSMENT-20261117.md)
and the complete [requirements](REQUIREMENTS.md).
