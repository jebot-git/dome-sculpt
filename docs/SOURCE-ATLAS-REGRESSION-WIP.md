Latest scoped qualification: [SOURCE-ATLAS-REGRESSION.md](SOURCE-ATLAS-REGRESSION.md). The pending states below are preserved historical investigation notes.

# Source-aware atlas regression migration (work in progress)

Base runtime: c09202fbf092eaa4d9ddf8d9a2249fe7bf013e74. The initial d09 test-only child exposed a real empty-raster stroke admission gap.
This continuation changes only the private shared paint-admission predicate,
plus the tests; public layouts and serialized formats are unchanged.
The original combined Release suite is still running on frozen original inputs;
its five observed failures are preserved rather than overwritten or renamed.

The initial source-paint adapter retains original images, samplers and logical
coordinates together with an independent writable paint grid. Old tests expected
images resampled into the grid and Scene18/material reparameterization. Tests now
check exact original owners, genuine native painting/history/restart, source-aware
Scene28 data and explicit finite export. The source-coordinate oracle compares
raw imported accessor coordinates with actual factory remaps at destination-grid
interiors and checks wrong-domain sensitivity. It is a coordinate/sampler oracle,
not a claim of rendered pixels or a resampled image.

The graphical workflow separately requires exact raw/prepared native pixels,
independently parsed ordinary GLB output, explicit finite PBR output against a
detached finite material view, and exact native package recovery. Finite view
loss relative to the original is reported and is not declared lossless. Existing
source-paint-bake minification counterexamples and strict ordinary painted-export
refusal remain required. No numerical tolerance or ownership guard is weakened. The admission predicate
now also rejects retained source domains with no covered texels and unpaintable
faces, using the existing native preparation notice before a stroke/history
transaction can start. Ordinary covered no-image paint fields remain supported.

The current-corner domain test previously tried to fabricate UV1 ownership by
replacing serialized baked maps. It now requires refusal at that actual boundary,
exact caller preservation and a successful valid current-colour UV Bake/adoption.
General current-corner plus genuine Source4 UV1 authoring is still required; this
negative does not implement or qualify it.

Qualification pending. No new main, full-suite, GPU, device or installation claim.

The first exact d09 run compiled all six complete test TUs successfully. Its
retained negatives identify the empty-raster admission gap and a test assumption
that all sibling materials must be flattened. The actual package correctly keeps
an unpainted sibling's original UV1 while compiling only the painted material.
Four other updated workflows pass, including the 12-case graphical workflow;
these intermediate results do not qualify the new admission continuation.

The f89 direct caller and native zero-coverage regression passes in Release and
sanitizer. Release16 finishes14passing, with a genuine asynchronous test error
(no surface wait after Undo/Redo) and another forged-UV owner fixture failure.
A separate diagnostic reproduces both normal and flat cases: before waiting,
`begin_native_stroke` correctly explains “Wait for the current surface before
painting”; after the existing public `wait_mesh`, the same gesture succeeds.
This child adds that missing wait and updates the second ownership negative,
while retaining its actual subsequent mixed retained-paint/raw-UV2 workflow.
The twelve finite views observed max1; their existing mean/interior2 nominal
bound is now asserted as well as independent compiled-view/recovery contracts.
These fixture bounds are not a global finite-sampling guarantee.
