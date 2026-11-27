# Dome Sculpt agent entry point

Read [docs/HANDOFF.md](docs/HANDOFF.md) first for the latest checkpoint, remaining
work and validation results. Preserve the full objective in
[docs/REQUIREMENTS.md](docs/REQUIREMENTS.md); this checkpoint is not completion.

Current admission: product `1bc13125` and public fixture `f1e193c7` are integrated
after Release90, strict sanitizer90, whole-application desktop64/syntheticXR260
and native Wayland14 qualification. Read
[docs/NORMAL-RUNTIME-ADMISSION-20261117.md](docs/NORMAL-RUNTIME-ADMISSION-20261117.md)
for exact source roles. The newer advanced `90fad800` adapter is separate and
must not be treated as admitted by those results. Historical notes below retain
their original dates and scopes; the current handoff takes precedence.

- Runtime: native C++20, FLTK frontends and direct EGL/OpenXR. Python is optional
  test orchestration only. Do not introduce Qt/GTK/Pango/Erlang runtime frontends.
- Fonts: native BDF/PCF through the canonical DomeD Fontconfig/FreeType helpers. Preserve DomeD
  theme, accent, bitmap size/scale, opacity and background conventions.
- Universal menus: actual commands, context-aware enabled/check/radio states,
  modal text/UV/document history routing, shell-owned application/Window menus,
  Ctrl+W/Escape cancellation and Cancel-default confirmation. Close native
  choosers through their close callback, not hide(). Read the menu audit.
- Rendering acquires immutable snapshots. Geometry extraction, UV/paint transfer
  and file work belong on workers. Preserve generation/source-identity checks,
  exact cancellation/undo and ordered saves. Never silently discard paint.
- Retained normal domains serialize through their owning Scene sampling archive.
  For geometry-only comparisons, inspect the Mesh; do not encode an unowned
  PaintedMesh wrapper just to extract its mesh field.
- Keep upstream commits, notices and hashes in third_party/sources.json current.
  Shared desktop helpers are canonical qimgvr copies; do not fork their behavior
  casually. Preserve unrelated work in the other de-xr repositories.
- Inspect live processes/logs before restarting validation. Generated build and
  artifacts directories are ignored. Use appropriate native, GUI, compositor and
  sanitizer checks, and distinguish actual device tests from synthetic fixtures.

Build (prepare the current canonical DomeD toolkit as in [docs/TOOLKIT.md](docs/TOOLKIT.md) first):

```sh
export PKG_CONFIG_PATH="/opt/fltk-vr/bdf-runtime/lib/pkgconfig:/usr/lib64/pkgconfig"
export LD_LIBRARY_PATH="/opt/fltk-vr/toolkit/lib:/opt/fltk-vr/bdf-runtime/lib:/usr/lib64"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=icpx -DDOME_COMPUTE_BACKEND=ONEAPI \
  -DFLTK_DIR="/opt/fltk-vr/toolkit/share/fltk" \
  '-U*DOME_BITMAP_RUNTIME*' '-U*NATIVEGL*' '-U*RENDERFONTS*'
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

Application validation requires a GPU for the configured compute backend and
hardware OpenGL. Use a headless, explicitly opted-in CPU reference build for
numerical oracle and native core testing.

The user authorized committing and pushing checkpoints to the public repository.
Keep the handoff and validation notes accurate when recording one.
