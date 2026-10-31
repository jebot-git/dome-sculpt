# Controller selection and XR scene hierarchy

This is a focused extension of the user-passed `993e8af` candidate. The broader
production goal remains paused. The previous candidate and its saved physical
headset session are preserved.

The user assessed this interface as **acceptable** after the physical headset
test on 2026-10-31. The test ended gracefully and the edited session was saved
and reloaded successfully. See `validation/XR-SELECTION-HEADSET.json` for the
exact source, runtime and session record. The production goal remains paused.

Open the controller menu and choose **Selection** on its first page. The same
page has a directly clickable **Selection ON/OFF** button and **Hierarchy**
button. The hierarchy's header also toggles selection directly. The desktop
universal Scene menu exposes the same state through **VR selection mode**,
**XR object hierarchy** and **Focus selected objects**. Brush flow remains
available through the existing brush/paint controls.

In selection mode, touch a surface with either controller and press its trigger.
A surface within 4cm of the controller takes priority; otherwise the controller
can point at a surface up to 5m away. Hold either grip while pressing a trigger
to add or remove an object. Releasing and pressing the trigger again produces a
new selection action. An ordinary trigger press on empty space deselects all.
Grip acts as a selection modifier in this mode, preserving the workspace pose.

Selected objects stay solid. The last added object becomes active and has a
stronger session-accent cue; hovered objects and pointer hit markers provide
feedback before clicking. Unselected visible objects remain at **18% opacity**
so the complete model remains visible as context. The original material alpha
and cutouts are preserved. Explicitly hidden objects stay hidden.

Turn **Selection OFF** to edit the active object while retaining the selection
and transparent context. **All objects solid** releases selection focus.
Existing visibility, locking and held-object editing restrictions still apply.
A trigger held during a mode change or after a hierarchy click cannot become a
brush stroke; release it first. Focus loss similarly requires a fresh release.

The XR hierarchy lists the actual scene groups and objects with indentation,
expand/collapse arrows, selected and partially selected group states, an active
object marker, visibility controls and lock indicators. Click a group name to
select its descendants. Grip plus trigger toggles a whole group as a set.
Collection selection prefers a visible unlocked child for active editing while
retaining hidden descendants in the selection. Collapse preserves membership.
Previous/Next pages cover the complete list. Object visibility changes use
ordinary document undo/redo. The interaction follows the separate active and
selected states used by the [Blender Outliner](https://docs.blender.org/manual/en/3.6/editors/outliner/selecting.html).

Selection and hierarchy layout are workspace state; saved scene content retains
its existing active-object representation. Original object transforms, retained
sources, topology, paint and materials are unchanged by selection. Document
replacement clears stale selection identities. Full-frame immutable snapshots,
source-generation/authority checks and existing accepted-save semantics remain
in use. Pointer selection uses the worker-built material-aware surface BVH;
rendering changes object uniforms without rebuilding geometry or reading GPU
geometry back to the CPU.

Validation and remaining qualification are recorded in
`validation/XR-OBJECT-SELECTION.json`. Synthetic controller/OpenXR fixtures are
separate from user confirmation in a physical headset. Full production
requirements remain in REQUIREMENTS.md and HANDOFF-466ebd1.md.
