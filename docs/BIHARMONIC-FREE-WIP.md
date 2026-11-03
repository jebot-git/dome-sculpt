# Private free-principal sparse LDL checkpoint

Base77eb5e7 is preserved. This private solver now compacts the actual free
principal submatrix before symbolic/numeric LDL. Relative global ordering stays
unchanged. Every free elimination path is also an earlier-vertex path in the
full graph, so the full symbolic fill bounds storage; the implementation checks
that bound, preallocates all mask scratch and verifies its exact numeric work
prediction on every factorization. Actual original matrix-free Q/residual and
box KKT remain authoritative. No public/native binding behavior changes.

Independent gradient-FEM/dense QP,2x2 principal inverse, changed-same-address
mask/equal-copy cache and open/disconnected strip controls pass.45/90 vertices
need2CG,75,800/146,018 counted work. These are optimized private C++ TUs with
copied uninstrumented b32 support, not new sanitizer/allocation qualification.
The first attempted edit failed a Python assertion and accidentally compiled
old source; that control log is retained and excluded from new evidence.

Both actual CC0 avatar cohorts now solve17/19 parts, with all85 solved-part
poses per avatar passing the unchanged mean0.5%/maximum2% height limits. Hands
newly solve at458,398,090/431,873,183 work (female/male), under the unchanged
500M cap. Body and Tops still refuse before a numeric factor exceeds remaining
work. Female4 auxiliary joints remain missing because Body refuses. No seam,
native point-consumer or Scene preservation errors occur on solved subsets.
All1,526 captured source/test/binary/support inputs remain exact. This is not
a full-avatar pass, production automatic-weight completion or native history,
Save, export/render, full solver sanitizer/allocation qualification.

A separately generated test-only diagnostic preserves the original runtime
source and fixed500M cap. Male Body uses75.6M before solving, then85 factors
reach453.9M after three columns. Tops uses16.5M before solving and178 factors
before refusal. It identifies repeated active-mask factorization as the main
next optimization. The diagnostic includes only Body/Tops and is not an
additional whole-avatar pass. Its source/binary hashes remain exact.

The next isolated candidate will reuse a single base-Dirichlet positive factor
as a restricted inverse preconditioner for changing active sets. Its principal
inverse block is SPD; true Q/KKT still decides success. Independent dense
controls precede any real cohorts. Source-assisted positive-joint intent stays
distinct from geometry-only/native-unskinned weighting. All prior failed
heat/prior/FMM/chamfer/biharmonic controls and wider stress/native goals remain.
