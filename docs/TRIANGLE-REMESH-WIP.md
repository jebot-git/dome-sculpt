# Isotropic remeshing authoring checkpoint

This private continuation of main10944 connects the existing native isotropic
kernel to an accepted document worker, FLTK Mesh command and shared XR edge-length
controls. It is **not admitted to main**. Both final native processes exit1:
the existing polygon editor still requests a similarity transform when editing
an object with exact affine placement. That failure remains an integration gate.

The operation measures edge length in world millimetres using the complete affine
placement, evaluates the displayed pose/morph and solves in world coordinates.
It maps the result back to the original local frame, retaining genuine placement
and reflection winding/seam correspondence. The new object has editable logical
polygon topology and a fresh unpainted atlas. The complete painted, rigged source
is retained hidden with a restorable original capsule; this operation does not
claim texture or rig transfer to the new topology.

Actual sheared-source testing exposed a kernel failure: successive operations
could create a numerically unresolved thin triangle whose float normal no longer
matched any eligible source face. The new scale-aware area screen rejects those
candidate edits and retains the previous valid topology. Projection eligibility,
distance bounds, budgets and the original triangle/voxel suites remain intact.

Release89996 and sanitizer38194 are terminal1 with no protected-input drift.
They print2155 passing lines/profile and the same affine-editor failure. The
successful checks include ordinary/sheared/reflected posed-source extraction,
independent analytic shell/winding checks, genuine Restore, native Save/Open,
immediate queued Save, exact Undo/Redo, gated worker cancellation with rejected
dependent Save, retry, controller controls and focus cancellation. The complete
unchanged triangle-kernel and voxel-authoring suites also finish successfully.
Four saved scene pairs are byte-identical. No sanitizer diagnostics were emitted.

The sanitizer compile continuation80616 first stopped before linking because
the proof script referenced an absent historical executable when estimating link
space. Its completed objects were hash-verified and reused by38194; neither
source nor compiler flags changed. Earlier numerical, diagnostic and test-harness
failures remain recorded separately. No failed invocation is relabeled green.

Six complete changed core translation units precede immutable matching main
support archives; link maps exclude their old members. Frontend changes have
syntax evidence only. Full affine polygon editing, allocation/cancellation
sweeps, actual normal/compact FLTK and compositor clients, broader workloads and
device validation remain required. No installation or headset launch occurred.

Commands, hashes, original failures and exact scope are recorded in
[the validation receipt](validation/TRIANGLE-REMESH-WIP.json).
