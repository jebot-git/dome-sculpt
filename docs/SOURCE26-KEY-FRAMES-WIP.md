# Coordinate-aware geometry keys — private continuation

This child of the frozen Source24/26 integration is not admitted to main. The
full production goal remains active. It replaces the retained-frame UV2 key
refusal with an actual coordinate-aware target factory. It has no new public
layout or archive version. The original integration/full regression stays in a
separate worktree.

The geometric change of basis uses each role's captured origin coordinates:
explicit generated_uv provenance first, then the retained PBR or versioned
MToon coordinate selection. Native-aligned owned arrays are checked against the
captured original primitive/source-vertex map when present. Only a detached
geometric mesh selects these coordinates for tangent computation. Authored
UV0/UV1/extra sets, raw source, base artist normals/tangents and their provenance
remain unchanged. Geometric frame caching includes coordinate identity, so
aliased artist frames for distinct latent PBR/MToon roles separate when needed.
Copies and mixes retain existing aliases.

Fresh coherent Release core/render and five selected tests have passed in
separate invocations: three existing shapekey regressions, 42 new analytic/native/
controller assertions, and 385 graphical assertions. The independent triangle
UV-Jacobian oracle has normal/tangent errors no greater than1.2e-7, against the
preserved old wrong-UV0 errors0.22933/0.764433. Native workers, ordered Save,
reopen, exact Undo/Redo/Restore, cancellation, controller strokes and focus-loss
cancellation are exercised. All72 parsed emitted GLB cases (UV2..7, provided or
generated tangents, mixed-joint pose and weights0/.35/1, Replace+Mirror) match
native pixels within1; deliberately sampling UV0 fails. Upload reuse is checked.
No current sanitizer, A770, Khronos or GUI claim is made for this child yet.

Two initial fixture negatives remain in artifacts: replacing an imported root's
rest scale3 with absolute1.1 reduced visible coverage to80 despite exact pixels;
relative scale factors preserve794 or more covered posed pixels. Setting a
controller aim before its first tracking acknowledgment used the old canvas.
The positive sets aim after that acknowledgment. Bounds and product code were
not changed for either refinement. A missing test include was separately fixed
after the first build terminated.

A separate next prerequisite is genuine native frame capture for a mesh with
owned UV2 normal maps but no retained frame set. The older native minting path
still chooses UV0; this child is not safe for general integration until that
path has its coordinate-aware factory and pre/post-capture evidence. Curved
raw derivative sampling, current normal-map transforms, independent seam-key
expansion, broader prepared-domain interoperability and typed UV2 remain
required work. Existing before-state-only UV2 and prepared active UV0 positives
are retained; no averaging of contradictory source-vertex edits is introduced.
