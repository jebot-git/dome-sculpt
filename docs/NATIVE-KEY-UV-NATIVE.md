# Native keyed-paint UV desktop and controller consumers

The qualified native36 post-key UV runtime1c5/1ae now has actual FLTK normal and
compact controls plus OpenXR-loader synthetic controller evidence. The product
source, public/private headers, core/render/support and application bytes are
unchanged. New matched inspector/runtime auxiliaries compile from the same
complete appended-layout parent. Only portable Python consumers and one CTest
registration are added here.

The exact final desktop harness passes16 normal +16 compact checks in each of
Release and wholly owned sanitizer profiles. It verifies required retained mode,
Ctrl+W Cancel, actual Scale/Apply, complete editable values/current frames/keys/
rig/shape sources, oneUndo/Redo, ordered Save, graceful Close and restart. It
resolves complete domain/basis and Paint records rather than comparing unrelated
pool indices, preserves shared alias patterns, and independently rejects a
changed current-domain reference even when its basis/corner values are equal.
The initial raw-index fixture failure remains separate evidence.

The portable real-loader consumer passes23 checks per profile: synthetic
controller Accept changes the actual chart and retains all grids/current roles/
posed model/source values, Cancel and real focus loss discard the whole draft,
Save drains normally, and real left/right swapchains render the palette with
balanced acquire/wait/release. Both exact private887 Mesa driver and EGL files
are observed in all three sanitizer XR client maps; no sanitizer suppression or
ASan/UBSan/LSan diagnostic occurs. Five R/S saved output pairs are byte-identical.
This is synthetic tracking with actual loader/application/software GL, not a
physical headset or new accelerator claim.

Read [the receipt](validation/NATIVE-KEY-UV-NATIVE.json) for exact parent support,
source/config/binary hashes, actual maps, all reports and preserved scope limits.
The canonical-key raster/retained Scene37 work, complete new36 factory/codec fault
coverage, inverse-row UV-source Restore, post-UV emitted model quality and full
semantic/main admission remain separate required work.

Reproduce the stdlib-only loader workflow after building the registered producer:

```
ctest --test-dir build -R '^native-key-uv-openxr$' --output-on-failure
```

For actual desktop controls, provide the matching app/inspector directory and the
real `native-key-uv` producer's `before.dsculpt`, with canonical SDK runtime paths:

```
DOME_PAINT_BUILD="$PWD/build" DOME_NATIVE_KEY_UV_FIXTURE="$PWD/build/native-key-uv/before.dsculpt" \
  uv run --offline --with python-xlib --with pillow --with dbus-next python tests/native-key-uv-menu.py
```

Add `DOME_NATIVE_KEY_UV_COMPACT=1 DOME_PAINT_SCREEN=640x360` for compact controls.
Use a private Xvfb launcher where the host server/toolchain has been retired.
