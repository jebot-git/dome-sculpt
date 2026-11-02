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
