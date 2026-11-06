# Retopology markers at the default compact view

The shader keeps a retopology vertex marker at least two physical pixels in
projected radius. Larger physical world markers keep their original positions.
Only marked endpoint XY can grow: the physical endpoint clip depth/w, existing
bias and source depth test remain. Projection uses the actual GL viewport and
an algebraic projected delta to avoid subtracting nearly equal NDC coordinates.
Edges, zero projected axes and near/behind-eye clipping retain their original
paths. No geometry readback, public field, layout or persisted state is added.

A complete new render.cpp object per profile links before the unchanged render
archive. All headers are byte-identical to a852; frontend objects, the exact five
qualified retopo/document objects and core/support archives remain unchanged.
Release has an ordinary app plus the complete renderer consumer. Sanitizer uses
one explicitly test-composed ELF to fit storage: the unchanged app entry and
whole renderer-test entry are selected by a small GNU main wrapper. The test
entry has a real body and explicit return; normal GUI arguments call the actual
unchanged app main. This sanitizer ELF is not described as an ordinary product
executable or a fresh whole-application build.

Both profiles pass 55 new marker scenarios and every existing renderer assertion:
48 quarter-pixel states over three viewport sizes, near/far views, larger physical
markers and homogeneous clipping fallback; retained geometry cache, stereo,
selection/revision changes, GL-state restoration, source occlusion, XR linear
composition, bitmap menus and ordinary PBR/baking checks remain. All 30 captured
renderer image pairs are byte-identical across profiles.

Actual native clients pass 15 normal and 16 **unzoomed** compact checks per
profile, including held-point pixels, exact Escape rollback, Apply, Undo/Redo,
Save, fresh-process reopen and clean exit without suppressions. Both native
output pairs are byte-identical. The held center-point regression uses the same
view and input that failed before. Subsequent quad corners use the visible
compact source interior; the historical 70-pixel corners were outside that
unzoomed source. No camera zoom is used to hide the marker failure.

The old failure remains intact. Real input telemetry showed a ray hit, valid
normal, positive pressure and one selected vertex while the fixed world marker
produced no visible pixels. The new positive does not reclassify that failure.
Actual renderer maps now show the private patched software EGL/gallium closure;
GUI maps show canonical bitmap/FLTK and installed software GLX modules. Those
lazy GLX digests also match the sealed a852 before-ledger predating this build.
Source, object, archive, library and executable scopes are explicit in the
[receipt](validation/RETOPO-SCREEN-MARKERS.json). No GPU, physical HMD, installation,
full build or whole-suite claim is made.

This private marker checkpoint is not main admission. A separate read-only review
found an additional held-gesture caller-allocation ordering gap before accepted
retopo handoff. The earlier 63 allocation prefixes cover completed drafts. That
repair and its semantic application integration remain required before publishing
retopology to main. The full authoring objective remains active.

The exact Release renderer/test objects subsequently pass all unchanged marker
and legacy assertions on observed Intel Arc A770, PCI 0000:03:00.0. The separate
`tests/retopo-marker-device.cpp` observer forwards glGetString unchanged and
requires the actual current EGL display's device/render node, char-device rdev,
sysfs vendor8086/device56a0, and open FDs on renderD129. Four actual FDs, installed
Mesa/i915 and immutable mapped-driver digests are recorded. All 30 hardware PPMs
remain available. The **same binary** with forced llvmpipe reaches its actual
renderer query and refuses with exit1; this is an identity negative, not an
argument or setup failure. No pixel or source bound changes. The finite cohort
closes0/drift[] and explicitly releases its GPU lease. No sanitizer hardware
duplicate, compute/baking or physical-headset claim is added. The portable helper
is a manual hardware-link seam, not a default software CTest requirement.
