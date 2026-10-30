# Explicit background export cancellation

This extends the existing native FLTK/OpenXR file worker. Product source is
`4cf3032`, following the polygon/color milestone. The full production objective
remains active. No archive version or replacement frontend is introduced.

File / Cancel background work and the controller Cancel work command now cancel
both queued and running snapshot exports. Authoring stays available during an
export, and project saves retain their ordered queue semantics. Explicit
cancellation reaches the accepted export even after the live document has been
replaced. It preserves existing destination bytes and removes partial temporary
output. Completion futures report cancellation; the native status retires the
export controls. Retry starts a new independent job.

Accepted snapshots still drain on application close and survive ordinary source
edits or document replacement. Internal authoring/asset cancellation remains
separate from the explicit background command. A global stopping or source-
generation predicate must not be used to cancel accepted exports.

Each accepted export has a bounded, strongly owned token. Explicit Cancel and
the writer atomically arbitrate its Working → Cancelled / Committing transition.
The writer claims commit after the temporary file is synced and closed, just
before atomic replacement. Cancellation before that boundary preserves the old
destination. Once replacement is claimed, it finishes and reports its actual
success/failure rather than claiming that a written file was cancelled. The
existing directory-sync error behavior is unchanged.

GLB and VRM propagate cancellation through surface extraction, atlas preparation,
attribute/normal transport, encoding and output writing. Large attribute/output
loops check bounded slices; temporary writes use at most 1 MiB per system call.
Cancellation remains cooperative across existing bounded library calls and a
kernel fsync; it cannot interrupt a blocked fsync itself. Optional before_commit
callbacks in export options provide the same arbitration seam to native callers.
The existing two-argument GLB writer wrapper remains available.

## Qualification

The exact source, binary/log hashes, configuration and results are in
[EXPORT-CANCELLATION.json](validation/EXPORT-CANCELLATION.json).

| Final source qualification | Result |
| --- | --- |
| Native export/cancellation/shutdown suites | 3/3 in12.36s |
| File/recovery/serialization/transaction regressions | 11/11 in3.11s |
| Same fresh native/core/support ASan/UBSan/leak suites | 14/14 in33.52s; no suppressions |
| Actual native FLTK/DBusMenu chooser/Cancel/retry | 7/7 checks |

The sanitizer configuration has UI off and XR enabled. These selected native
targets compile the whole core and bundled support with both C and C++ sanitizer
flags; no instrumented frontend or XR renderer run is claimed. The optimized
native application and its renderer/XR consumers were coherently rebuilt for
the actual GUI workflow. The preceding polygon/color full CPU/OneAPI/Arc and
synthetic XR results retain their own source scope in that milestone's receipt.

The native fixture places deterministic barriers after actual file and directory
fsync calls. It covers queued/running GLB cancellation, actual joystick Cancel,
the following accepted Save, exact old-output/temp cleanup, independent retry,
document replacement with continued and explicitly cancelled older snapshots,
late cancellation after replacement, original VRM preservation and real destructor
draining of both running and queued exports. The lower-level test interrupts a
real temporary write after its first MiB, preserving the old file exactly.

The actual FLTK workflow exercises document-owned export choosers, universal
Close, background Cancel, duplicate/Undo/Redo while exporting, ordered Save and
independently parsed retry. A test-only fsync interposer gates only the chosen
temporary output after its real sync. It is neither linked into nor installed
with the application. Captures were inspected. No headset or active desktop
session was launched or restarted.

An initial native test exposed that routing export cancellation through the
internal asset helper would also cancel accepted output during document
replacement. The final implementation uses a separate explicit command and the
retained regression checks both behaviors. An additional older-generation
cancellation check verifies that the new document's status retires correctly.

Public Document/export-option layouts changed. Rebuild all native/controller/
renderer/test consumers together; do not link old objects against these headers.
This cancellation milestone does not complete the remaining sculpt, paint,
humanoid/rig/shapekey, interchange, accelerator or physical-device requirements.
