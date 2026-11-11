# Current preview public workflow qualification plan

This is a test-only extension on composed source `ccb2b7b5`, following the user's
explicit resumption. No application or GPU case has run for this extension yet.
The existing 775 and 27f evidence and all original scenarios retain their scope.

The previous blur fixture uses an imported source model, retained appearance and
material bindings, and genuine paired source-color preparation. Those inputs are
explicitly outside `CurrentGridPaintPreview::eligible` in
`src/current-paint-preview.cpp`. Repeating that workflow alone cannot qualify the
new plain current-grid preview. The texture-set workspace producer is different:
`tests/texture-set-resolution.cpp:47` initializes genuine plain64/128 texture-set
fields, with mask/hidden/physical/tiny-FLOAT authoring. These meet the new
`eligible_field` conditions and use `for_field` through the texture-set worker.
Its existing81 scenarios are therefore useful current texture-set UI coverage
when rerun with fresh producers/app/inspector, although they do not independently
assert packet identity. They do not replace the ordinary single-actor
`for_render` fixture below, and their earlier passes do not qualify this new ABI.

The separate native producer option `--plain-workflow-fixtures OUTPUT` creates a
genuine strict-UV64 `PaintedMesh` without imported/projected/baked/polygon or
retained-domain authority. Actual native stamps create nonuniform color,
authored alpha distinct from coverage, metallic/roughness, independent mask,
physical FLOAT emission with tiny coverage, and a hidden zero-opacity layer.
It uses the same public default Flow1 Blur command and existing independent
per-texel oracle. Exact whole-scene Undo/Redo/Save/reopen remains required.

Offline fixture construction additionally checks that the actual Document worker
publishes an authenticated current-grid FLOAT packet with no CPU image substitute
before Blur, after acceptance, after Undo/Redo, and after reopening both native
endpoints. Those assertions are native fixture checks, not an observer in the
application. The application drivers do not read private product state or use the
manifest's descriptive domain metadata to authorize progress.

The original source-backed `--workflow-fixtures` body/options and the public
desktop10/XR39 drivers and runtime are unchanged. Both manifests can drive those
same real public menu/highlight/checkbox/Save/history and native endpoint checks.
New runs must have separate namespaces for source-backed versus plain input and
freshly built current-union binaries. Proposed matched coverage is normal desktop
and real-loader XR in each profile for both domains; compact/old81/old117 retain
their earlier scope rather than being silently relabeled.

Required current owning targets per profile:

- `dome-sculpt`: fresh actual FLTK/XR app, renderer, core and native GPU providers.
- `dome-sculpt-native-authoring-inspect`: fresh native Scene/TextureMaps ABI.
- `dome-sculpt-gpu-color-blur-native-test`: fresh original producer/control target;
  the changed fixture-only TU can link against the exact frozen current core and
  helpers after the root build closes, preserving its `eglCreateContext` wrapper.
- `dome-sculpt-gpu-color-blur-font-reference`: current palette-text/session font
  providers and current canonical DomeD SDK/font reference.
- `dome-sculpt-gpu-color-blur-runtime`: current real-loader test DSO, independent
  source-font/NEAREST/title/checkbox and both-eye capture controls.

No old core, renderer or inspector ELF/archive qualifies the changed TextureMaps
ABI. A fixture-only TU relink is not a whole fresh application build. Exact source,
owning output and link-provider manifests must be checked before and after each
case, including the current app/runtime/inspector/font/native inputs and actual
mapped ELF/device identity. The external GL query observer may record genuine
EGLDevice/DRM/PCI/live FD and return the unchanged result; it must not manipulate
GL or UI state.

Preserve XR 6,000 frames/240 seconds, desktop50-second public waits/900-second
phase, all existing independent native oracles and strict SAN leak detection1/UB
settings without suppressions. Acquire the shared Arc lease only after the root
fresh builds are ready and explicit coordination. Capture real public failure
images and retain any original failures. No physical headset, desktop-session
restart, installation, performance or full painting-goal completion is implied.
