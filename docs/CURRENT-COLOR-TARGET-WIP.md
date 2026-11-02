# Current-corner target preparation — private foundation

This branch starts from committed `3042a86`, including the combined Scene19
normal UV authority and controller rebind fix. It adds a geometry-only immutable
`PolygonColorSurface` descriptor to color preparation. Receipt version2 uses the
explicit `PolygonCorners` kind; Identity and original-accessor versions retain
their old meaning. Material archive6 and polygon archive5 share one bounded
corner-field pool under Scene20, so an active authority and its projection regain
the same field/basis owners after decoding. Retained source roots, affine
capsules and normal-UV before-state recursion remain separate and intact.

A fresh coherent regular-user Release core compiles. Fifteen native checks pass
for real preparation/analytic quality, exact input retention, same UV/editable
layers, exact restart, shared owner identity, old-version/unreferenced-field
refusal, cheap replacement-owner refusal, wrong REST/UV rejection and actual
cancellation. These checks are a foundation proof only. No sanitizer, graphics,
FLTK/controller or full regression qualification is claimed at this checkpoint.

The current-field target Bake guard is deliberately still present. Next work
connects the same-mesh strict conversion and guarded native worker, validates the
actual rendering/picking/export route, preserves one Undo/Cancel/Save and adds
persistent-failure/adversarial tests. UV/topology/artist-frame transfer remains
refused until its own real current-field remap is implemented. No fake creator
appearance or retained-Origin inference is used as current color authority.

## Subsequent actual workflow checkpoint

The private implementation now connects the strict same-mesh bridge and actual
native Bake worker. Source field/basis, PA Origin and shape capsules survive one
Undo/Redo, Save/reopen, repeat Bake and native Restore. Forty-eight native checks
pass, including empty authority/retained-only Scene20 feature restoration,
malformed owner/UV/history, orphan records and material-node quota. Shared material
reachability is preflighted before constructing paint SurfaceMaps.

Actual FLTK9, compact9 and synthetic OpenXR11 checks pass on interim workflow
binaries. The independent software consumer passes357 checks over32 combinations
of alpha mode, seams, prior paint and skin/morph pose: actual256-resolution native
imports have original-to-prepared image maxerror1; both prepared and baked GLB
matrices are exact. Default64 is independently sampled against its declared
linear projection bound and its exported/native pixels match exactly. A strict
.001 bound on the default64 input refuses rather than silently accepting lower
quality. A universal two-encoded-level approximation claim for64 is false (the
initial provisional image oracle measured8), and is not made here.

These are still separate incremental proof scopes, not final qualification. No
fresh target sanitizer/OOM or hardware batch has completed yet. The inherited
Bake caller has a post-acceptance fallible publication gap; it is being fixed and
qualified in a separate public04-based branch before integration back here.
Current-field UV/topology/prepared-normal/artist/UV1 transfers remain explicitly
guarded. The target shader consumes the prepared field once; no fake creator
appearance or borrowed original accessor is introduced.
