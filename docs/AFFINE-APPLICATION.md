# Scene 13 application consumer checkpoint

This private stage adds actual Document publication, mesh packing, material-aware
picking and native desktop paint for full immutable affine placement. It merges
published export-cancellation source 63df; accepted export cancellation tokens,
callbacks and atomic writer semantics are preserved. The earlier Scene 13 codec
and hierarchy-preserving GLB exporter remain in its ancestry.

Packing uses the complete factored E/native-TRS/original-local chain. Positions
use the full affine map, normals its inverse transpose and tangent directions
its linear part; transformed source tangent W includes determinant sign. Shader
normalization/interpolation conventions remain unchanged. Only transient packed
indices compensate reflected primitive orientation; original ordered corners,
UVs, rigs and frame owners remain unchanged. Published ObjectInfo/SurfaceRange
carry an optional full matrix instead of falsely marking affine placement as a
Canvas similarity. Geometry identities compare complete matrix values and retain
all prior immutable geometry dependencies; scalar painting reuses the same key.
Protected-object equality includes source-node and affine-edit owners.

Native painting prepares the current skin/morph geometry in world coordinates,
then uses the existing evaluated-paint machinery to update original UV tiles.
This gives the actual world-space spherical brush metric under nonuniform scale
and shear. A CPU-only query reflection flag preserves original ordered corner
identities during ray tests. World-frame capture is checked before every native
preview/commit; existing source/authority/generation/cancel gates remain. Public
raycast/gradient select the current world-space active surface.

Completed Release proof: 47 actual Document checks include analytic packing,
normal directions, reflected front/back picks, static and combined skin/morph
world-brush footprints, exact Undo/cancel/save/restart, original-source retention
and unchanged geometry identity during paint. Eight scoped existing/new CTest
suites pass (seven in 8.83s, shared-document separately in 6.96s). Actual graphics
consumer tests pass 13 checks on software GL and Intel Arc A770: independent
analytic geometric and retained-PBR normal/tangent/reflection pixels, real first
geometry uploads and zero geometry bytes for the second eye. Tests use the
canonical FLTK eee23b7 runtime. The first retained-frame fixture correctly refused
nonunit authored base directions; corrected unit inputs pass, with that negative
log retained. This was a test-input error, not a relaxed production validation.

Real FLTK/Xvfb/universal-menu flows pass six assertions: while detached preparation
is blocked, Escape/cancel and Save remain responsive and source-exact; accepted
release, Save and Close drain in order and update original UV paint without
changing rest/skin/morph/affine/source fields. The screenshot was visually checked.
It uses explicit isolated X11 test mode and does not change Wayland-first product
behavior or the active desktop session. Fresh whole-own-core/support C/C++
ASan/UBSan/leak qualification is running at this source checkpoint; no terminal
sanitizer claim is made here yet.

General affine import admission still retains the existing similarity guard.
Physical held-object/controller preview, XR painting, affine SDF brush/resolution,
remesh, bind/transfer, bake and polygon/cut/retopology consumers remain active
required followups. Their affordances are guarded where their old Canvas/metric
path would misrepresent placement. This checkpoint does not describe those
operations or full affine VRM/general import as complete. Source PBR UV1/no-map
UV authority, FBX skin/morph and original joint/bind-space hierarchy work remain
on the original full import scope. No system install, push or HMD test occurred.

Held followup design: track a similarity pivot at full object-world origin,
without approximating its orientation/scale. Native world gesture D commits
E_new=P^-1*D*P*E_old. Keep source local chains untouched and avoid per-frame SVD.
Preview needs full current/packed matrix delta plus inverse-transpose normal
uniform, and ephemeral preview-E/pivot fields in Transaction/AuthorityTransition
rollback. Ordinary similarity behavior remains a separate exact legacy path.
