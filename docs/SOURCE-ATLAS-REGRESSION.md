# Source-aware atlas admission and regression qualification

Imported meshes with unused degenerate source UVs remain viewable with their
original data. A paint attempt now explains that an editable atlas is required,
before starting a stroke or creating history. Explicit atlas preparation provides
writable coverage while keeping original image bytes, samplers and coordinates.
The change is the private admission predicate only: no public layout, native
format, numerical tolerance or source-ownership guard is weakened.

The original combined regression exposed five failures. Four expected the former
resampled-image/Scene18 path or attempted to create UV ownership by replacing
serialized maps. The degenerate-UV test additionally exposed the real empty-raster
stroke gap fixed by f89c4c7. Updated tests retain actual worker painting, queued
Save, native restart and exact Undo/Redo. The native worker correctly refuses a
new stroke until Undo/Redo surface publication completes; the fixture now waits
for that existing public boundary. Its before/after diagnostic is preserved.

The coordinate oracle checks 22,692 interior samples across repeat, mirror and
clamp, with 15,928 differences when writable-atlas coordinates are substituted
for actual source coordinates. This is an accessor/remap/sampler oracle; rendered
pixels are checked separately. The twelve graphical fixtures cover authored and
generated frames, OPAQUE/MASK/BLEND and two morph states. Native original/prepared
pixels are identical. Ordinary pristine GLB, explicit finite painted GLB,
independently parsed file pixels and exact native package recovery are exercised.
Only the painted instance needs finite compilation; its unpainted sibling keeps
original UV1. The nominal finite mean/interior bound remains2, and these fixtures
observe maximum1. Existing minification counterexamples remain valid; this is no
general lossless finite-export claim. The strict ordinary painted-export guard
remains in force.

The additional combined retained-paint/UV2 workflow rejects forged UV1/UV2 owners
at decode, then performs actual raw UV2 import, native UV preparation, projection,
Save/reopen, Undo/Redo and independent GLB rendering. Its wrong-UV control visibly
fails. Genuine current-corner plus Source4 UV1/UV2 authoring still needs its own
adapters; a forged-owner rejection does not implement that requirement.

Qualification is explicitly scoped:

- Release and owned ASan/UBSan each qualify16 selected workflows, composed of13
  actual unchanged passes and3 corrected complete-test-TU passes. Dependency
  files prove the reused13 do not include the three changed test sources.
- Complete paint.cpp/document.cpp and all actual private-header-dependent tests
  are rebuilt. Only those two product archive members differ; matching owned
  c092 support/renderer and unchanged core members are reused. This is not a
  fresh whole application or full589-test result.
- The two graphical workflows pass on strict PCI03 Intel Arc A770; two actual
  software runs with `--hardware` exit1. All2,231 hardware inputs stay exact.
- All82 actual emitted GLBs pass Khronos with0 errors,6 unfiltered
  NODE_SKINNED_MESH_NON_ROOT warnings and216 infos. Of54 corresponding S/Arc
  comparisons,50 are whole-file identical. Four mixed-fixture files differ only
  in their original PID-derived source node name; every corresponding BIN byte
  and all other JSON are identical. Files are not normalized or rewritten.

Read validation/SOURCE-ATLAS-REGRESSION.json for commands, hashes, preserved
negatives, exact build/reuse boundaries and limitations. The original frozen
combined regression remains untouched. This private milestone does not admit
other unfinished branches, complete the production goal, install software or
qualify a physical headset. Controller drawers remain required after basics.
