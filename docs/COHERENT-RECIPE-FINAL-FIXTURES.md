# Coherent recipe regression and final fixture repairs

The fresh coherent recipe candidate completes all 615 registered Release tests
with **613 passes and two failures**. That original result is preserved. Both
failures are test-contract errors; this checkpoint changes no application or
library implementation and does not relabel the original invocation as green.

The post-UV key rendering fixture expected geometric normals, but constructed
flat authored normals on a curved quad and relied on empty morph evaluation to
replace them. The published zero-weight fix correctly retains authored normals.
The geometric fixture now calculates its triangle normals explicitly. A separate
artist-normal case independently checks that key authoring preserves the angle
between authored and geometric normals after repeated UV edits, rather than
replacing artist shading. Both kinds still use the original image tolerances.

The zero-morph Bake fixture compared complete source JSON from two different
archive capabilities. Successful Bake now records an actual recipe, introducing
Scene30 and a null recipe field on untouched source objects. The fixture now
independently captures the actual source/settings recipe to establish the proper
archive context, then compares the complete source, REST mesh and deformation.
Signed-zero persistence, actual worker export, exact Undo/Redo and active-morph
refusal remain checked.

The complete changed consumer translation units pass seven affected commands per
profile plus the separate zero-morph command, in optimized Release and owned
ASan/UBSan. The strengthened graphical test passes 73 checks per profile, and
zero-morph passes ten. These are component continuations over matching frozen
Scene30 application archives, not a new whole build or full sanitizer suite.

The actual fresh coherent Release application also passes native universal-menu
and Bake-dialog tests at normal and compact sizes, and through a private nested
DomeD Wayland compositor. The tests exercise recipe restoration, Unicode cage
selection, Recall, Cancel, edited settings, ordered Save, exact history and clean
shutdown. Active desktop and headset sessions are untouched.

All 168 files emitted by the affected native/render consumers have zero Khronos
validation errors, with every warning and informational message retained. The
Release/sanitizer file pairs are exact. See
[the complete receipt](validation/COHERENT-RECIPE-FINAL-FIXTURES.json) for commands,
timings, scopes, original failures, hashes and validator reports.

The remaining long sanitizer recipe workflow run is separate evidence. This
checkpoint does not finish the full production objective, admit the independent
Scene31 normal-conversion or other agent work, or qualify a physical headset.
