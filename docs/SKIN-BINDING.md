# Binding and transferring skin weights

Select a visible, unlocked prepared mesh, then use Scene → Bind or transfer
skin. Automatic bind accepts a reference mesh carrying a fitted skeleton,
including a selected skeleton that has no weights yet. Transfer requires a
different reference mesh with skin weights. Hidden and locked references are
available for reading; the operation changes only the selected target.

Place the meshes in the intended rest-space alignment before binding. Distances
are world millimetres in the dialog and headset controls, independent of object
or group scale. Automatic binding exposes maximum seed distance, seed margin and
distance falloff. Transfer exposes the maximum nearest-surface radius. Both have
an influence cap, pruning threshold and explicit influence locks. Existing target
poses, morph targets/weights, paint, source appearance and archive identities are
retained. A newly bound target follows the reference pose. The existing rig is
accepted only when its exact ordered rest hierarchy matches the reference after
coordinate conversion; a different rig requires a separate explicit mapping.
Bone names alone never infer correspondence. Equivalent quaternion signs do not
create a false difference between identical world placements.

World-coordinate conversion changes the copied reference root transforms for a
new target rig. It leaves rest vertices and their identities untouched. Target
skin rows always refer to those original vertices. Existing landmark and morph
metadata stays intact; copied reference landmarks follow the same explicit frame
conversion. Identity placements avoid unnecessary floating-point transforms.

Hard locks retain their original exact positive values and zero assignments.
Unlocked contributions share the remaining normalization mass within the chosen
cap. Impossible locked caps fail explicitly. When locks leave no editable mass,
the operation retains the original rows without running a needless bind solve.
Unchanged rows preserve their original order. An entirely unchanged result adds
neither a dirty marker nor an Undo entry; a changed result is one complete
native Undo/Redo transaction.

The headset route is Objects → Avatars → Rig and shapes → Weights → Bind /
transfer. It also works for prepared targets that do not have skin yet. Source,
Method, Distances, Falloff, Influence cap and Prune open large joystick/ray
sliders. The three distance rows are Seed, Seed margin and Transfer, using a
zero-capable logarithmic scale. Done confirms settings only. Cancel, focus loss
and other implicit control boundaries discard a staged panel. Use locks selects
the current exact influence locks from the Skin weights tool. Bind / transfer
explicitly starts worker computation. Undo, Redo and Cancel work remain on the
same page. The existing painting tool remains available on the parent page.

Native Cancel, Return's initial Cancel focus, universal Close and Ctrl+W dismiss
the options without enqueueing work. Accepted operations run on the existing
ordered asset worker. While a job is running, File → Cancel background work or
the headset Cancel work requests cancellation. Cancellation retains the worker
lease until it is terminal, and a returned candidate is checked again before
publication. Source generation, revision and selected-object checks reject stale
dialog or controller requests. Failures retain the original complete scene.

Automatic weighting uses the existing bounded welded surface-graph kernel.
Weight transfer builds a bounded nearest-triangle BVH and barycentrically blends
the source weights. Its additive overload accounts construction, comparison,
node and triangle visits, checks cancellation within traversal, and enforces the
caller's work limit. The original API wrapper remains available with a default
50-million-operation bound. This workflow does not infer missing humanoid
landmarks or retarget different rest skeletons. Landmark placement/fitting and
volume-aware weighting remain production requirements in REQUIREMENTS.md.

## Validation

The isolated source at artifacts/verify-skin-workflow-6947 starts from6947fb2 and
contains only this workflow and the page14 palette support, excluding concurrent
material-holder changes. Native fixtures cover independent world-frame/pose
expectations, morphology and paint preservation, hidden references, fitted rigs
without weights, exact locks/no-op/history, stale leases, a deterministically
blocked executing-worker cancellation and stressed overlapping BVH budgets.
Actual CC0 VRoid data verifies that closed locks retain all original rig/morph/
paint/source identities. A real FLTK/DBusMenu fixture performs Automatic binding,
reference transfer, complete Undo/Redo and Cancel/default-Cancel/Ctrl+W.

The real-loader synthetic OpenXR fixture navigates the actual new page, uses the
tracked ray method selector and saves after accepted worker work drains. It
checks independently known transferred weights, fitted-but-unweighted rig
binding with exact pose/morph/source retention, and a focus-cancelled staged
method. Existing signal-interruption, linear/sRGB transport and other workflow
assertions remain intact. These fixtures do not prove physical controller use
or certify all production-size bind/fitting workloads. The final component snapshot passes three Release suites3.80s, three whole-core
ASan/UBSan/leak suites34.16s,96 complete real-loader OpenXR checks129.48s,
11 native GUI checks and5 compact double-BDF GUI checks. Logs are
artifacts/tests-skin-workflow-final.log, tests-skin-workflow-sanitize-final.log,
tests-skin-workflow-xr-final.log, menu-skin-workflow-final.log and
menu-skin-compact.log. Native and compact source-picker captures were visually
inspected. The compact640x360 test confirms scrolling, readable scaled labels,
responsive buttons and Ctrl+W cancellation. These component results are separate
from later whole-tree/material-holder integration validation.

Before committing, the exact scoped index tree was reconstructed over the
committed d505e97 CPU/GPU/mesh fixes in the isolated directory. Its complete
skin/UI/runtime-helper build passes and the three native suites pass3.74s
(artifacts/build-skin-staged.log and tests-skin-staged.log). Unfinished material
holders, dormant-source guards and unrelated export work remain excluded.
