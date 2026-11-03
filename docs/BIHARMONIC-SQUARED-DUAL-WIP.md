# Squared-NNLS/L-BFGS: actual bounded native control

Base5287472 remains preserved. The audited primary method's squared multiplier
parameterization now uses native8-pair L-BFGS, bounded Armijo search, complete
work/byte staging and true NNLS/primal/simplex/complementarity-gap gates. Small
squared-variable gradient alone never authorizes return. The original native
matrix-free Q stationarity and inverse residual are checked independently.

The first native math control exposed a real history-slot overwrite on skipped
curvature, then a near-optimum objective cancellation stall. Both negatives and
exact diagnostic sources remain. Staging history before its positive-curvature
commit fixes the former. Evaluating the exact quadratic objective difference in
multiplier coordinates fixes the latter without changing any acceptance bound.
Final128 independent64-face long-double primal QPs pass: max1.32e-10,
KKT9.39e-11, partition1e-15,2733totaliterations (versus17504 projected gradient).
Native original-Q inverse/open/disconnected/unseeded controls pass.

The original500M real cohorts still fail: female14/19 and male13/19 solve.
Body/Legs/Hands/Tops/Bottoms exhaust work; maleTie misses the unchanged4096
certificate bound. Shoes now complete but need thousands of iterations
(male4897total across2components,418.7Mwork). All solved-part poses, seam/source,
consumer and Scene checks pass,1544capturedinputs unchanged. No full avatar,
solverSAN/OOM/native UI/history/Save/export admission. Best fixed-cap route
remains998c93a17/19, itself incomplete.

The squared parameterization creates slow near-zero multiplier variables under
our strict certificates (the paper's default stopping alone is insufficient).
Next isolated optimizer will apply projected limited-memory quasi-Newton steps
directly to the SAME nonnegative convex dual, with exact quadratic Armijo and
unchanged certificates. This is not a fit-error fallback, new pose policy,
source-value copy, or tolerance/cap increase. Preserve all prior negatives.
