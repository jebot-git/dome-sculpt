# Native desktop and headset baking

Keep the high-detail object, select a separate low-detail target and prepare its
painting mesh/UV atlas. The high source may be hidden or locked. The target must
be visible, unlocked and in its rest pose with zero active deformation; unsupported
posed targets fail explicitly. Source models, images, rig/morph/paint and target
editable layers stay intact.

Mesh → Bake surface opens the FLTK source picker and projection settings. Front,
back and AO radius are world millimetres, with logarithmic dragging and fine
wheel adjustment. AO samples and seam-padding pixels are integer sliders;
zero samples disables geometric AO. Defaults are 50/50/250 mm, 16 samples and
8 pixels. Kernel coordinate/ray/memory limits still apply; invalid cages or misses
show a visible failure and retain the complete prior surface.

Bake queues detached worker computation. The progress bar and window title show
estimated phase progress; the viewport keeps its previous immutable snapshot
until successful publication. The dialog stays open for another explicit Bake
or Close to inspect the result. Return initially activates Cancel. Escape,
Ctrl+W, universal Close Window and the window-close callback dismiss options;
if a bake is running they request cancellation. The Cancel bake button requests
cancellation and waits visibly for the worker to stop. A completed bake can be
reverted with one global Undo. Clear baked surface removes the base beneath
editable layers and is also one global Undo step.

The headset route is Objects → Mesh tools → Baking. This replaces the duplicate
Sculpt layers shortcut on Mesh tools; Sculpt layers remains under Objects.
Select Source, Projection bounds or AO / padding with the joystick. Aim + trigger
drags the large sliders. Left/right adjusts continuously; up/down selects a
component on multi-row panels, and a scalar panel has the existing fine-wheel
adjustment. Source names and hidden state are displayed. Done/joystick commits
settings only; Cancel or focus/tracking loss discards that staged panel. These
settings do not dirty the scene or add history. Bake surface explicitly starts
the worker. Its page shows progress and enables Cancel work while running.
Accepted completion publishes maps and closes the spatial menu for inspection.
Reopen the page to rebake/clear or use the normal Undo/Redo/save/export commands.

Settings are session controls; generated maps and retained editable sources are
persisted in native projects. A durable bake recipe, custom cage geometry,
partial masks/multiple source selection, supersampling and production-scale
quality/performance remain further work. This is an implemented projection-bake
workflow, not completion of every production requirement.

Worker and cancellation contract
-------------------------------
Document owns a per-operation AssetWork handle with atomic progress/cancel state.
The detached closure captures this status resource, never UI widgets or mutable
Document/model access. EditorState receives copied progress/availability when
published; renderers still acquire immutable geometry/state snapshots.

Cancel background work is also an actual File command, available only for
cancellable scene operations. Cancellation does not clear busy state early.
The worker brackets calculation and validates its original generation, revision,
content and selected target. It checks cancellation again under the model lock
before publishing, so a returned candidate cannot race an accepted cancellation.
Cancellation publishes no scene/history entry, clears busy state when terminal,
and reports a cancelled message rather than an error. New-document generations
keep older jobs from changing their state or clearing a newer job's status.

The baker reports monotonic estimated progress during preparation, triangle
projection, padding and final installation. It is not a time/texel-weighted ETA;
large individual triangles and uncancellable bounded upstream Mikk phases can
hold a percentage. Cancellation checks in projection/AO/padding remain independent
of progress reports. See BAKING.txt for kernel contracts and production limits.
