# Private heat construction audit

Base50d3057; no public/CMake/legacy Automatic changes. Pinned Blender3a4070ad armature_skinning.cc, meshlaplacian.cc and intern/eigen/intern/linear_solver.cc are primary references. This is native private experimental code, not a byte-equivalent Blender port or a native admission receipt.

| Construction | Reference | Explicit candidate / remaining distinction |
|---|---|---|
| Bone geometry | Actual armature world heads/tails; B-bone segments share a deform group. Heat does not use envelope radius. | Current authenticated imported joint/semantic child geometry. Raw source axes are not invented bone-Y tails; ambiguous forks are recorded joint points. Native added/edited roles remain unfinished. |
| Distance | Closest point on segment; physical distance divided by0.5*(normal cosine+1.001). | Analytic head/interior/tip and current worst-row independent long-double controls pass. Double arithmetic and safe cosine clamp remain distinct from float reference. |
| Normals | Normalize sum of unit geometric face normals. | Explicit UnitFaces option; artist normals never supply this geometry. Old area-normal control retained. |
| Nearest / ties | Minimum over sources before visibility; relative1e-4. Each visible tied segment counts, including same-bone segments. | Explicit ReferenceSegments implements this count and aggregated per-bone linear RHS. Existing DistinctBones/1e-6 retained. Independent50ppm and duplicate-source constant-solution controls pass. |
| H / matrix | H=tie_count/max(distance,1e-4)^2. Row Laplacian scale1/(4*mixedArea); cotangent/incident-face-count. | Mixed mass with heat_strength4 is the symmetric row-equivalent matrix. Independent clipped-cell, gradient/count dense row solve errors2.62928e-8/4.85803e-9. |
| Linear solve | Least-squares matrix built as M^T*M then SparseLU. | Bounded iterative PCG measures true original residual, valid only for SPD matrices. Reference boundary incidence scaling can break PSD: exact4-vertex certificate gives FEM energy23/5 but counted-edge energy-1/5. Two actual parts correctly refuse lost positive curvature. |
| Negative fields | Solves signed potentials and applies explicit output limiter. | Strict FEM negative-potential refusal retained unchanged; PositiveGraph is a separate clipped-edge energy. No post-error fallback. |
| Output | Below0.025→0;0.025..0.05 maps linearly to0..0.05; above0.05 unchanged. | ReferenceLimit option tested independently; export-oriented top4 and normalization remain explicit extra steps. No threshold/bound relaxation. |
| Visibility | Whole supplied mesh, shifted ray origin, entry-facing dot<-1e-5. | ConnectedSurface policy is explicit, not reference. Conservative grazing and exact incident-face filtering/no origin bias preserve real thin-wall control; never silently switch after an error. |

## Actual causal diagnosis

At male Body armpit row276, genuine Shoulder-tail/UpperArm-head coincide at(.147266,1.404071,.004221). Their physical distance is59.33mm and adjusted71.22mm, versus UpperChest169.38mm. Both emit; solved raw fields Shoulder0.3757, UpperArm0.4513, UpperChest0.0849. The artist reference is UpperChest0.8999/UpperArm0.0990. Tops row110 similarly prefers shoulder101.140mm versus UpperChest207.274mm; UpperArm is only18ppm farther, so the reference tie rule includes it. This is direct source attraction and field construction, not a large PCG residual or merely discarded top-K weights. Artist values are diagnostic oracles only, never solver inputs.

With the positive graph and complete reference source/limiter option, female19parts×5poses passes. Male still has3 failures (Body37.125mm, Tops40.240mm, Shoes36.570mm vs35.133mm max). Raw output before the limiter has the same three maxima. Strict signed FEM with the reference boundary matrix solves17parts and refuses2 in each avatar; male still has three quality failures among solved parts (Body37.281mm, Tops38.477mm, Shoes37.058mm). All original Scene bytes, solved seam positions and independent point consumers remain exact. No both-avatar route is admitted.

The next distinct candidate is a box-constrained **standard FEM heat** energy, not incidence-normalized reference least squares: minimize0.5*w^T*(L+H)*w-b^T*w for each bone, with0<=w<=1. Standard cotangent-half FEM preserves its PSD variational energy; source-informed handles/H are fixed before solving. Actual bound KKT residual is required, not clamping and returning an unconstrained result. This preserves negative edge information without the separate PositiveGraph clipping. Strict raw-FEM control, all failed quality limits and source continuity negatives remain unchanged.

Scope: own fresh private kernel/fixtures with copied immutable b32 support archives, not whole-owned sanitizer/native/history/Save/GLB/render/GPU proof. Small reference+dense fixtures pass Release and own-kernel O1 ASan/UBSan/leaks. Actual pose input checks1474 exact.
