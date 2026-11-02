# Accepted UV transfer cancellation

This checkpoint fixes the existing source-aware UV Apply worker on public
`824f6cdb6657f6b6a9f240782935e7d46195efcd`. It adds no public fields, archive
versions, normal-preparation support or new frontend.

An accepted UV transfer previously set `asset_busy` without installing an
`AssetWork` token. Consequently File/Cancel and controller Cancel Work could
leave the transfer running and publish its result after cancellation. The
existing generation, source, session and final publication checks did not
observe that user cancellation.

UV admission now creates a token before changing the queue, publishes it in the
existing model transaction, and passes its cancellation flag through the real
transfer. Success and failure retire only the matching token. Admission and
publication retain the existing transaction rollback and immutable source
checks. A cancelled transfer leaves its original scene/history and a ready UV
draft for retry.

`tests/uv-worker-cancellation.cpp` holds the actual source-aware transfer at a
deterministic linker-wrapped boundary. On the unmodified public product, the
same fixture fails because Cancel still publishes the changed scene. With the
fix it checks internal asset Cancel, explicit background Cancel, exact source
preservation, token retirement, retry, Save/new-document reopen, one Undo/Redo
and stale document replacement. The gate releases before document destruction
even when an assertion throws.

The original eleven-check fixture and its failed baseline log are separately
hashed. Four appended checks close a committing UV draft while the transfer is
held. Its generation stays unchanged, but its busy token remains installed:
asset edits, Prepare, Open and asset-draft admission all refuse newer work
until that worker finishes. A new asset job succeeds after cleanup. Thus the
tested public admission paths cannot install a newer same-generation asset
token before the old UV failure handler runs; this fix does not widen the
generic failure-handler policy.

Validation and exact source/binary/log hashes are recorded in
`validation/UV-WORKER-CANCELLATION.json`. This is a native CPU worker checkpoint;
it makes no new GUI, physical headset or GPU qualification claim. The complete
production goal and the separate Scene19 normal-UV reprojection work remain
required.
