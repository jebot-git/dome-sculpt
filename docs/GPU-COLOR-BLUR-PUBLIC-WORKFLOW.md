# Public GPU color blur workflow fixtures

This test-only continuation adds actual universal-menu and OpenXR-loader workflows
for native GPU blur. C++/Python syntax and source checks pass. No new target link,
GUI, loader or hardware execution is claimed yet. The private branch now starts
from root `775f92b8`; product sources and canonical dependency metadata are unchanged.
Root's frozen build is untouched. The earlier 82-based review remains archived. Root source review caught an
unexecuted producer mismatch: direct blur options default to0.5 while public
commands use default Flow1. The producer now asserts actual public Flow1 and
invokes `paint_command(BlurColorLayer)`; the independent oracle still checks1.
Existing product defaults remain unchanged. The first actual producer attempt
then refused before EGL because a detached PBR paint had no matching retained
Scene color-preparation binding. That failure remains in release-fixtures-v1.
The producer now builds the genuine imported source/normal/rig fixture and uses
public `prepare_source_colors` to retain the paint and binding together; it keeps
Scene validation and the independent pixel oracle.

Existing GUI 81/XR 117 scripts, inputs and deadlines remain byte-identical. Current
139378 software/139458 Arc native controls retain their original dispatch. An
explicit `--workflow-fixtures OUTPUT` mode on the native blur target produces a
genuine nonuniform 64 px strictUV input: separate RGB/authored alpha and coverage,
prepared PBR color, source color/normal owners, other common channels, physical
FLOAT emission, independent mask and hidden zero-opacity layer. It runs an actual
accepted Document operation at default Flow 1, checks every relevant output pixel
with the existing independent linear-light cross-kernel oracle, proves exact
global Undo/Redo and writes original/accepted native files plus a manifest. Fresh
paths are required. This offline producer does not drive the application under
test; its output files are immutable reference inputs during UI scenarios.

`tests/gpu-color-blur-menu.py APP INSPECTOR MANIFEST OUTPUT` starts the ordinary
application on private Xvfb and invokes its actual exported DomeD universal Paint
command. It queues ordinary Save immediately after Blur, inspects the whole
accepted native scene, exercises standard Ctrl+Z/Ctrl+Shift+Z with exact native
Save endpoints, closes and freshly reopens the result. Public X11 titles/focus,
window screenshots and actual DBus menu properties acknowledge state. Failure
diagnostics capture root/windows/focus. Limits remain 50 seconds per wait and 900
seconds per phase. The shell's rendered global menu is outside this Xvfb scope.

`tests/gpu-color-blur-xr.py APP RUNTIME INSPECTOR MANIFEST OUTPUT` loads the separate
`dome-sculpt-gpu-color-blur-runtime` module. It derives genuine loader/swapchain
plumbing from the retained public-ack runtime and supplies synthetic OpenXR
Boolean/vector/pose input. Independent left/right released stereo pixels acknowledge actual radial
labels, selected cells, panel and checkbox state. Each eye uses its own +/-32mm
projection, reference title and consecutive visibility history; every page, Save
readiness and disappearance gate requires both eyes. The initial c74 source review
had left-only acknowledgments with both-eye captures; it was never run and is
retained as unqualified source history. The stronger scenario first proves Painting's
unchecked Erase, toggles it on, reopens Painting to prove the filled checkbox,
enters Brush controls and proves Filters has no Erase checkbox, then opens Filters
and invokes Blur. The same single scenario uses ROOT Save with correct public
history availability and subsequent palette disappearance, then one real Undo
and Redo. External gates advance only after actual atomic native Saves reach the
exact accepted/original/redone whole-scene endpoints. Both eyes, action delivery,
frame/image/session ownership and mapped ordinary app/runtime are checked.

The original 6000 physical-frame/240 second limits remain. Pixel tests derive from
the source palette's radius 220,110x40 cells,8x8 checkbox/4x4 fill and canonical
foreground/disabled/accent colors. No private Document state, direct callback,
wrapped dispatcher or product success marker controls runtime progress. Original
117 scenarios continue using their unchanged independent module and script.

Success needs actual hardware, including the worker. Setting
`LIBGL_ALWAYS_SOFTWARE=1` makes the explicit product command refuse. Private Xvfb
may provide software GLX while the separate EGL worker uses hardware; that route
still needs actual qualification. XR explicitly checks the chosen hardware
renderer. The eventual coordinator must authenticate device/mapped providers and
hold `/tmp/dome-sculpt-arc-device.lock`. No physical headset/session is started,
modified or restarted by these scripts.

Build the changed native test and new module only after coordination with root.
The planned hardware sequence is fresh native fixture production, normal/compact
public desktop cases and the actual-loader XR scenario, under the shared lease.
Preserve strict unsuppressed SAN settings and actual process/evidence outcomes in
both profiles. No timeout/count reduction or software success fallback is allowed.
The companion source receipt records syntax commands and exact inputs, rather
than claiming runtime qualification. Software-refusal presentation, further
busy/modal/unsupported-domain GUI cases, compositor behavior, production throughput
and physical controller usability remain outside this minimal added scenario.
