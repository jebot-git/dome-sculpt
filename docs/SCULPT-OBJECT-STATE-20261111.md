# Sculpt material submission repair

Candidate49465f29 submits ordinary sculpt material/shader/raster state once per
object, then draws its immutable cell-ordered runs with their actual chunk VAOs.
Previously every run repeated that identical state. The ordering of objects,
triangles and draws is unchanged, including Blend/ghost and GL_LESS ties. Packed
PBR/MToon/source-field ranges retain their existing paths. No geometry merging,
CPU flattening, source publication, paint change or new GL capability is added.

The current full P1 Arc baseline exposed high CPU cost in unchanged stereo
views despite zero geometry/scalar uploads and sub-millisecond ray/contact work.
A recorded main-thread stack sample was inside glUniform1i in the cell-run loop.
That observation motivated this repair; it is not a statistical attribution of
all frame time. Concurrent host builds also affected elapsed times. The sample
briefly observed the target and is retained with an explicit timing caveat in
`artifacts/sustained-current-20261111/stack-samples`.

The regression consumer counts actual base-color uniform queries while retaining
all original72 pixel/ownership/scalar-failure controls. The unchanged renderer
passes its independent pixel oracle but fails the added submission bound:
581 setups for3 actual objects/ranges. The repaired renderer uses3 in every
opaque/Blend/Mask and reflected/sheared variant. Six added checks bring that
consumer to78 controls. This measures state submission, not an FPS speedup.

Both optimized Release and strict ASan/UBSan/LSan pass the five software GL
consumers,1733 checks perprofile. All94 PNG/rawRGBA files perprofile are
byte-identical to the corresponding original currentb8 outputs. All94
Release/SAN files are also byte-identical. Sanitizer runs enable leak detection,
halt on undefined behavior and use no suppressions.

Only the complete changed renderer and regression-consumer translation units
were recompiled perprofile. Seven consumers, including the ordinary application
and sustained-workload executable, were relinked against those and the exact
unchanged frozenb8 providers. The new archive is thin and names those persistent
objects. Compiler/link recipes preserve original options, with all output and
dependency paths redirected privately. Every old owned provider and the changed
source tree is checked before/after. This is explicitly incremental component
qualification, not a fresh whole-source build. Read the
[scoped receipt](validation/SCULPT-OBJECT-STATE-20261111.json).

Actual Arc, ordinary app/XR and full sustained-workload comparison of this change
remain pending. The original full P1 baseline is still running with all planned
counts and its shared GPU lease. No hardware performance improvement, main
admission or complete-goal claim follows from the software controls. Sources,
commands, objects, linked ELFs and raw outputs are retained under
`artifacts/sculpt-object-state-build-20261111`.
