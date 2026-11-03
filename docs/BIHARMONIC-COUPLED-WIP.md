# Actual fixed-inverse coupled route: preserved cost negative

Base9b7af75 is preserved. This native private factory uses one unchanged sparse
Dirichlet factor for the coupled convex-dual problem. Every inverse application
checks the original matrix-free Q residual, with bounded refinement; each dual
step certifies quadratic majorization by backtracking. The final field must
independently satisfy original primal stationarity, nonnegative dual KKT and
partition of unity. Component-specific source-assisted handle sets stay separate;
there is no cross-owner/source-value copying or native/public admission.

Independent original gradient-FEM/principal2x2 inverse (2.75e-16 residual),
open/disconnected45/90 fields, unchanged inputs and unseeded refusal pass.
All128 independent primal QP oracles also pass deliberately1e-8 undersized
initial step bounds. Final field certificates precede the existing explicit
max4/prune/float SkinWeights storage policy; no independent box field is used
as fallback for a failed coupled solve.

Actual original500M cohorts complete1538input hashes unchanged, but only13/19
female and12/19 male parts solve; Body/Hands/Legs/Tops/Bottoms/Shoes exhaust
work, maleTie misses the unchanged4096 iteration bound. All solved-part poses,
seams, consumers and original Scene encodings pass; many required auxiliary
outputs remain absent. This is not a usable whole-avatar or native UI feature.
No whole solver sanitizer/allocation qualification is claimed.

The verified inverse/step assembly is useful, but plain accelerated projected
gradient is too slow. Next isolated optimization follows the primary paper's
squared-NNLS/L-BFGS strategy with bounded history and charged line searches.
True primal/dual/simplex/gap certificates must remain authoritative; a small
squared-variable gradient alone cannot authorize return. Original500M/pose
limits, source-assisted vs native/unskinned distinction and all prior negatives
remain. No avatar-specific parameters or postnormalized-field fallback.
