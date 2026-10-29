# Layered painting, XR controls and DomeD integration — 2026-10-29

This checkpoint adds stable UV painting assets and sparse editable paint layers,
controller-oriented parameter controls and concrete DomeD convention fixes.
The full production objective remains active in [the requirements](REQUIREMENTS.md).

- All nine optimized CTest suites passed: controller controls, texture painting,
  scene authoring, shared document, sculpt geometry, independent glTF/PNG
  interoperability, session appearance, real GL rendering and OpenXR client.
  A final four-suite run covers the updated controller buttons, version-two
  migration fixture and complete slider framing. A further GL run verifies
  visible aim-ray/crosshair feedback above the slider panel.
- All 32 synthetic OpenXR checks passed through the real loader and GL
  swapchains. The added cases paint and save a blue texture layer, embed its PNG
  in GLB, adjust a material through continuous stick and aim-ray input, and
  verify slider interaction leaves source geometry unchanged.
- All 13 private-Xvfb FLTK checks passed, including opening the native brush
  editor, painting a visible blue patch and restoring exact viewport pixels
  with undo. All ten private DBusMenu-host checks passed for real exported
  actions, radio/enabled states, local-bar handoff, Ctrl+W dialogs and the
  Cancel-default confirmation. The GUI fixture now isolates its Fontconfig
  cache and allows a longer startup deadline under concurrent test load.
- Painting tests cover pressure footprints, independent channels/coverage,
  layer visibility/masks, exact stroke undo/redo/cancel, immutable snapshots,
  persistent mesh/UV/layer round trips, independent duplicate edits and malformed
  input. cgltf parses the exported scene and libpng independently decodes its
  embedded maps, checking UV indices, PNG rows/bytes, material texture bindings
  and factors.
- Controller tests cover continuous preview, scalar fine-wheel rate, ray drag,
  secondary stylus interaction, RGBA row isolation, logarithmic radius,
  single-step persistent undo, focus cancellation and ray-clickable Cancel/Done.
  Trigger capture prevents sculpting behind a control panel.
- Appearance checks cover live standalone XR BDF face/size/2× scale, theme,
  accent, 25/50/75/100% chrome presets and recovery from partial preference
  writes. GL checks cover exact #303030/#d0d0d0 backgrounds, transparent overlay
  space, moving sliders, visible pointer feedback and 25% panel alpha.
- Runtime linkage uses the patched FLTK libraries with no Qt, GTK or Pango.
  Vendored checksums pass; canonical qimgvr appearance/menu helpers are unchanged.
  Blender ray picking provenance and the system libpng dependency are recorded
  in the [source manifest](../third_party/sources.json).

All six native suites passed ASan, UBSan and leak checks in the final run
(279.54 seconds). The initial concurrent sanitizer run exceeded the 60-second asset
wait in two fixtures; it did not report an address/undefined/leak error. Core
asset fixtures now allow 180 seconds for instrumented extraction/atlas work,
while independently checking that render-snapshot reads remain responsive.
The application still prepares meshes asynchronously.

Reproduce the native instrumentation checks with:

```sh
cmake -S . -B build-sanitize -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DDOME_BUILD_UI=OFF -DDOME_BUILD_XR=OFF -DDOME_COMPUTE_BACKEND=CPU \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build-sanitize --parallel 2
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build-sanitize --output-on-failure
```

Machine-readable evidence is in `artifacts/validation-paint.json`; log paths
and tested binary hashes are recorded there. These generated artifacts are
excluded from source control. Physical headset/controller/stylus behavior,
production-scale performance and CUDA/HIP/SYCL execution are unverified. Baking,
remeshing, manual UV editing, humanoid rigging, weights, shapekeys, full MToon,
full glTF/VRM and the remaining plugin-equivalent workflows are unfinished.

# Scene and material integration checkpoint — 2026-10-29

The persistent scene supports independent sculpt objects, uniform transforms,
visibility, edit locks and per-object PBR material channels in FLTK and XR.

- All six optimized CTest suites passed: scene authoring, shared document,
  sculpt geometry, independent glTF interoperability, GPU rendering and the
  OpenXR client. The scene suite verifies object identities, version-one
  migration, transformed sculpt isolation, metadata/stroke history, exact
  persistence, saved-state markers and malformed-input rejection.
- All 26 synthetic OpenXR checks passed, including creating an independent
  sculpt and changing its material through joystick submenu actions.
- All ten private-Xvfb FLTK checks passed, including object duplication/undo,
  the native object/material inspector, applying a blue material and undoing it.
  Captures include `artifacts/gui/08-material-editor.png` and
  `artifacts/gui/09-material-applied.png`.
- The real EGL/GL rendering fixture displays separate red and blue sculpt
  objects. Independent cgltf validation checks separate nodes/meshes/materials,
  exact local geometry, node transforms and PBR values including emission,
  opacity and double-sided flags.
- ASan/UBSan/leak checks passed for all four native core suites. The final
  scene and document suites were rerun after ordered save-queue changes.
- The save regression queues export and an older asynchronous save ahead of a
  newer desktop save, then verifies the newest geometry remains on disk after
  every worker drains. Save-failure/retry and exact cancellation checks pass.

Machine-readable results are in `artifacts/validation-scene.json`. Full native,
GUI and sanitizer logs are named there. These generated artifacts are excluded
from source control. The full objective remains active; production authoring,
painting, rigging, VRM, hardware and accelerator requirements remain unfinished.

# Native XR integration checkpoint — 2026-10-29

The OpenBrush DomeD basis is pinned at
`1373d5e839a9720adaeb187064785f9d9d0e8492`; the Blender basis remains 5.4 Alpha
at `3a4070ad41ff715f9c2e1a27f1003fbd01b80cd2`. Source roles and checked hashes
are recorded in [the provenance manifest](../third_party/sources.json).

- Optimized CPU build: all five CTest suites passed: shared document,
  sculpt geometry, independent cgltf interoperability, EGL/OpenGL rendering
  and native OpenXR client integration.
- The native synthetic runtime uses the real OpenXR loader and real GL
  swapchain textures. All 23 client checks passed, covering tracked controller
  and MX Ink tip-pressure edits, stereo eye rendering, focus/tracking loss,
  scheduled reference-space rebases, joystick menus, background save/export,
  and explicit primary-session selection when overlay support is absent.
- All five private-Xvfb FLTK checks passed: shaded surface, carve, exact undo
  pixel restoration, default trackball orbit and view reset.
- ASan, UBSan and leak checks passed for all three native core suites.
  The shared-document suite was rerun successfully after the final recenter
  and worker-notification changes.
- The GPU rendering test checked PBR/toon output, light/dark grey backgrounds,
  a transparent XR overlay, and visible BDF menu selection. Rendering used
  software OpenGL; it does not validate CUDA, HIP or SYCL execution.
- A bounded installed-Monado run exited successfully after 12 frames with
  11 stereo submissions. Monado reported a simulated HMD and no controllers.
  Physical headset, controller and stylus behavior remains unverified.
- Runtime linkage inspection found the patched FLTK frontend and no
  Qt, GTK or Pango dependencies. No system installation was performed.

Local logs and captures are generated under `artifacts/`, with machine-readable
results in `artifacts/validation-xr.json`, `artifacts/gui/report.json` and
`build/xr-client/report.json`. These generated files are intentionally excluded
from source control. Reproduction commands are in [the README](../README.md).

This checkpoint establishes a connected native editing path. The full
production objective remains active; incomplete authoring, scene, material,
rigging, export and hardware requirements remain in
[the completion matrix](REQUIREMENTS.md).
