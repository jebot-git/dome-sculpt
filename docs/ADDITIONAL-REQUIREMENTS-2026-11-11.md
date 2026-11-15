# Additional XR authoring requirement — 2026-11-11

The user requested: “Additionally, mark spatial node-based editing/history as
further goal along with controller trays”. This extends the complete original
Dome Sculpt objective. It is a required later XR phase alongside the existing
controller-mounted tool/function/selection drawers, after basic authoring targets.

Provide spatial node-based editing and an inspectable spatial representation of
editing history in the existing native C++20 FLTK/OpenXR application. Controller
interaction should support selecting and arranging nodes, editing their exposed
parameters and connections where applicable, and inspecting/navigating prior
editing states with a clear indication of the active document/history state.
The exact node types, graph layout, controller bindings and whether history
branching is appropriate remain design work; this request does not prescribe a
new document format or silently replace the current history model.

Use the existing shared document commands and preserve original geometry,
material/source-role and avatar authority, exact Undo/Redo/cancellation, stale
source/generation rejection, ordered saves and crash recovery. Node/tray focus
must prevent unintended scene strokes and handle tracking/reference/focus loss.
Respect DomeD universal-menu, bitmap appearance and standard keybinding rules.

Acceptance needs actual native/controller interaction, persistence/history
correctness and physical-headset usability evidence for representative editing
and history workflows. A flat desktop node graph, static diagram, controller
tray alone, or accepted earlier selection UI does not complete this requirement.

The spatial presentation is deferred; underlying material, painting, geometry,
history and other basic functionality already required by the original goal
remains required in its original priority. Animation and final scene rendering
remain excluded. No implementation or qualification is claimed here.

The user subsequently reactivated the full goal. This addition remains future
scope alongside controller trays, after basic authoring targets; reactivation
does not mean this later phase has started. See [HANDOFF.md](HANDOFF.md) for
the current integration checkpoint.
