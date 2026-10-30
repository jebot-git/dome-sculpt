# Transactional document publication

This follow-up to native worker checkpoint `17e540d` hardens structural edits,
scalar scene-control acceptance, accepted asset and UV work, Open/replacement, Save
status/path/content updates, and document Undo/Redo. It preserves the seventh
ordered native worker. It does not claim that every independent kernel or
synchronous explicit stroke API has been made universally allocation-proof.

Publication prepares its EditorState, immutable mesh-job/source cache and UV
validation job before any candidate becomes visible. The model lock protects
candidate preparation; worker predicate locks protect the final non-allocating
queue handoff. No accepted recovery checkpoint is queued before publication
succeeds. A failed publication preserves the prior published state and source
cache without transiently exposing rejected geometry.

A private endpoint transaction retains source Scene metadata, input/control
state, document identities and the authoritative cache. History allocates its
new node before moving aside redo. Failure restores undo/redo, content and
nextContent, revision/generation, source cache and live control ownership.
Undo/Redo stage detached Volume/PaintedMesh histories and destination nodes;
only after publication succeeds do private non-allocating member swaps update
the existing shared history target. Source-history logical availability is
computed for the final committed stack. No complete undo/redo deque copy is
introduced for ordinary edits. Synchronous Undo still stages a bounded copy of
the selected internal stroke history; this change does not certify sustained
large-history UI latency.

Accepted asset/UV commits keep draft ownership until all publication allocations
succeed. Destructive worker invalidation happens after success. Failed UV paint
transfer/publication retains its geometrically valid draft; preparation errors
are reported without incorrectly labelling the UV geometry invalid. The same
retained draft can be applied again. Scene add/duplicate stage complete objects
before consuming identities or publishing partial entries.

Native Begin also holds its queue predicate through guarded publication. A
rejected Begin retains no active gesture. Native Save prepares a finishing state
but releases the accepted-End predicate only in the final non-allocating
publication handoff; an admission failure retains an accepting, cancellable
gesture. Unused native tickets remain monotonically increasing, so cancelled
work cannot acquire a reused token.

Asset, UV, preparation, Open, Save, Export and recovery request publication holds
its predicate mutex through the staged state publication. A rejected job cannot race out of the queue. Open includes its
adopted filename in the replacement transaction. Save writes an immutable
committed capture atomically before changing in-memory saved-content/path
metadata; failed status publication may leave a valid saved file, while the
original dirty in-memory state remains retryable. Recovery source changes,
discard and checkpoint enqueue follow model success and remain best-effort
persistence; filesystem failures do not roll back successful user edits.

Validation uses linker-wrapped private endpoint boundaries and a thread-local
C++ allocator instrument, with no runtime application callback or test mode.
Structural/scalar/Undo fixtures instrument the complete synchronous call;
request fixtures instrument preparation through successful admission. Every
measured pre-swap/publication boundary is injected individually. Post-commit
best-effort persistence has an independent fixture: its failures must preserve
the accepted edit and one undoable history transition, rather than incorrectly
rolling back published work.

The initial seven optimized fixtures passed all 452 measured boundaries; a
larger sculpt-history stress case subsequently passed all 8,136 allocations.
The current bounded sculpt fixture retains independent real changes and every
exact rollback oracle. Recovery uses independently written durable sources;
failed preparation/refresh/restore are retried through their actual APIs.

Final validation of the complete private source snapshot:

- Fresh optimized Release build of all targets; **114/114 native suites** pass
  in 227.45 seconds, including all 91 baseline suites and 23 transaction scopes.
- **1,274 pre-commit/admission allocations** are injected individually; **32
  post-commit persistence allocations** independently prove that accepted work
  remains applied and undoable. The larger earlier sculpt stress fixture also
  passed all 8,136 measured allocations before the bounded final fixture.
- Whole-core C/C++, including MikkTSpace/xatlas, ASAN/UBSAN with leak detection:
  **35/35 suites** pass in 150.00 seconds. This includes the expanded fault
  matrix, native mutation/preview/allocation, seven-worker shutdown, controls,
  UV transfer, files, recovery, resolution and Retopo/Cut cancellation.
- Both real OpenXR loader suites pass: **150 existing checks plus 17 resolution
  checks**, 192.09 seconds combined. These use the test runtime, with no headset.
- Real FLTK/X11 input and DBusMenu host: **12/12 native responsiveness,
  Save/Close, posed painting and failure assertions**, plus **53/53 general
  menu assertions**. The final failure screenshot was inspected: the original
  project remains visible and its error status readable.
- Application headers and runtime use canonical FLTK `282467e`; Release uses
  `-O3 -DNDEBUG`, and toolkit `lib64` is selected explicitly at runtime.

An earlier broad run exposed a fixture race: it captured the control baseline
before the already accepted material preview completed meshing. The fixture now
waits for actual readiness; its exact pointer/state/source checks are retained.
Recovery admission fixtures likewise await the recovery predicate explicitly,
and distinguish admission from best-effort post-commit persistence. Logs and
hash manifests are in `artifacts/validation/` of the private worktree.

These scoped failures do not certify behavior under permanent process-wide
memory exhaustion or make every independent synchronous kernel transaction
allocation-proof. Native mutation and accepted file/UV preparation remain on
their existing workers. No physical XR session, installation or display restart
is used for this validation.
