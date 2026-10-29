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
