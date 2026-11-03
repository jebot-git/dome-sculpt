# Matrix-free projected Newton-CG: real coupled progress, still private

Base9121a51 and every earlier cost/quality negative remain. The new solver uses
the SAME convex coupled dual and one fixed sparse Dirichlet inverse. It maintains
at least one exact zero multiplier per row, fixing the zero with largest current
gradient to remove the known PoU null direction. The restricted Hessian is SPD:
a row-constant null vector with an excluded entry per row must vanish. Working
charts do not authorize success; actual global NNLS KKT/gap/primal/PoU still do.

Native PCG uses T*P_free*A*P_free*T, positive by congruence with original SPD
free-row A. T is the independently verified inverse square-root of the exact
principal label block I-11'/labels. The inexact-Newton forcing is predeclared
min(.5,sqrt(actualKKT)); it is never output acceptance. Descent globalization,
projected Armijo, CG/work/byte bounds and failure staging remain explicit. No
avatar/gender/outcome parameter choice, source-value copying or fallback field.

128 independent64-face long-double primal QPs pass Release and wholly-owned
kernel ASan/UBSan/leaks: max4.12e-11,KKT2.47e-11,PoU1.23e-15,342totalouter
iterations. Exact T*P*T identities, independent original-FEM energy/principal
inverse and open/disconnected/unseeded controls pass. KernelSAN is arithmetic
only; actual factory controls link copied uninstrumented b32 support. The extra
tracked energy regression is byte-identical to the artifact source compiled for
its recorded proof; no full-build/SAN claim is inferred.

Actual original500M full cohorts solve15/19 each, all75 solved-part poses per
avatar within fixed0.5%mean/2%maximum height limits. Legs now complete (female
53.8M); Shoes22.8M versus squared-dual male418.7M. Body/Hands/Tops/Bottoms still
exhaust work. All1558capturedinputs, original Scene, seams and native point
consumers remain exact; female10auxiliary outputs absent. No whole avatar,
robust source-free/native authoring, history/Save/UI/export admission. Best old
relaxed bounded count remains998c93a17/19 but has its own real pose negatives.

Next a separately labelled enlarged-budget Body/Tops control tests whether the
proper coupled field itself meets real poses. It cannot replace original500M
failure or increase any production cap/certificate/pose tolerance. Only actual
quality+cost success can open whole-owned solver SAN/OOM/Cancel and real native
worker/history/Save/controller/GUI/export qualification.
