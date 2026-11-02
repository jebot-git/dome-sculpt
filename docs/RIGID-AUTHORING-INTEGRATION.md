# Rigid source avatars in the retained-authoring application

This integration extends the qualified Scene19 authoring application with the
mixed rigid/source VRM0 component17b0ff7. Productd5de43b combines actual post-UV
prepared-key authoring, current-corner source Bake and original-space VRM0 export
with real unskinned accessories and durable linked-pose attachments. Public merge `9ccddef` has identical runtime/header/asset/third_party/CMake/test
bytes to qualified4f79dbb. No public
field/layout or archive version changes are introduced. Source-rest/reparent
migration, divergent skinned placements, VRM1/physics and the complete production
objective remain required. Private Scene20/21/23 work is excluded here.

An imported avatar's rigid parts retain their original source nodes and real
mesh/primitive instances. They never receive fabricated skin/joint attributes.
A linked native pose resolves original source joint identities across distinct
inverse-bind views, rejects unsynchronised/conflicting/locked states, and stores
the rigid world delta in existing NativeAffineEdit in the same Scene transaction.
Save/reopen, origin capture and deletion of a skin owner retain placement.
Export factors each rigid part under its original emitted source node and checks
complete node/mesh/primitive membership, including shared-mesh instances.
See VRM-RIGID-SOURCE.md for the component's algorithms and boundaries.

## Coherent integration qualification

All relative build/artifact evidence refers to the frozen workspace tree
`artifacts/rigid-authoring-union-9e6cb1b`. Its optimized all-target CPU-compute
FLTK/OpenXR build completed: the initial build terminated143 at96% without a
compiler error; process inspection confirmed no remaining build descendants,
then an incremental resume completed0 with unchanged source and flags. The
interrupted log is preserved. This is not a clean uninterrupted-build claim.

Sixty-seven selected Release suites pass438.93s, including new rigid/source,
old rig, generic import, post-UV key/actual avatar, source-color Bake, original
material/frame/expression consumers and persistent failure regressions. This
is a scoped integration run, not a new full321-suite run.

Fresh whole owned C/C++core/support/renderer sanitizers initially pass45/46 native
suites in799.26s; the large rigid source/metadata/corner oracle hits its240.21s
time limit. The same binary/assets/oracles directly pass all424,070 assertions
in214.11s under an explicit600s allowance. A test-only CMake budget correction
then passes the registered CTest1/1 in255.37s. Runtime/header/asset/third_party/test
bytes remain unchanged. Preserve45/46 initial plus corrected1/1; do not relabel
one46/46 run. The separate six graphical suites pass1012.74s and six persistent
allocation-failure suites pass1364.03s. External dependencies and an instrumented
FLTK/frontend are not claimed by these core/support/renderer checks.

Six actual Arc A770 batches pass404 assertions: source Bake133, post-UV keys52,
actual avatars87, skinned source VRM37, normalUV46 and mixed-rigid source VRM49.
The last49 checks/twelve images cover both actual CC0 avatars with three added
source parts, ordinary/reflected-mesh/sheared-source cases, current morph/pose
and independent rigid affine placement. Every hardware rigid image has0 error
at unchanged tolerance2. These are actual GPU OpenGL consumers, not OneAPI
compute qualification or physical headset evidence.

Actual native FLTK export chooser/Close/durable-Cancel/ordered-Save/retry passes7
with22 exported parts and an unskinned active badge (independently inspected
active23,23 Scene objects including the original empty sculpt object). Latest
private d271 DomeD native Wayland/universal-menu routing passes8. The canonical
FLTK/bitmap prefixes remain unchanged; no live desktop or system installation
was touched.

All105 emitted GLB/VRM files have0 Khronos errors and1,130 unfiltered/unlimited
warnings. All71 emitted VRM0 files pass15 official schemas pinned at
94e82dd346fa6cf0337c4421728640e5252dd38e, each Git-blob SHA1 checked. Raw source
variants retain their inherited advisory diagnostics and are excluded from
emitted-file claims. The initial schema launcher raced fixture-script generation
and exited before validation; that setup log is preserved separately and the
completed report is authoritative.

The full original goal and the newly requested controller drawers remain
incomplete. Drawers are a required phase after basic targets. The accepted
physical selection interface and its saved session remain preserved; no new
physical test, system installation or session restart occurred.
