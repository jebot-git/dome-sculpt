# Affine polygon workspace draft

This isolated child of main10944ab now has a scoped matching Release/owned
ASan/UBSan runtime milestone263c753. Twelve own-header syntax checks also pass
(including all seven affected product TUs, new consumer and older polygon/render/
framing consumers). This is not full-build, all-regression, graphical or main
admission; those scopes remain explicitly separate.

The local polygon authority and its immutable history remain local. Real affine
placement is used independently for world-metre operations and pointer metrics:

- Extrude/Inset run on a detached linear-world geometry, with reflection winding
  corrected in both directions. Results map through the actual inverse. Untouched
  existing vertex positions remain bit exact; stable vertex/corner/edge IDs, UVs,
  hard flags and original corner-color provenance are retained. No atlas or paint
  resampling, similarity fit, nearest projection or identity placement is used.
- Affine picks use watertight double local ray expressions and actual world-space
  vertex/edge distances. They do not inherit the old float ray's local distance
  ceiling or rebuild a transformed BVH on each sample. Work/cancellation limits
  still apply. Drag planes are local covectors (A-transpose of the world plane
  normal); total pointer displacement maps through the real inverse. XR adds the
  actual canvas frame and held-object edits use the current full affine matrix.
- Apply captures the current held NativeAffineEdit in the accepted token. It
  publishes that placement and topology together, retaining one Undo. Origin
  capture uses the complete initial Scene's placement capsule; current placement
  is independent of original mesh/paint/material/deformation identity checks.
- The private preview and camera-framing path use ObjectInfo.affine_world when
  present instead of hiding the workspace. The polygon shader uses inverse-
  transpose normals and correct reflected front-face convention, restoring
  caller GL state. Geometry upload/immutable motion semantics remain unchanged.

The combined new consumer has basic native, controller and kernel-fault modes.
It contains independent physical Extrude/Inset endpoint/radius checks, corner
color owner controls, actual native selection/drag/Cancel/Apply/queuedSave/
restart/Undo/Redo/Restore and held-affine publication. The fault mode measures
all C++ allocation prefixes and callback cancellation sites of the new detached
world-metric kernel. It does not claim all Document/file/worker OOM paths.
Graphical fidelity, actual FLTK normal/compact UI and application synthetic-XR
remain required after the initial bounded cohort. Existing parent remesh R/S
SelectAll failures remain preserved in the separate root tree.

## Exact affected object closure

No public data member, Scene codec or persistent record changes. PolygonMesh
adds only a friend; PolygonWorkspace adds methods; private preview fields change.
Document's actual fields remain unchanged. Own-header constant-assembly probes
show Document6968/align8, EditorState1896 and Scene72 unchanged; private PolygonJob
changes96->232 and PolygonAccept344->360. Size equality is not the sole proof.

Source search plus definitions AND undefined symbols in complete immutable R/S
C179 core/render/XR/polygon archives and all26 frontend objects/profile find
nested job/token symbols only in document.cpp.o and document-polygon.cpp.o.
Transaction/authority helpers use shared pointers/views, not changed nested
object offsets. Actual changed private include consumers add the other five TUs:

1. document.cpp (constructor/destructor/deque elements and local End/Undo jobs)
2. document-polygon.cpp (jobs, accepted token, worker and geometry publication)
3. polygon-authoring.cpp
4. polygon-conversion.cpp
5. polygon-render.cpp
6. app.cpp (preview/framing private inline helpers)
7. xr.cpp (preview private inline helper)

All seven must be compiled whole. Both new app/consumer include current own
headers. Unchanged objects are reused only with exact source/body/header layout,
symbol and link-trace proof; no changed old archive member may be selected.
Main10944 and C179 have byte-identical src/include content before this patch.
No static archive is copied/repacked or modified.

## Authorized first cohort only

