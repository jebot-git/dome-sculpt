# Analytic finite-bone energy and exact shadow intervals

Private component child of c2912f6. The parent keeps actual certified whole-group
fields that fail Body/Tops quality and the later500M integral-support refusals.
No cap, solver/deformation tolerance, source sampling density or public binding
behavior changes. These new kernels alone do not produce/admit weight fields.

`bone-line-integral.hpp` evaluates the normalized physical-source parameter
integral of `mass/(|head+t*(tail-head)-row|^2+mass)` on a supplied interval. The
cross-product identity computes its positive quadratic determinant without
subtracting near-equal squared terms; a direct atan2 identity computes the angle
difference without cancellation between near-equal arctangents. A true point
source has the isotropic point kernel. This is the mathematical limit/discrete
source energy, not an anatomical/visibility or source-owner certificate. Invalid,
nonfinite and unrepresentable metric/angle ranges refuse. Work/cancellation are
bounded and caller geometry is never mutated.

Independent long-double32-point Gauss-Legendre integration uses spatial bands
around closest approach to resolve narrow regularized peaks, with no reuse of
an antiderivative.512cases cover on-line/point/short-source/partial intervals
and13mass orders. Release and own standalone ASan/UBSan/leaks pass: absolute
7.85e-17, relative1.80e-14; split/reverse/scale/rotation/translation1.12e-16.
Invalid/range, actual work and Cancel controls pass. There is no ray/field or
whole-solver SAN claim from this arithmetic proof.

`bone-source-shadow.hpp` receives binary32 geometry and an oriented triangle.
For a row in front of the triangle, an entry-wall shadow is the intersection of
four affine determinant inequalities in the actual finite-bone parameter:
source behind the face plane and the three ray/edge barycentric halfspaces.
The result is an **exact rational interval**, not finite point samples. The
existing bounded dyadic Arithmetic supplies exact determinant magnitudes and
fraction comparisons; the common determinant scale is cancelled in each ratio
before cross-products. Its existing10000m/precision bounds remain. No new
arbitrary geometry tolerance is introduced.

Coplanar overlap, unowned origin contact and isolated grazing are distinct
explicit ambiguity results; no ambiguous path is declared clear. Ordinary
coplanar disjoint fans are distinguished through exact planar clipping and
containment. The helper is geometry-only: owning caller must prove source/mesh/
incident-corner correspondence and account/query the complete triangle set.
It does not weld foreign coincident positions, establish volume membership or
authorize ignoring a material/source owner. Oriented entry-wall policy is
explicit; orientation-independent open-source admission is not claimed.

512small-dyadic random cases match131584 independent long-double division-free
Moller-Trumbore point-ray checks. This oracle uses bounded dyadics whose products
fit its mantissa. Exact bone reversal and1024rotation/translation/scale cases
match; scales2^-100 and2^10 retain the analytic1/6..5/6 wall interval exactly.
A separate wall has a nonempty interval missed by both endpoints AND midpoint.
Explicit coplanar, grazing, origin-contact and real work/Cancel guards pass.
Release+own standalone ASan/UBSan/leaks pass, with all native code under test
instrumented; no linked core/archive or GPU is involved.

Next is the bounded owning whole-group mesh query, complete shadow union and
conservative rational-to-floating integration endpoints, then actual Body/Tops
fields under500M and the unchanged pose limits. It must not use endpoint-only
visibility, hidden coarsening, scalar tuning, borrowed artist magnitudes or an
error fallback. Pure exact kernel is retained as a control for any adaptive
filter. Primary adaptive-predicate reference inspected for that next filter is
Jonathan Shewchuk's official public-domain predicates.c, source SHA
f8662c3f407d1c1c5dcd4dd49ea8b8ddd801a71827d65734206ddc3741792029;
no filter/source code from it is yet embedded in these kernels:
https://www.cs.cmu.edu/~quake/robust.html

Actual full38-part poses/stress, independently verified source-assisted versus
user/native intent, complete solver SAN/persistent faults and native Undo/Save/
UI remain required. Current fields still fail; this component is not completion.
