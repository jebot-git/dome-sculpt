# Canonical toolkit recovery — 2026-11-26

The Nov18 temporary Release/sanitizer SDKs are absent at resume. The installed
Release prefix `/opt/fltk-vr/toolkit` remains available. Canonical DomeD local
and live remote main both resolve to
`acc2b70ae61dedec2a23727f67c60c0303d0f76f`.

The [new static receipt](validation/installed-toolkit-static-20261126.json)
records44 matching checks: the canonical builder, all27 installed patches,
pinned upstream archive, generated poll configuration, all12 shared-library
path hashes and canonical/vendored bitmap helper. Poll is actually enabled in
the retained private configuration. The bitmap runtime prefix remains
`/opt/fltk-vr/bdf-runtime`.

Fresh Release application configuration may use
`FLTK_DIR=/opt/fltk-vr/toolkit/share/fltk`, with that toolkit and explicit bitmap
runtime in the selected library paths. This is an installed Release SDK check,
not a rebuild, sanitizer SDK qualification, new loader pass or application/UI/
device acceptance. New runtime validation remains required. No system file or
active session was changed.

Fresh native-only component and sanitizer builds may use the supported
`DOME_BUILD_UI=OFF` and `DOME_BUILD_XR=OFF` configuration; this does not qualify
their desktop or XR frontends. Their actual source, compiler, dependency and
runtime evidence must remain distinct from missing historical artifacts.

## Rebuilt sanitizer SDK

The canonical builder now also completes a fresh whole-owned ASan/UBSan FLTK
build at `artifacts/sdk-20261126/toolkit-sanitize` in116.134 seconds. All484
actual toolkit/support compiler owners use O1/g1, ASan+UBSan and frame pointers;
their real commands, objects and retained raw compiler depfiles are checked.
All165 FLTK public headers match the installed Release prefix. The installed
profile binds27 patches, poll configuration and new shared-library digests;
static ELF inspection confirms the core links both sanitizers.

Two ordinary negative platform-header configure probes remain recorded. An
initial audit also misclassified two successful CMakeTmp feature probes as
toolkit owners; its failed observation is preserved, and the final audit
separates configure probes from the484 real build owners. No source or
instrumentation guard changed. The completed transient unit had already
auto-unloaded when inspected; builder exit0 and the real journal are retained.

The [receipt](validation/canonical-sdk-sanitize-20261126.json) and compressed
raw evidence cover compilation and static artifact verification. Desktop/XR
application runtime, actual mapped SDKs and leak/device behavior remain separate
gates. This prefix is a regular-user local installation; no system files or
active sessions changed.
