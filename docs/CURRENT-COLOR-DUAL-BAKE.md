# Simultaneous current-corner source and target Bake

This test-only child of private `65b0ae9` exercises both adapters in the same
actual native worker operation. It makes no production or schema change. Source
and target have separate imported current-corner fields and immutable basis
owners; one has a zero/one-alpha seam while the other has continuous alpha.
The source remains hidden and locked, with the target independently editable.

Sixteen cases combine the two seam assignments, four alpha modes and saved
skin/morph poses. The fixture checks actual source pixels against the accepted
target, exact source preservation, the target's own PolygonCorners receipt and
retained PA Origin, one Undo/Redo, native Save/reopen, and native source Restore.
An independent cgltf/PNG/GLSL consumer reads the actual emitted GLB and checks
the rendered result without a duplicate raw COLOR_0 accessor. Preparation uses
the real native 256-resolution import option, matching the tighter image oracle
already qualified by the target consumer. Default64 quality remains governed
by its separately checked declared linear error bound.

The standalone build uses the exact frozen `65b0ae9` headers and corresponding
Release or instrumented core, renderer and C/C++ support archives read-only.
Only this new test translation unit is compiled in the child. Exact compilation
and link commands are retained under `artifacts/compile-dual*.json`. The added
CMake registration lets a subsequent integrated candidate rebuild the complete
consumer normally. This is scoped evidence for this exact product dependency,
not a whole latest-main integration or additional OneAPI claim.

Software, actual PCI03 Intel Arc A770 and the separately instrumented consumer
each pass185/185: source image maxerror1 and all exported GLB images exact.
The instrumented run links the exact already instrumented owned core, renderer
and C/C++ support archives, with ASan/UBSan and leak checking; no reports occur.
External system dependencies are not claimed instrumented. This is not a new
whole-project sanitizer rebuild or a full OneAPI/all-native proof.
The parent's extended51598-denial sanitizer sweep remains independently live;
this child neither changes nor substitutes for that sweep.

Unsupported prepared normal, artist-frame and active UV1 domains remain guarded.
Prepared current-field UV/topology transport still requires its real remap
adapter. Pending generic Bake Save remains explicitly refused; this fixture
tests Save after accepted completion. No installation, physical headset launch
or session restart is performed.
