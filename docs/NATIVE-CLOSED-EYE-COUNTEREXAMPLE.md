# Native closed-eye counterexample (private)

Current codef5/b973 passes both actual nineteen-part native humanoids, but a
procedural current-semantic-role stress fixture exposes a remaining geometric
gap. Native head/leftEye/rightEye controls have nonsemantic bone/object names;
no source asset or skin values are present. Actual public binding preserves full
paint/UV/reference/placement and saves/reopens each accepted field.

Per Release and wholly owned O1 ASan/UBSan/leaks, eight controls pass: two actual
eye disks and two opposing annular frames with0.1mm separation, under identity
and unrelated full affine/shear/reflected placements. Independent analytic rigid
posed points keep the unchanged3%max/1%mean local bounds. The same actual method
incorrectly assigns a closed octahedral eye shell to Head, with foreign weight1,
local maximum11.47%/14.50% and mean7.65%/7.17%. Both failures remain; no bounds or
default are changed. Exact closed-manifold interior eye-pivot evidence is the
next proposed repair, independently of open projected-disk evidence.

A missing JSON include was a compile-only negative. The initial test used xyzw
for the native wxyz quaternion and falsely failed disk pose controls; that input
and report are preserved separately. Correct wxyz input closes all disk/frame
controls while retaining the genuine two closed-eye weight and pose failures.
