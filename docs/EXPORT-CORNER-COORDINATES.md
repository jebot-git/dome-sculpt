# Private corner-coordinate layout foundation

An existing full normal-frame split can still share a current vertex between
corners with different retained paint UVs. Following only rest_vertices would
lose that seam. The detached split_frame_corner_coordinates helper refines an
already authenticated full frame layout using exact ordered-corner coordinate
bits. Its output retains REST vertex and representative corner maps, triangle
order and every prior base/target/role split. It neither averages coordinates
nor rewrites authored UV0 or original UV1.

This helper supplies data layout only. An actual caller must independently
validate the owning REST/paint-domain witness before using it. The output does
not turn an old attachment into authority for newly expanded geometry. No
exporter, renderer, codec or guard is changed by this checkpoint.

Thirty optimized and thirty sanitizer assertions pass, including all ten
measured persistent allocation failures, exact retry, early/final cancellation,
malformed/range/nonfinite inputs, work/retained-frame quotas, independent UV
seams, secondary-role target splits and analytic anisotropic N/T morph/skin
formulas. The new inline helper/test TU was compiled against the unchanged
f822 owned coherent core/support/renderer archives; this is a native helper
scope, not a full-suite or graphical-consumer claim.

The Paint24 owning adapter and complete export integration remain required.
Genuine native retention/reimport of more than two source UV sets is the next
assigned stage; the known compiler UV2 refusal is preserved. Exact hashes and
proof scope are in [the receipt](validation/EXPORT-CORNER-COORDINATES.json).
