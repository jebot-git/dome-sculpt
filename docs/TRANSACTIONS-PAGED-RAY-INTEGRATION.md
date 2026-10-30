# Transaction, paged picking and retained-frame integration

This candidate combines root `0c9a44e` (paged storage, retained normal frames,
ordered seven-worker desktop input, static COLOR_0 and CUDA qualification) with
published `8e206fc` ancestry, paged picking `6c64c5d2`, and transaction hardening
`13556e5`. The complete production objective remains unfinished. Source import
`13b5156` and the parent's procedural sculpt work are separate subsequent work;
this candidate does not silently include or claim their proof.

The published ancestry merge is `f488963`; the ray foundation merge is
`9f54028`. The generic transaction merge and new integration tests form the final
candidate checkpoint. Older milestone documents retain their exact historical
source and validation scopes.

## Semantic preservation

The complete `Document::mesh_loop` body has exactly the `0c9a44e` SHA-256
`0c5410f115af75399f108ce0c2bea983e2d25a9f914865e6ed67100bb3a8461a`.
Original/canonical normal frames, tangents, static linear source colors, source
ranges, geometry identities and native preview tokens are retained. Scene9,
material archive2, and all original-source/canonical-authority refusal guards
remain unchanged. Runtime preview fallback is not promoted to export authority.

Volume's 23 state members are exchanged by the private complete-state swap:
geometry history/start; mask/settings and optional gesture starts; gesture flags;
layers/active/next identity; before-delta; voxel/physical band; cells/before;
undo/redo; history limit/bytes; editing/revision. Paged `before_` and
`before_delta_` are full CowMaps, whose swaps exchange both immutable data and
empty-provider owners. Every member swap is statically required to be noexcept.
There is no new Volume data member or archive change. The ray index owns only
minimal SDF storage plus voxel/band context, not mutable Volume/layer/mask state.

The old three-argument picker API remains. The native worker uses the additive
four-argument overload with its real cancellation predicate: stop, session
cancellation, epoch, generation, revision and active-object lease. Accepted End
still drains before ordinary shutdown; an unaccepted gesture is cancelled.

## Integration-specific tests

`native-paged-ray-cancellation` gates actual indexed work after a reduced cold
page, 256 hierarchy visits or 256 candidate cells. All three phases exercise
Cancel, stale replacement and unaccepted shutdown: nine scenarios and 90 checks.
A linker observer wakes only on the real native predicate notification; no
manual barrier release is used for successful cancellation. It proves exact
Scene/history/render preservation, immutable held owners, no partial cold cache,
and reusable warm indexes. Existing native accepted-shutdown tests remain.

`transactions-paged-state` has 27 checks over a real resolution-converted
Volume4, retained physical band, hidden/locked/strength/active layers, global and
sparse masks, symmetry, protected mirrored edits, Undo/Redo, authenticated local
ancestry, independent mask owners, later cancellation, native save/reload and
the legacy ray API. Original and edited strong owners stay immutable.

The measured native allocation fixture now primes global cache bookkeeping
capacity and then clears the index before every case. Each measured/faulted
worker still builds a **cold** index. This prevents prior LRU growth from moving
late-allocation boundaries; all exact rejection/history/redo/source oracles are
retained. It passes 59 selected worker boundaries through 601 allocations.
An initial context-dependent unhit boundary is retained in the earlier log.

## Completed combined validation

The final regular-user optimized build compiles every target against the current
headers and core, with canonical FLTK282467e include paths and explicit toolkit
lib64 runtime. There is no reused archive from a different Volume/Scene ABI.

- CPU/software GL: **131/131 native suites**,385.03 seconds; both real OpenXR
  loader suites, **150+17 checks**,176.15 seconds. JUnit artifacts retain the
  complete final runs.
- Complete own C/C++ core, MikkTSpace/xatlas, renderer and native application
  compile with ASAN/UBSAN. **52 unique scoped suites pass**, with leak detection
  and halt-on-error. The initial51-suite run passed50; the new dense native ray
  setup hit its inherited10-second meshing wait under instrumentation. Its
  explicit60-second actual-ready predicate retains all90 cancellation oracles;
  that corrected case plus the added indexed allocation contract pass2/2 in
  184.65 seconds. No product or sanitizer defect was found. Scope includes all
  transaction/native/ray/paged tests, frame archives/runtime/creator/UV/export,
  independent frame/color consumers, source-UV guards and Document→GL reuse.
- Actual Intel Arc A770 with a fresh full ONEAPI build: **21/21 selected native,
  transaction, ray, frame and compute suites**,29.95 seconds. The compute oracle
  covers184 analytic endpoints,224 CPU-reference batches and445,088 samples,
  maximum error `8.9407e-8`. No CPU fallback is substituted.
- Six additional actual-Arc GL suites pass: original-frame renderer47, GLB
  consumer25, VRM consumer23, source-color renderer100, source-color VRM41 and
  actual Document/buffer reuse22 checks. Hardware renderer strings and pixel/
  upload assertions are recorded, rather than inferred from build success.
- Real canonical FLTK/X11 plus universal-menu host: resolution15 +compact4,
  native worker responsiveness/Save/Close/posed-paint/failure12, general-menu53
  assertions pass. The status-observer fixture awaits a genuine indexed draw
  of the loaded field's exact index domain before sending the pointer gesture;
  its precise budget-error, no-modal, source and working-Save oracles remain.

Two inherited fixtures needed contract/readiness updates. Cold indexed picking
now deliberately allocates, so the allocation fixture separately injects and
propagates cold `bad_alloc` while preserving exact source/protection, then proves
zero-allocation warmed picking and all91 measured resolution fault boundaries.
The earlier GUI attempt correctly displayed “Wait for the current surface” while
asynchronous meshing was pending. The test-only observer forwards real GL draws
and real FLTK labels; it adds no application API, replacement input or arbitrary
startup delay. The successful status screenshot was inspected.

The23 generic transaction scopes inject1,307 measured C++ allocations, including
34 separate post-commit persistence boundaries. Exact original source, painting,
history/redo/content, cache and draft assertions remain; accepted persistence
failures keep the successful edit applied and undoable. The corrected native
cold-worker fixture passes59 selected preparation/publication boundaries across
601 allocations. Original failed attempts and terminal successes are retained
in `artifacts/validation/`, with exact source/log/binary hash manifests.

This checkpoint includes neither the later qualified import nor procedural
feature branch. It does not certify permanent process-wide OOM, all independent
kernel transactions, NVIDIA execution, physical headset pacing or the complete
production goal. No headset, system installation, display restart or external
push is performed for this candidate.
