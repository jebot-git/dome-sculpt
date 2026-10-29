# Dome Sculpt agent entry point

Read [docs/HANDOFF.md](docs/HANDOFF.md) first for the latest checkpoint, known
failure and validation results. Preserve the full objective in
[docs/REQUIREMENTS.md](docs/REQUIREMENTS.md); this checkpoint is not completion.

- Runtime: native C++20, FLTK frontends and direct EGL/OpenXR. Python is optional
  test orchestration only. Do not introduce Qt/GTK/Pango/Erlang runtime frontends.
- Fonts: BDF through the shared Fontconfig/FreeType helpers. Preserve DomeD
  theme, accent, bitmap size/scale, opacity and background conventions.
- Universal menus: actual commands, context-aware enabled/check/radio states,
  modal text/UV/document history routing, shell-owned application/Window menus,
  Ctrl+W/Escape cancellation and Cancel-default confirmation. Close native
  choosers through their close callback, not hide(). Read the menu audit.
- Rendering acquires immutable snapshots. Geometry extraction, UV/paint transfer
  and file work belong on workers. Preserve generation/source-identity checks,
  exact cancellation/undo and ordered saves. Never silently discard paint.
- Keep upstream commits, notices and hashes in third_party/sources.json current.
  Shared desktop helpers are canonical qimgvr copies; do not fork their behavior
  casually. Preserve unrelated work in the other de-xr repositories.
- Inspect live processes/logs before restarting validation. Generated build and
  artifacts directories are ignored. Use appropriate native, GUI, compositor and
  sanitizer checks, and distinguish actual device tests from synthetic fixtures.

Build:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DDOME_COMPUTE_BACKEND=CPU -DFLTK_DIR=/opt/fltk-vr/toolkit/share/fltk
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

The user authorized committing and pushing checkpoints to the public repository.
Keep the handoff and validation notes accurate when recording one.
