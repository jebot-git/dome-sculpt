# Owned controls while selection is active

Selection mode previously suppressed every trigger-pressure sample, including
samples addressed to an actual open modal control. A controller could open the
UV workspace or a slider, but its ray could not activate the control.

The shared input now reports whether its live menu owns the exact control
parameter. Document checks that ownership together with the active object,
generation, edit-authority lease and worker-admission state before allowing
modal pressure through. Selection still disables scene editing after all mode
overrides, including retopology. This changes neither direct object-selection
gestures nor the physical selection UI previously accepted by the user.

The isolated regression opens an actual brush-radius panel while selection is
enabled. It changes the owned slider and verifies that the Scene remains exact.
Cancel, explicit close, focus loss, tracking loss and Confirm are each exercised
with a held trigger. Returning to selection cannot leak a scene stroke; leaving
selection with the trigger held still requires a release before ordinary editing
rearms, and that fresh edit has one exact Undo.

The same corrected fixture against the unmodified b265706 core opens the panel
successfully and fails at the ray-pressure assertion. This distinguishes the
original product failure from an unavailable control or an incorrect radial
sector. Earlier fixture-development failures are retained as artifacts and are
not counted as product negatives.

The Keep-normal-detail authoring branch separately uses this repair and passes
its selection-on UV control path. That branch adds other fields and is not part
of this narrow source patch. Native retained-normal export and further normal
authoring remain separately incomplete. No physical headset launch or new human
acceptance is inferred from these automated fixtures.

Exact commands, final result scopes and hashes are recorded in
`validation/MODAL-SELECTION-CONTROLS.json` after validation finishes.
