# Public GPU color blur workflows

The actual ordinary FLTK application passes four desktop cases (10 controls
per case) and two real OpenXR-loader cases (39 each) against unchanged 775
product sources. See [the qualification](GPU-COLOR-BLUR-PUBLIC-WORKFLOW-20261111.md)
and its exact [receipt](validation/GPU-COLOR-BLUR-PUBLIC-WORKFLOW.json).
These tests use synthetic poses and private Xvfb; they do not launch a headset,
install a build or establish native-Wayland/compositor or physical usability.

Generate fresh native inputs with:

```sh
dome-sculpt-gpu-color-blur-native-test --workflow-fixtures NATIVE_OUTPUT
dome-sculpt-gpu-color-blur-font-reference TITLE_REFERENCE.json
```

The first producer needs actual hardware and uses the public default Flow 1
command. Its nonuniform strictUV input retains genuine imported source/normal/
rig owners, paired PBR preparation, other channels, physical FLOAT emission,
mask and hidden layers. Independent pixel oracles and exact native history/
reopen checks produce immutable original/accepted projects. Color preparation
precedes later physical-emission authoring; supporting conversion on already
physical paint remains an explicit adapter limitation.

Run the public workflows with coherent app/inspector/runtime providers:

```sh
python3 tests/gpu-color-blur-menu.py APP INSPECTOR NATIVE_OUTPUT/manifest.json OUTPUT
python3 tests/gpu-color-blur-xr.py APP RUNTIME INSPECTOR NATIVE_OUTPUT/manifest.json TITLE_REFERENCE.json OUTPUT
```

The title/stencil reference generator performs no GL work. It uses the audited
native bitmap layout/session appearance, exact source 640 px placement and actual
eye projection. ROOT9 must first match the literal source-font pattern and pose.
Subsequent page titles and observable checked/unchecked/absent markers match
complete expected GL_NEAREST patterns in both eyes. No observed pixel minima,
private app state, callbacks or product success markers authorize progress.

The original GUI 81/XR 117 scripts remain unchanged and retain their own scope.
New GUI waits remain 50 seconds/900 seconds per case; XR retains 6000 physical
frames/240 seconds. Actual whole native Save, one Undo/Redo, fresh reopen,
public menu/history/palette acknowledgments and frame/image/session ownership
are authoritative. Hold the shared Arc lease for hardware execution and preserve
strict unsuppressed SAN settings. Optional `DOME_BLUR_APP_PRELOAD` applies only
external device measurement to the app/inspector, leaving Xvfb/Python untouched;
measurement reports never drive the UI state machine.

The dated qualification retains original pre-EGL fixture failures, the failed
coordinator assertion, the initial 6000-frame title alias failure and the separate
48-frame capture-only diagnostic. Original counts, limits and product guards
were retained; source-derived shape recognition replaced unjustified new coarse
pixel assumptions. The complete painting/material/baking and resident-cache/
source-domain goals remain open.
