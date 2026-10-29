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
