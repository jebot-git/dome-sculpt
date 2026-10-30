# Native project files on the ordered worker

Desktop Open, Save, Save As and startup project loading use `open_async` and
`save_async`. These return ordinary promise-backed futures; destroying an ignored
future never waits for I/O. CLI/tests may use synchronous `open`/`save` wrappers.
The renderer continues acquiring immutable snapshots while file work runs.

A Save captures the committed scene and history identity at acceptance. Ordered
jobs write temporary files, sync, rename and sync the directory before publishing
success. Later edits remain dirty; failures retain the prior project path and
saved identity. The shared worker drains accepted saves during document shutdown.
The published project path is authoritative for window titles and subsequent
Save operations, so an unwritten Save As path is never adopted by the UI.

Open leases authoring while the ordered worker reads and decodes a detached
scene. It checks cancellation and document generation, and atomically replaces
the scene/history/path only after successful validation and a final content and
active-gesture check. Failure/cancellation preserves the old scene and history.
Replacing the document invalidates an older queued Open; its result cannot
change the newer document's status or path. Background-work cancellation also
cancels Open. UI discard confirmation remains required before replacing edits.

File reads enforce the 256 MiB limit as bytes arrive, including if a file grows
after the initial size check, and check cancellation per 64 KiB chunk and around
CBOR parsing/Scene decoding. Those two parsers currently have cancellation
boundaries rather than fine-grained interruption. Save snapshot creation and
final state publication still acquire the model lock; file encoding, disk I/O,
parsing and validation execute on the worker.

`project-files` uses a native fsync barrier to hold the actual worker before file
replacement. It exercises responsive editing, immutable ordered snapshots,
queued Open cancellation, new-generation invalidation, failures, project path
adoption, read cancellation and save draining without timing a fast filesystem.
Combined Release, sanitizer and real FLTK menu evidence must be recorded after
integration; this document alone is not validation evidence.

Integration additionally verifies Save As recovery epochs: adopting a new path
invalidates queued recovery work, then schedules cleanup against the current
epoch and requeues any newer unsaved content. Ordered saves compare recovery
source identity/generation rather than an obsolete captured path. The original
recovery-retention assertion passes unchanged under whole-core sanitizers.
