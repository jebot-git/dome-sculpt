# Dome Sculpt progress assessment — 2026-11-15

The project has a substantial native FLTK/OpenXR authoring foundation and a
qualified integrated baseline. It remains an incomplete production suite. The
current round ends at the successful public-test compilation checkpoint; work
is paused at the user's request. No percentage is assigned because requirement
rows have substantially different scope and many have only partial evidence.

## Completed and qualified at this stage

- Main integrates the source-aware baking, HDR/native-field and mapped-humanoid
  baseline. Release and strict sanitizer native suites each pass 87 tests;
  scoped public desktop checks total 208 and synthetic XR checks total 776.
  Software and authenticated Intel Arc rendering are qualified separately.
  See [baseline admission](GUARD87-RUNTIME-ADMISSION-20261115.md).
- Existing functionality includes sculpt layers/masks/symmetry, polygon editing,
  editable UVs, layered paint/projection, rig/weight and morph tools, native
  import/export subsets, ordered saves, recovery and exact edit history.
  Coverage and limitations remain in the full requirement matrix.
- Controller selection/multiselection, hierarchy, hiding and translucent
  unselected objects remain the user-accepted interface baseline. Only verified
  CC0 VRoid female/male creator bases are offered; MakeHuman defaults are removed.
- The successor artist/prepared-normal baking component passes native and
  sanitizer checks plus 24 graphical cases in each software/Arc profile.
  See [normal-source qualification](ARTIST-NORMAL-GRAPHICS-20261115.md).
- A fresh application using that successor component links, loads and passes
  the existing 52-check public GUI regression. This is component-level evidence;
  it does not establish the new prepared-source public workflow or replace the
  complete combined build. See [GUI result](APP1BC-SOURCE8-GUI52-PASSED-20261115.md).
- New prepared-source public desktop/XR test sources compile successfully:
  four complete translation units, genuine compiler dependency files, zero
  recorded input drift, successful service exit and no surviving owned children.
  These new tests have not linked or executed.

## Immediate integration work on resume

1. Link and run the new prepared-source fixtures; verify actual desktop/XR
   source selection, Bake, immediate Save, reopen and exact Undo/Redo.
2. Merge the qualified successor and tests into one frozen source revision,
   complete matching optimized Release and sanitizer builds, and run the
   relevant native, GUI, rendering and synthetic-XR regressions. The planned
   combined build registers 90 native tests; it has not been completed.
3. Validate the combined candidate through native Wayland/DomeD and then physical
   headset/controller workflows. Recent Xvfb GUI and synthetic-XR evidence do not
   constitute those device tests. Installation follows a qualified candidate.

## Main remaining production goals

- Sustained production-size sculpting/local meshing, allocation/recovery
  hardening, topology/paint preservation, robust remeshing and retopology.
- Complete texture sets, advanced layered painting and broader normal/AO/material
  baking, source/UV/deformation workflows and GPU execution where viable.
- Robust humanoid proportions/face/hair authoring, imported-rig rest/reparent
  handling, production automatic weights and broad deformation coverage.
- Complete required glTF/VRM interoperability, including remaining expressions,
  physics, constraints, metadata and source-preserving round trips.
- Finish all authoring workflows entirely in XR, then verify physical controller,
  stylus, trackball/3D-mouse, reconnect, session-loss and sustained performance.
- Extend accelerated workloads and backend validation. Intel Arc/OneAPI has real
  positive execution evidence; CUDA/ROCm compilation and explicit missing-device
  behavior do not establish positive NVIDIA/AMD hardware acceptance.
- Implement controller-mounted tool/function/selection trays and spatial
  node-based editing/history after the basic authoring targets are met.

ArmorPaint has a source-based integration assessment, not a hosted painting
engine integration. Reusable GPU tools remain worth prototyping behind the
existing native document/UI/resource contracts; see the
[assessment](ARMORPAINT-ASSESSMENT-20261111.md).

The [full requirement matrix](REQUIREMENTS.md) remains authoritative. This
checkpoint adds no system installation, physical-headset acceptance or session
restart. Old failed runs remain preserved; passing subsets do not complete the
goal. Resume from this assessment and the recorded held plans, without restarting
finished jobs or treating compilation-only results as runtime evidence.
