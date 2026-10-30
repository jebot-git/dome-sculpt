# Ordered spatial input

Controller/stylus adapters submit original poses, pressure, buttons, sequence and
time through `Document::submit`. The span overload accepts a burst without
interleaving another producer between its samples. Rendering reads immutable
snapshots; model edits execute on the input worker.

A focus-loss frame is an ordered cancellation boundary. Earlier trigger releases
and UV Apply actions must be processed before that boundary. An unfinished
stroke or UV draft rolls back when the boundary is reached. Clearing the queue
when focus was lost previously erased queued confirmations, making native UV
persistence depend on worker timing. A deterministic burst regression reproduced
that loss before the fix and now verifies retained paint/UVs and exact undo.

Session shutdown stops accepting samples, drains previously submitted events,
then cancels the unfinished action. Late samples are ignored until
`begin_spatial` reopens input. Each native OpenXR READY transition reopens the
stream, including resumption after STOPPING/IDLE. This follows the
[OpenXR session lifecycle](https://github.com/KhronosGroup/OpenXR-Docs/blob/main/specification/sources/chapters/session.adoc).
The native runtime fixture verifies a second session begin and a new sculpt
stroke after controller release/re-press. This covers resuming the existing
session; recreating a lost instance/session remains separate unfinished work.

The queue remains bounded at 64 samples. Overflow cancels the unfinished action;
reference-space rebases and document generations still invalidate old-space
samples. This change does not permit editing from stale poses. Controller
ownership and release-to-rearm rules are unchanged.

Verification covers a queued completed stroke before focus loss, an unfinished
stroke at focus loss, queued completion at shutdown, rejected late input, input
reopening, accepted UV Apply before focus loss, continuous UV scaling/turning,
fine-wheel rates, exact cancellation and global undo. The XR test harness writes
incremental reports with a `complete` flag and preserves generated projects and
UV/restart comparison JSONs under its output directory. Physical-device behavior,
production-size latency and instance-loss recovery remain unverified.

UV previews now schedule cancellable background validation. An accepted Apply
captures its exact immutable mesh for independent validation and transfer on the
asset worker. It remains ordered before later focus loss/shutdown; invalid
captured targets cannot commit paint. Unaccepted draft checks are invalidated
on cancellation/replacement and cannot recreate a closed workspace. See UV.md
for pending-check/commit behavior and the separate wait helpers.
# Standalone XR process termination

SIGINT and SIGTERM set a lock-free stop flag; the handler performs no rendering,
I/O, model editing or cleanup. The normal XR loop finishes the current frame and
uses the existing shutdown boundary: drain queued completed strokes/accepted
operations, cancel unfinished gestures, release session resources, wait for
pending accepted asset work and perform the requested CLI final save. Previous
signal handlers are restored on exit. Real-loader regressions synchronize the
signal with a rendered second stroke and compare the saved volume to an
independently completed first stroke, proving both retained edits and exact
unfinished-gesture cancellation for SIGINT and SIGTERM.

Native retopology uses the same pressure ownership and focus boundaries, with a
dedicated topology worker. Joystick navigation reaches page 16 from Mesh tools;
while editing, a joystick press opens that page directly. Tool mode switches
Place/Move, and Snap range uses the common logarithmic joystick/ray slider.
Points project in source-local coordinates with a document-world distance bound.
Held placement/move cancellation restores its draft baseline; focus/session loss
retains the surrounding patch workspace. Explicit Apply queues ordered guarded
asset publication, and later focus loss does not retract that accepted operation.
The redundant Mesh-tools Undo sector now opens Retopology; root/hardware Undo
remain available and route to draft history. See RETOPO-AUTHORING.md.

Retopology page16 uses cell0 as New patch / Resume patch. Starting or resuming
closes the menu so pressure can place/move points; Tool mode also resumes the
canvas. Face/edge/pin/relax commands can keep the page open for chaining, with
Resume available afterward. Apply/Cancel close the menu; a subsequent joystick
press reopens the active patch page. Skin14 and Masks15 remain separate.
Standalone CLI final save cancels only an unconfirmed patch after accepted work
drains; stopping embedded XR retains the draft for desktop continuation.

Plane-cut authoring uses page 17, reached from Mesh tools sector 4 (Center remains
available on the baking page). While cutting, joystick presses open page 17
directly. Offset, two-row Direction and four-row Cap color use the common
joystick/ray controls. From hand captures the confirming menu device's current
grip in document space, rather than preferring an idle right controller.
SpatialInput retains command_device alongside its consumed command for that
purpose. Release-to-rearm and ownership rules remain unchanged. Tracking/focus
loss rolls back the active parameter adjustment; accepted Apply remains ordered
through XR shutdown and asset waits. See PLANE-CUTS.md.

# Sculpt masks and symmetry

Desktop Sculpt → Masks and symmetry edits a draft of the active source object's
mask mode/tool/strength, overlay visibility, mirror axes and local plane origin.
Numeric plane fields use millimetres; paired sliders support continuous edits.
Return initially chooses Cancel. Apply unchanged values creates no undo entry;
Apply changed values creates one guarded transaction. Standard Close/Escape and
Ctrl+W cancel the draft. Source object locks/visibility remain authoritative;
mask painting can edit protection on a hidden/locked geometry layer.

Sculpt → Paint protection mask enables pressure painting. Add/Remove/Smooth act
on protection; Clay/Carve/Smooth/Flatten and keys1–4 explicitly return to geometry
while retaining the mask. Radius changes preserve the current mode. Protect all,
Clear and Invert execute on the ordered asset worker. The global default also
protects geometry added later; layer visibility/strength changes stay independent.

In XR, Root → Objects → Sculpt layers → Masks opens page15. It provides mask
painting/Resume, mode, strength, Fill/Clear/Invert, XYZ mirrors, planes/view and
Cancel work. Planes/view has four large ray/stick rows: local X/Y/Z origins and
Show mask. Native and spatial controls share the same persisted state. Resume
closes the menu to expose pressure input. Release after an action before pressing
again; the existing source-lease rearm boundary also applies to mask controls.
A bulk worker publication rebinds the source and closes the menu; reopen it by
the same route. Undo restores the complete prior mask in one step.

Darkened surface feedback preserves PBR/MToon coverage and material picking.
Mirrored brush rings follow object/group-local planes. Focus/tracking loss cancels
an unfinished protection stroke or unaccepted parameter draft; completed earlier
edits survive. Skin14, Retopology16 and Recovery remain on their existing routes.
See SCULPT-MASKS.md for bounded storage, compute and validation details.