Root approved cumulative1.25GiB allocated bytes for sequential Release then owned
O1 ASan/UBSan app plus one combined consumer, with at least20GiB actual free and
prospective output checks before every compiler/linker. No further target is
approved until actual growth/outcome review. Existing matched member payloads
are2.67MB Release and72.02MB sanitizer for these seven TUs; matched existing app
is14.1MB/410.4MB. This replaces the rejected broad5GiB proposal.

The runner is tests/polygon-affine-proof.py. It overrides whole objects before
immutable currentmain10944/C179 support, pins archive/frontend/runtime inputs,
captures dependencies/link traces and actual ELF maps, and refuses old member
selection or drift. No archive copies, full build, GPU work, installation,
headset/session action or new Scene38 reserve is authorized by this cohort.

## First Release runtime advance and preserved capability negative

7849cb5 first Release cohort82003 completed compile/link/app-version and all160
basic controls with no source/input drift. The controller case failed at actual
workspace entry: publish_locked still excluded every affine object from
can_polygon, so shared XR/FLTK commands remained disabled despite the proven
real direct-native adapter. This initial binary/status/log remains untouched in
artifacts/affine-proof/release. Its fault mode was not reached; no sanitizer or
graphical admission follows this partial result.

The next child removes only that superseded affine capability exclusion,
retaining all modal/source/editability/factory guards, and asserts actual
can_polygon before controller entry. Tagged proof directories preserve earlier
attempts. Whole-TU closure remains unchanged.

## Closed bounded phase1

Tagged Release handle7849 and owned sanitizer handle34005 both completed0 on
frozen263c753, with exact source/support/runtime-map hashes before/after. Each
profile passes160 basic native checks,32 actual-controller checks and9265
detached-kernel failure assertions (all3283 measured persistent C++ prefixes with
retry plus all2699 callbackCancel sites). There are no sanitizer diagnostics.
The real sheared/reflected native worker Save/reopen outputs are53117 bytes each
and R/S byte-identical. App--version also exits0; it is not GUI execution.

All seven whole changed/affected product TUs were compiled/profile; remaining
current-main/C179 objects and complete owned supporting archives were reused
with exact source/body/layout/symbol/link-trace closure. No archive was copied or
modified, and no old replaced member was selected. This is an incremental matched
qualification, not a claim of a fresh whole-source app/core build.

Cumulative allocated bytes at closure are approximately1,044,189,184 (996MiB),
below the approved1.25GiB cap. Actualfree is approximately22.78GiB at that sample;
no extension/new target has started. Independent preview pixels, real FLTK
normal/compact and actual app-loader workflows, affine posed/colored/imported
combinations and full native worker/queuedCancel OOM remain required. Existing
app binaries can support a later bounded GUI cohort; further consumer links
require root review. POLYGON-AFFINE-PHASE1.json contains exact per-profile receipts.

The software followup at `docs/validation/POLYGON-AFFINE-FOLLOWUP.json` closes
231 independent preview checks per profile (36 image pairs, max0 with unchanged
limit1), and78 real FLTK workflow checks per profile across ordinary and compact
native/sheared and posed corner-color/reflected projects. Apply/immediate Save,
local and global Undo/Redo, Cancel and complete Restore are checked by real file
worker output. The actual sanitizer application exits cleanly with leak checking.
The standalone sanitizer consumer compiles complete production workspace/render
TUs with function/data sections and linker GC; it does not substitute Release
workspace output or extract production functions. Old link-cap, observer and SDK
selection failures remain recorded. This is scoped matched incremental software
qualification; strict hardware and root semantic integration are separate gates.

The strict hardware continuation is sealed in
`docs/validation/POLYGON-AFFINE-HARDWARE.json`: actual PCI03 A770/EGL renderD129
and open-FD/sysfs proof,236 checks/36 image pairs with max0<=1, and intended
same-binary software/UHD identity refusals. Source, executable, mapped ELF and
SDK/driver hashes are unchanged. The lease is released. The first hardware
observer's omitted five device checks remains an explicit failed observer case.
This completes the authorized bounded polygon-affine cohort; coherent integration
with current retopology/remesh and broader regression remains separate.
