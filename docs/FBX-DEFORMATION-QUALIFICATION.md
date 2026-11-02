# Bounded native FBX deformation authoring

This extends the existing native importer and FLTK frontend. It does not complete
the full FBX requirement. Runtime880ec05 is the3a55c84 adapter on published
main7a4443e; registered native fixtures/data are frozen at d95e29e. Qualification
tree: `artifacts/fbx-authoring-3a55c84`. No new Scene schema is introduced.

The implemented subset converts linear/rigid skin clusters and independent
blend targets, retaining original inverse binds, source/cluster/channel IDs,
unweighted geometry rules, Blender shape offset weights and original files.
Actual source/reference fixtures qualify the linear and partially unweighted
skin subset and weighted/default morphs. New captures identify adapter3.
Existing adapter2 projects retain their saved data. The opaque legacy Lambert
case is distinguished from actual unsupported coloured transmission.

## Evidence

Fresh optimized Release and separate owned C/C++ ASan+UBSan core/support/render/
frontend/selected-fixture builds complete. System dependencies are not rebuilt
with sanitizers. New registered native2/2 pass0.17s; selected Release11/11 and
selected ownedSAN11/11 (9.39s) also pass. These are scoped runs, not a full-suite
or accelerator qualification.

The deformation fixture checks9073 assertions and1152 independent world
vertices: five genuine pinned ufbx inputs, nine newly edited translation,
rotation and anisotropic-scale poses, exact Save/restart and14 independent
cgltf consumers. Every posed case must produce measurable motion. Consumers
evaluate actual skin/morph/world data across ordered triangle corners,
preserving legitimate seam reindexing with the original fixed numerical bound.
All14 emitted GLBs pass pinned Khronos2.0.0-dev.3.10 with0errors and12unfiltered
warnings (`maxIssues:0`).

The real Document fixture passes51 source-read-barrier checks: accepted import
Save queues without replacing the prior file, commits exact source/deformation
data with one Undo/Redo, preserves old bytes/history on Cancel, rejects stale
generation completion, retries on the same worker and retains original inputs.

Actual native chooser/options/Save/Undo/Redo/Close passes14 checks for Maya skin
and14 for Blender morphs, in each of Release and ownedSAN: four distinct final
reports. Both use the real millimetre input-unit control and verify the saved
receipt. Every instrumented process exits normally with leak detection enabled.
No physical headset, new FBX device renderer or compositor result is inferred.

Initial failed probes are retained. The first GLB consumer assumed vertex index
identity across normal-frame seam export; the corrected oracle uses exact
ordered triangle corners, with no tolerance change. The old generic GUI Tab
sequence selected the axes dropdown rather than units; the corrected observed
control and persisted options independently confirm the intended choice. A GUI
fixture label initially dispatched the skin sanitizer case to its morph check;
the final exact script and four separate reports pass.

## Reproduce the optional native GUI

The native fixture/consumer suites are registered CTest cases. The optional
GUI driver requires the same dbus-next/python-xlib/Pillow environment as the
other repository GUI checks. Build `dome-sculpt-import-authoring-test` with the
same headers/core as the application. For the skin case:

```sh
DOME_IMPORT_FORMAT=fbx DOME_IMPORT_OBJECTS=2 DOME_IMPORT_GROUPS=7 \
DOME_IMPORT_MESHES=1 DOME_IMPORT_VERSION=13 DOME_FBX_GUI_KIND=skin \
python tests/fbx-import-menu.py
```

For morphs, select `DOME_FBX_GUI_KIND=morph`,
`DOME_FBX_GUI_INPUT=blender_279_shape_weights_7400_binary.fbx`, groups3 and
version11. `DOME_SCULPT_TEST_BUILD` selects the coherent sanitizer build. Use
distinct kind suffixes/output directories for each run. The canonical bitmap
toolkit/runtime prefixes remain required, as in TOOLKIT.md.

Read `validation/FBX-DEFORMATION-QUALIFICATION.json` for exact source, binary,
script, log, JUnit and input hashes. DQ/multiple-deformer semantics, named UVs,
full materials, generated target-normal options, independent combined unit/axis
deformation/attribute oracles and broader production/device workflows remain
required. The full goal is active; controller trays stay after basic targets.
