# Active-layer GPU color blur: private implementation

This branch starts the genuine native painting integration after the isolated
ArmorPaint extraction. It is a partial product milestone for review,
not completion of the layered painting/material/baking backend objective. No
installation, physical-headset test or main product admission is claimed here.

The first operation blurs one active layer's common RGB, authored alpha and color
coverage, plus its optional independent prepared PBR RGB/alpha. These are three
different semantic quantities; authored alpha never substitutes for paint
coverage. RGB is decoded/filtered in linear light and deliberately re-encoded
into the existing byte color plane. The native independent layer mask gates
the operation. Other common14 components, layer mask, legacy/physical emission
and actual FLOAT coverage, hidden-layer contents, source/normal/material roles,
geometry, rig/morphs and original file metadata are retained. Existing native
codecs persist the result; this introduces no flattened preview/project format.

The current admission subset is an unambiguous strict current writable UV grid,
including actual selected texture-set membership. Retained writable-coordinate,
repeated/overlapping original-coordinate policies remain explicit guards.
Original material maps and typed frames on an otherwise admitted current grid
are preserved as their actual owners, not resolved into new generic textures.
Future source-domain adapters must authenticate those broader coordinates
rather than weaken these guards. The full original requirements remain in force.

## Native data and worker boundary

`PaintedMesh::blur_color_layer_gpu` reads genuine active-layer COW tiles. It
plans only tiles containing color coverage and their neighboring 7px halos,
forms indexed UV chart labels while respecting authored seam edges, and rejects
foreign material membership. Three small FLOAT32 input planes carry native
sRGB common/PBR color/coverage and independent common/PBR authored alpha, mask
and chart identity. The GPU decodes sRGB, filters associated linear values,
unassociates and encodes the native color result; CPU packing performs no color
power functions, filtering or unassociation round trip.
The GPU uses the exact pinned ArmorPaint cross-kernel weights, with explicit
per-chart weight normalization and typed destination blending. No normal or
emission vector is interpreted as generic color. UV chart separation has real
native geometry controls in addition to the independent GPU-label oracle.

The file/asset worker owns a private EGL context, actual GL textures, a bounded
result PBO and a completion fence. Any hardware-backed EGLDevice/DRM node meeting
OpenGL 3.3/FLOAT-MRT requirements is eligible; Arc-specific PCI authentication is
validation policy only. An explicit GPU application command has no software/CPU
fallback. Software execution is an explicit detached validation mode and is
rejected by `Document` commands.

Native CPU sparse tiles are still the authoritative editable/persistent data.
The worker uploads dirty 32px tiles plus 7px source halos and asynchronously
returns only completed 32px dirty results for durable COW authoring. Fence polling
is cancellable and runs on the worker, with no UI/XR thread wait or whole-atlas
per-frame readback. It is not yet a GPU-resident native history/cache model.
CPU packing, connectivity planning and final native byte rounding remain;
color conversion, blur/filtering and typed accumulation execute on the GPU.

All allocations, GPU results and native histories stage on a private draft. The
public result swaps only after full validation/completion; failure/cancellation
leaves the original unchanged. Existing accepted SceneOperation and recipient
leases enforce generation/revision/object/edit/set/geometry/material/source/
deformation/group/placement identity. Existing policy requires releasing held
object motion before this asset operation. One accepted command has one global
Undo/Redo endpoint, and ordered Save waits for that exact accepted scene.

Desktop uses the canonical universal Paint menu. XR preserves Painting → Controls
and every slider sector. Controls → Filters → Blur color layer opens the new
operation; Back returns to Controls. Filters replaces only the duplicate Erase
entry on Controls; Painting → Erase remains directly available and toggles the
actual brush. The
existing paint Flow setting supplies strength for commands. No new frontend,
font path, keybinding override or application-owned menu bar is introduced.
Actual controller qualification uses public synthetic SpatialFrame routing;
physical headset/controller usability remains required.

## EGL lifetime prerequisite

The worker and XR renderer may receive the same EGLDisplay even when they own
different contexts. The previous helper could terminate the live renderer's
display when the shorter-lived worker exited. The branch adds mutex-protected
display leases: initialization/termination occur once per helper-owned display,
and only the final helper lease can terminate it. Already-initialized external
displays are borrowed and never terminated by this helper. Their external owner
must outlive the borrowed contexts; a caller using a helper-owned display must
retain its owning helper lease while creating/using external resources on it.

Failed construction restores the previous selected API. Teardown preserves a
different caller's current context/API and never calls blanket eglReleaseThread.
The class ABI is unchanged. Real tests cover a live renderer texture while a
worker context dies, plus externally owned same-display OpenGL and GLES contexts.
Existing deterministic constructor-failure/foreign-context checks remain required.
This ownership fix is separate from the independently observed 132KB
driver leak; it does not claim to diagnose or repair that leak without evidence.

## Qualification scope

