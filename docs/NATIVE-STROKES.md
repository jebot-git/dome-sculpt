# Native desktop background strokes

The existing FLTK Canvas queues camera rays and brush metadata. A dedicated
seventh worker prepares a detached sculpt/paint scene, evaluates picks and
normals, performs mutation and prepares immutable previews outside both the
FLTK event callback and model mutex. The previous explicit synchronous APIs
remain available to tests/CLI callers. This extends the existing application,
menu conventions and rendering path.

## Ownership and ordering

A mouse-down captures the already frozen authoritative Scene associated with
normal mesh publication, the source generation/revision/object, full world
transform, immutable deformation identity and paint settings. No bulk geometry,
BVH or paint preparation runs at mouse-down. The worker makes its own mutable
snapshot. The cached Scene shares the existing COW-frozen mesh-job data, rather
than adding a second flattened volume or mutable alias to the live model.

Every delivered FLTK sample is retained in order; no path coalescing is added.
The gesture budget is 4096 submitted samples. Exceeding it cancels the complete
transaction, restores the committed scene and reports an explicit retry status.
Starting another gesture while an accepted endpoint is finishing also reports
backpressure. Rejected pointer drags do not accidentally rotate the viewport.

Worker previews carry independent session/sample tokens. Meshing consumes their
immutable Scene snapshots; authoritative Scene/history stay unchanged while the
pointer is held. Cancellation invalidates those tokens and restores the prior
immutable render snapshot immediately. Mesh-job cancellation and final source/
pose/transform checks prevent a late preview or result replacing a newer
project. Expensive mutation never owns the model mutex; metadata publication
uses it briefly. The unused whole-Mesh copy in Canvas was removed.

Release drains accepted samples and publishes one history command. Missed,
fully protected and otherwise unchanged gestures create no revision or history.
Escape, focus loss or Undo while pending cancel the gesture, retaining older
history. An accepted endpoint is drained before normal destruction; an
unreleased pointer gesture is cancelled. Seven worker predicate handshakes and
an active native-wait cancellation barrier verify notification ordering.

Save requests made during a gesture wait on that exact accepted endpoint using
a file-worker completion capture. Ordinary Save also captures the existing
immutable authoritative Scene; its metadata snapshot and active-object selection
occur on the file worker. The FLTK application queues other commands until the
stroke finishes, and waits for its Save future before closing. Native
rendered status acknowledges pending commands. Mutation errors cancel deferred
Save/Close commands and keep the original project open with the error visible.
A failure does not silently save an unchanged project and dismiss its status.

## Evaluated painting with original UV identity

The per-gesture immutable evaluated mesh retains original triangle and UV
identity. A MaterialSurfaceQuery supplies bounded visibility-aware picking,
including alpha/cull and original animated source coordinates at each sample's
material time. PaintSurface Texel face IDs and barycentric coordinates map its
original UV coverage onto actual posed/morphed positions and normals. Evaluated
per-tile bounds preserve moved-limb coverage without scanning an unrelated
rest-space brush region. Painting writes only the original UV layers.

Original rest positions, triangle/UV/pin/seam identities, deformation, skin,
morph and retained source data remain unchanged. Mapping rejects a different
surface/topology or stale source. Cancellation checks cover preparation and
paint tile/candidate work, with transactional restoration of prior tiles. A
single cached evaluation is reused throughout the gesture; pose changes require
a new source lease. Current evaluated meshes must retain rest topology; this is
not automatic correspondence for arbitrary remeshed or collapsed surfaces.

## Transaction boundary

Native acceptance stages its History node and state before swapping authority.
Any publication allocation failure restores Scene, undo/redo, content and future
content identities, the source cache, prior published state and original render
snapshot. Recovery is queued only after successful commit and remains best
effort persistence of that committed endpoint.

The common record helper now allocates its history node before changing content
or redo. This alone does **not** repair all generic allocation transactions:
scene-operation publication, UV acceptance, scene_changed_locked and scalar
control callers can swap or mutate authority before constructing History.
Those callers need separate staged-history/strong-rollback work. Native-only
fault evidence must not be relabelled as proof of all global OOM publication.

## Verification and limits

The optimized canonical FLTK282467e build passes native functional,
measured worker-allocation and deterministic seven-worker/active-predicate
fixtures. Functional cases include exact ordered equivalence with the existing
synchronous stroke, cancellation during mutation, stale project rejection,
overflow, one undo/redo, accepted shutdown/file draining, translated and morphed/
skinned evaluated UV painting, native reload and retained glTF rest/skin/morph
export. The allocation fixture dynamically measures the worker and injects
preparation plus late-publication boundaries, checking Scene/revision and
undo/redo/content/nextContent identities directly.

Real native FLTK tests use a preload-only allocation barrier on the diagnostic
`dome-native` thread. There is no application test hook. While actual detached
preparation is blocked, Escape must change native Undo state and Save must
remain usable. Accepted release, queued Save and queued Close are acknowledged
by rendered native status before releasing the worker. A posed painting case
checks actual pointer input, exact source preservation and resulting UV layers.
XSync alone is never treated as client processing or cross-protocol ordering.

Evidence for the private `803479d` plus desktop-stroke source snapshot:

- Release: 61 functional checks; 62 measured allocation boundaries through
  6,754 worker allocations; seven idle and active cancellation predicate gates.
  The complete 90-suite native regression passed in 264.46 seconds. Both real
  OpenXR loader suites passed: the existing 150 checks and the 17 resolution
  checks, in 236.71 seconds combined. A supplementary immutable-preview fixture
  verifies actual changed geometry, held snapshot stability, unmodified
  authority and exact render/scene/history restoration after cancellation.
- Whole-core C/C++ ASAN/UBSAN with leak detection: 15 common suites passed;
  the corrected functional fixture passed separately. The first run exposed a
  test quaternion convention error (the fixture had supplied Z90 while testing
  X90); the product was unchanged. A final supplementary run passes both native
  functional and immutable-preview suites, two of two in 2.71 seconds.
- Actual Intel Arc A770 with the ONEAPI backend: functional strokes, immutable
  previews, deterministic worker shutdown and compute parity all passed, four
  suites in 2.42 seconds. The numerical oracle includes 184 analytic endpoints,
  224 independent CPU-reference batches and 445,088 samples, maximum error
  `8.9407e-8`; device storage and concurrent queue/reuse checks also passed.
- Real FLTK input, native universal menus and software rendering: 12 assertions
  across cancellation, accepted Save/Close draining, posed UV painting and an
  injected preparation failure. A preload barrier holds the actual worker;
  UI acknowledgements arrive while it is blocked. Failure preserves the file
  and keeps the application open. Native protection/symmetry regressions pass
  17 normal and five compact assertions, including actual asynchronous mask
  painting and one exact undo.

Logs live in `artifacts/validation/` in the private validation worktree. The
release all-target build includes fresh consumers of the Document/Paint ABI;
no archive from an older header layout is reused. General universal-menu
regressions pass all 53 assertions, and native skin authoring passes all 11.

Remaining throughput limits include the existing full packed-mesh upload after
geometry changes and 20,000-cell trilinear ray budget. Their failures are normal
status with the original transaction retained. This is not certification of
large-model sustained frame rates. Native pointer samples currently use unit
pressure because the selected FLTK public event API supplies no tablet-pressure
value; the queue accepts explicit brush pressure for future adapters. XR stylus
pressure remains on its existing separate input path. This milestone does not
claim a port of every remaining synchronous authoring command to workers.
