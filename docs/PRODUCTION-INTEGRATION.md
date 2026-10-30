# Current integration candidate

The production objective and additional requirements remain active. This candidate
extends the existing FLTK/OpenXR editor. It combines generic Document transactions,
procedural sculpting, static source colors, conservative paged picking, resolved
VRM material roles, VRoid-only creation, Scene 11 authored binds/original source
receipts, native FBX/OBJ capture, and the source-preserving authoring workflow.

## New behavior

- Humanoid creation offers only verified CC0 VRoid female/male bases. MakeHuman
  default geometry and its converter are removed; the legacy command is disabled.
  Existing authored legacy projects and their metadata still load and export.
- The native C ufbx adapter captures original OBJ/FBX files and resource bundles
  immutably, separately from canonical GLB conversion. Static geometry, instances,
  corner attributes, hierarchy and supported materials are preserved. Full affine
  nodes, native-format skins/blends and unsupported material/UV cases still refuse
  explicitly. The options UI and large millimetre-coordinate normalization are
  separate work in progress; this candidate is not full FBX/OBJ completion.
- Exact authored glTF rest/bind matrices and skin instances are represented in
  Scene 11. Unsupported source poses/transforms remain guarded. This is not a claim
  of complete glTF or VRM interoperability.
- Newly prepared sculpt tangents and generated UV atlases handle the concrete
  failures found in a full import/remesh/detail/bake/paint/UV/save/export journey.
  Partial/backface projection is explicit and defaults off. Missed bake texels keep
  every previous baked channel; completely missed projections still refuse.

The user also requires Wings3D-style polygon editing and mesh visibility/held-mesh
isolation. Those interactive workflows are separate agent branches, not implemented
by this integration alone. General colored-source baking and the other uncompleted
original requirements remain required.

## Qualification so far

The candidate uses a regular-user optimized Release build against the prepared
DomeD FLTK 1.4.5+22 toolkit from commit 282467e. The DomeD remote was rechecked and
still points to 282467e727df78c8e7f2452aa05a7747bac854a6. No active session was
restarted, no system installation was performed and no new headset test is claimed.

- CPU native suite: 146/146 passed in the final full run.
- Whole C/C++ core, xatlas, both Mikk builds and ufbx instrumented with ASan/UBSan:
  35 selected suites passed with leak detection. This is not all-suite sanitizer
  qualification.
- Actual FLTK workflows: 51 checks passed: bake 7+4 compact; static import 8;
  authored-skin import 8; VRoid-only creation 11+13 compact.
- Synthetic real-loader OpenXR: all four suites pass (150 original, 17 resolution,
  11 procedural, 11 projection-policy checks), with actual GL eye images and image
  ownership checks. This is not physical-device evidence.
  The OneAPI build also passes all four suites on actual Arc compute; eye rendering
  in these synthetic fixtures uses software GL, distinct from the hardware batch.
- Eight direct graphics fixtures pass on Intel Arc A770: retained frames,
  independent glTF/VRM consumers, source colors, buffer reuse, resolved VRM roles
  and independently rendered authored/implicit skin binds.
- The initial full OneAPI run passes 145 of 147 suites. Two global-allocation fault
  fixtures are being investigated: first-use runtime allocation counts vary, and
  persistent allocation blackout reaches an unchecked null allocation in Intel's
  Level Zero driver. GDB identifies NEO CommandContainer::fillReusableAllocationLists;
  a single failed allocation instead produces a typed UR out-of-memory error.
  The native-worker fixture now warms first-use global JIT state before counting
  comparable workers; 59 actual failures pass on CPU (601 allocations) and OneAPI
  (702 allocations), with cold ray-index construction and all rollback assertions
  retained. The paged-device fault model is still being qualified. Do not claim full
  OneAPI qualification until that work passes.

Retained integration failures include the removed MakeHuman fixture's misspelled
helper (`spatial_command` instead of the public `spatial_action`), the old blanket
textured-OBJ rejection expectation (now a precise missing-UV refusal plus a positive
textured import/source-bundle/pixel/Undo test), and two import GUI invocations with
incorrect fixture expectations. Final authored-skin GUI parameters are
`DOME_IMPORT_HELPER=dome-sculpt-import-skin-test`, `DOME_IMPORT_GROUPS=6`,
`DOME_IMPORT_VERSION=11`. Initial failures and final logs remain under artifacts.

The vendored ufbx C source, header and license were byte-checked against the exact
pinned upstream commit. Third-party notices and source hashes include that evidence.
All consumers were rebuilt against matching public headers and archives.
