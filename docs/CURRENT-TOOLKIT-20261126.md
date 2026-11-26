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
