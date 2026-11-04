# Source-aware atlas regression migration (work in progress)

Base runtime: c09202fbf092eaa4d9ddf8d9a2249fe7bf013e74. This child changes tests only.
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
refusal remain required. No tolerance or runtime ownership guard is changed.

The current-corner domain test previously tried to fabricate UV1 ownership by
replacing serialized baked maps. It now requires refusal at that actual boundary,
exact caller preservation and a successful valid current-colour UV Bake/adoption.
General current-corner plus genuine Source4 UV1 authoring is still required; this
negative does not implement or qualify it.

Qualification pending. No new main, full-suite, GPU, device or installation claim.
