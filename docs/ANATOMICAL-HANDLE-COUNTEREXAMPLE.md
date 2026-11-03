# Exact anatomical boundary collapse — no native admission

Runtime02a066b remains original500M. Separate test-only16B Newton controls retain
all4096iteration/CG limits,512MiB, primal/dual/gap certificates and fixed poses.
MaleTops completes a genuine coupled field at6.670B,102outer/4089CG, dualKKT
6.00e-10, original primal1.40e-12 and gap4.39e-10; pose1 still fails37.121mm
maximum versus35.133mm. Original point consumer/seams/Scene are exact. Female
Body/Tops and maleBody exhaust16B, so their coupled quality remains unknown.

A separate construction-only audit identifies a structural cause. MaleTops
upperChest4,neck5,leftShoulder9,rightShoulder33 have exactly the same sole hard
row919(-.0004975,1.4309608,.0146232); head6 alone owns row971 at1.523284m.
Identical Dirichlet columns in the unique symmetric convex energy force equal
fields, regardless of optimizer. At failing collarrow1096, original neck5=.986;
generated head6=.766, with chest4/neck5/shoulder9 all.078. Reference weights are
only inspected by the independent diagnostic consumer, never fed to construction.

Actual anatomical source endpoints are already correct: upperChest1.3046→neck
1.45584→head1.54491m, shoulder branch to upperArm. There is no zero-length-helper
bug to patch. One global minimum over the complete physical bone discarded its
longitudinal support. FemaleTops likewise merges upperChest/neck/both shoulders
and some genuinely weighted secondary parent/end controls.

Longitudinal bone sampling can separate those columns using actual geometry,
hierarchy and mean-edge resolution (the primary bone-sampling principle).
However, it must NOT blindly force every outside sample onto a part. FemaleTops
also genuinely uses leg61/72 whose segments start below the garment (.87→.52m),
and male head6 is above the Tops axial extent. Source positivity does not grant
unit surface ownership. A source-aware support/ownership layer must mark such
out-of-part handles and retain their required influence without silent dropping.
A detached bounded sampling/support probe is the next step, not a guard lift or
claim to complete robust/native/unskinned humanoid authoring.
