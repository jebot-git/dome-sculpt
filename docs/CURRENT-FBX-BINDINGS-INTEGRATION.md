# Current per-mesh FBX bindings integration

Public merge `870aa6c7c912d5f8eae1110b23ed890b496256d2` is runtime/header/asset/third-party/CMake/test
byte-identical to qualified `069ed9cdadea9127790b75e3a6203e34fda0612b` (runtime/test source8a89787).
It merges adapter4 with the current native C++20 FLTK/OpenXR authoring app.
A named texture UV set resolves on each actual source mesh, including one
shared material meaning UV1 and UV0 on different meshes; normal tangent basis
and canonical material variants follow that identity. Images/original resource
identity remain retained. Old saved captures are not rewritten. No public ABI
or persistent Scene change, and no admission of private Source21+ data.

Fresh coherent production core/support/render and owned C/C++frontend builds
are followed by test-only helper/fixture refinement, with exact production
archive/SANApp equality. Both optimized alltarget builds complete: CPU398 and
OneAPI399 cases register. Registered16/16 pass8.35s Release and16/16 pass18.45s
ownedSAN with leak detection. This does not claim a full398/399suite pass.
The earlier395 split result and corrected150loader proof stay separately scoped.

New actual named-UV source/native512atlas/CorePBR/authored restore/Save/restart/
GLB pixel proof passes37checks on software and strict actual PCI03 Intel Arc
A770. Maxpixelerror1 stays within unchanged4-bound; wrong-coordinate and wrong
tangent controls remain sensitive. Source/binary before/after manifests match.
New75genuine5inputs×5unit×3axis choices pass37097checks/5130independently evaluated
raw-unit skin/morph world corners, native persistence and parsed GLB evaluation.
Those are default poses; prior9073 edited-pose/consumer checks stay separate.

Actual FLTK Maya-skin14 and Blender-morph14 checks pass in BOTH Release and
ownedSAN with normal App/GL/font teardown. Current93positive GLBs pass pinned
Khronos2.0.0-dev.3.10/maxIssues0 with0errors/57unfilteredwarnings. These GUI cases
are genuine deformation inputs choosing Millimetres, not a named-UV chooser
fixture. Current-source OneAPI15/15 pass13.87s on actual A770 level_zero:0:
184analytic endpoints,224CPU-reference batches/445088samples,max8.9407e-08.
The explicit missingGPU case refuses with host samples unchanged. Current
sycl-ls independently confirms A770index0/UHDindex1. GL stayed software for
compute scope; actual Arc rendered images are separately qualified above.

The first graphics fixture crashed because its independent consumer assumed
an index accessor; canonical FBX correctly emits nonindexed triangles. Exact
helper-SAN null78, old binary, failed JUnit/log and test-only sequential-corner
fix remain preserved. Indexed cases also pass. No runtime fix was inferred
from that fixture defect. All123 recorded component evidence hashes were
rechecked intact, and main source equality verified before publication.

Read validation/CURRENT-FBX-BINDINGS-INTEGRATION.json and the component receipt
for exact build stages, binaries, logs, source and limits. Full FBX/GLTF/VRM
semantics, direct raw multi-UV authoring, private paint/rig/extra-UV work and
all other REQUIREMENTS rows remain unfinished. Controller spatial drawers
stay queued after basic targets. Latest DomeD remote main remainsd271cf6;
canonical toolkit/bitmap inputs, unrelated dirt/stash, private worktrees and
saved accepted XR session remain preserved. No installation, new physical
launch or active-session restart occurred.
