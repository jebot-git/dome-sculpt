# Retopology caller admission — Release checkpoint

Base: `9dd63f90f6062b366276b4ce8192a46892de9e81`. This private checkpoint repairs public `Document::edit_retopology` admission. Shared controller, history and gesture routes are still under audit. Sanitizer and ordinary app qualification remain required before main admission.

The old caller queued Face before its immutable model publication. A real calling-thread publication allocation failure therefore reported refusal while the worker later committed Face. The preserved counter waits for a healthy accepted PlaceMode FIFO barrier, observes one face, and fails the exact draft predicate. Its native exit is **1**, as expected; the measurement orchestrator exits 0. The earlier immediate busy=false observation was not a valid queue barrier and remains historical evidence only.

The repair queues a pending admission token and publishes the busy view before releasing the worker. Caller failure rejects/removes that job and restores the prior view/error/flags/pending count. A held gesture is not physically cancelled before this publication succeeds. Its CancelGesture and discrete command instead execute on one detached worker candidate with one immutable publication. A late worker failure can commit neither half. Only `job.accept` carries a file-owner handoff; an ordinary admission token has no AssetWork and never accesses its null work/changed pointers.

The new registered consumer exercises normal and held Face, MoveMode, Clear and Undo. Every measured calling-thread allocation is denied persistently, followed by an accepted queue barrier, exact original source/paint/pose/meta checks, healthy Face retry, local Undo/Redo, actual Apply with immediate ordered Save, exact reopen and global Undo/Redo. It also injects actual post-enqueue publication failures and the held Cancel+Face late worker publication failure. These are public discrete API tests; they do not establish atomicity of other input routes.

## Actual Release evidence

- New whole product TU and whole test TU: **4,031 checks**, all **152** measured persistent caller boundaries (19 × four actions × normal/held), eight explicit post-enqueue publication throws; native exit 0.
- Matching existing whole worker consumer, relinked with the new product object: **4,308 checks / all 251** one-shot worker denials, **42** persistent error-reporter recovery checks and **97** accepted-publication/file-cancel checks; all three commands exit 0.
- All recorded source/object/archive before/after hashes match at each finite endpoint. Original failures and their binaries/logs are retained under `artifacts/caller-proof/counter`.
- Cumulative new allocated storage at the final worker endpoint is **44,711,936 bytes**, within the initial 64 MiB Release allowance. Reflinked original source blocks are shared and were not rewritten. Actual free space remains above the 20 GiB floor.

`docs/RETOPO-CALLER-ADMISSION.json` pins the exact statuses, binaries and source hashes. Scripts and logs are ignored artifacts under `artifacts/caller-proof`.

## Object and layout boundary

Only `src/document-retopo.cpp` allocates/reads `RetopoAcceptance`, including its new cancellation flag. Compiler layout dumps prove unchanged sizes/alignment for Document (6968/8), EditorState (1896/8), Scene (72/8), RetopoJob (72/8) and the private pointee itself (112/8, using existing tail padding). RetopoJob fields are unchanged. The old whole Document object has only deque-destruction symbols for RetopoJob and no RetopoAcceptance allocation/access; shared_ptr deletion is dispatched through the control block produced by the new whole product TU. Other immutable override objects have no nested-type symbols. This is an explicit matching component proof, not a fresh whole app/core rebuild or public-header byte-equality claim.

## Remaining production work

The source audit confirms separate enqueue-before-final-publication seams in desktop Begin/Sample/End/Cancel, Document Undo/Redo, shared XR commands/hand history and lifecycle/distance paths. They require real counterexamples and a scoped batch transaction before the full caller repair is complete. One candidate must retain exact same-frame/held history; waiting under the model lock, a product sleep, or a later healthy endpoint does not repair a false earlier acceptance. Ordered Apply/file-owner and source/session cancellation semantics must be preserved. The current checkpoint does not exempt any of these routes from the full requirement.