Release and whole-core ASan/LSan/UBSan builds exercised the same native implementation.
Both explicit software controls and the Intel Arc route passed the independent
kernel oracle (61,442 software / 61,444 Arc controls), native color/PBR/hidden/
FLOAT/source/history/dirty-tile controls and 12 real shared/borrowed-display
controls, with 40 deterministic helper-lifetime controls. Sanitizer runs use
`detect_leaks=1:halt_on_error=1` and strict UBSan, without suppressions. The
full native application was linked with the current DomeD FLTK SDK in Release;
this is not a physical headset or installed-build acceptance claim.

Hardware native checks exercise actual accepted worker operations, ordered
Save/reopen, one global Undo/Redo, cancellation behind an actual EGL-acquisition
barrier, stale-generation rejection, imported source/rig/morph/material owners,
genuine extra texture-set membership and public synthetic controller routing.
The first real plain-object recipient adoption failed because the previous
texture-set adapter required an explicit collection. Its original failure is
retained; the repaired adapter preserves genuine plain-recipient authority and
passed the same worker/history endpoint checks. An earlier negative fixture
also failed before reaching the tool; its corrected control creates actual
original-source authority rather than weakening the guard.

The legacy shared-document test initially asserted only packed mesh storage.
The unchanged baseline already publishes sculpt geometry as separate indexed
chunks: the seed diagnostic has 19,944 actual chunk triangles and no packed
triangles. The repaired test captures independent exact triangle-value copies,
requires the edit to change them, checks the held owner remains unchanged and
requires Undo to return those exact values. The original failure is preserved.
Legacy controller controls also test the retained slider shortcut, filter Back,
focus-loss cleanup and the direct Erase toggle. Compact receipts pin actual
sources, commands, outputs, retained failures and their verification limits.

Reproduction targets are `dome-sculpt-gpu-color-blur-kernel-test`,
`dome-sculpt-gpu-color-blur-native-test`, `dome-sculpt-native-graphics-shared-test`
and the existing `dome-sculpt-native-graphics-lifetime-test`. The first three
are registered as explicit software CTests. Pass `software` for detached
validation; native document/controller success controls require hardware.
Native hardware checks also take an output directory as their second argument.
For Arc validation use `DRI_PRIME=pci-0000_03_00_0`, remove software overrides
and hold the exclusive `/tmp/dome-sculpt-arc-device.lock` for the entire process.
The kernel test authenticates the local Arc renderer/node/PCI pair; that
machine-specific test policy is not a product vendor restriction.
Weights are generated offline with CMake in the build directory; Python is not
required for the native product build. No installation or running root-candidate
session is changed by these checks.

Device identity has two distinct observations: the GL renderer describes the
rendering context, and EGLDevice/DRM describes the EGL display. DRI_PRIME can
offload GL rendering to a different device while EGL still names the display's
primary device. The independent baseline lifecycle audit observed that mismatch
on a UHD route with an Arc EGL display node. An EGL node alone is therefore not
proof of the context's physical PCI identity. Strict Arc controls require both
the Arc renderer and the expected EGL/DRM/PCI pair. Generic product admission
does not require Arc; its worker resources remain private, so differing worker
and renderer routes do not introduce renderer-texture interop or readback.

Remaining work includes broader typed channels/source domains, clone/smudge,
procedural brushes/material nodes, masks/layers/texture sets, baking/cages,
GPU-resident/cache optimization, failure/production/device evidence and the full
XR authoring interface. One color filter does not redefine the complete objective.

## Next hosted paint boundary

This is one real layer operation, not a replacement for the original full
layered painting/material/baking objective. The next coherent boundary is an
accepted native paint transaction with typed per-layer tile resources and an
explicit source/geometry/set lease. A resident tile cache can be keyed by the
actual native immutable tile owner plus role, precision, revision and device;
its output must retain a sealed dirty-tile/fence result and native cancellation,
ordered Save and exact history endpoints. A preview-composite texture cannot
authorize that cache or replace its common/PBR/MToon/normal/emission planes.

The isolated experiment's 512px cap is not inherited by this tiled operation:
it handles the existing native 64–2048 grids, subject to the actual retained
source/history/chart and dirty-output memory guard. The sparse 512px test proves
new neighboring tile allocation and preservation of at least 240 original tile
owners. This does not qualify 2048px production throughput or lift the native
2048 grid limit. Larger grids require bounded/tiled native raster ownership,
project/codec limits, layer/history budgets, source-coordinate authority and
measured scheduling/cancellation evidence before raising those caps.

Broader source-domain blur needs the existing original-coordinate transport and
seam/repetition policy to authenticate sampling and output ownership. Normals
need their actual tangent/source frames; physical emission needs unclamped
FLOAT RGB/coverage; MToon and PBR bindings remain separately typed. Clone/smudge
need original immutable stroke inputs and brush-space transport. Material nodes,
procedural brushes and GPU baking require actual graph inputs, mesh/cage/query
owners, per-role output admission and preservation of source/VRM metadata.
Those features, broader GPU-resident authoring, production failure/device
coverage and the streamlined XR authoring interface remain required work.
