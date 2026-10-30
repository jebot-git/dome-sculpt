# Affine interaction adapter (private migration)

This continues 55ce on public 7152, retaining both ancestries. It is a native
adapter prerequisite, not an enabled affine Scene/import workflow. Scene fields,
archive versions and acceptance guards are unchanged.

`src/affine-interaction.hpp/.cpp` supplies a factored world/inverse/normal frame,
world-gesture conjugation, ray parameter conversion and a capsule brush metric.
World here means scene/model metres after object/group placement, before headset
or global canvas navigation. Parent-to-child factors remain ordered. Raw normal
vectors do not acquire a material interpolation/normalization policy.

A world gesture D commits parent-local E_new=P^-1*D*P*E_old. It can contain local
shear even if D is a hand's rigid/similarity movement. This is intentional; no TRS
projection is allowed. A later parent edit still acts on the local result.

World ray directions are normalized before inverse placement. Their local
length determines both maximum local distance and conversion of the hit distance
back to world metres. Radius is never divided by an average object scale.

The capsule metric evaluates full transformed displacement vectors, equivalent
to L-transpose*L without squaring extreme scales into an unrepresentable matrix.
Local support bounds use inverse-linear row norms with outward rounding. This is
currently an adapter/analytic fixture; it does not change Volume's local SDF
semantics or authorize a nonuniform sculpt kernel. The existing evaluated-paint
route must be extended in place rather than replaced with a separate painter.

Fresh standalone Release and ASan/UBSan/leak runs pass 3/3 suites: prior 74 math
and 56 hierarchy assertions plus 667 interaction assertions. The latter includes
independent reflected-ray distances, analytic sphere/capsule extents, 128 world
sphere boundary/inside/outside directions under shear, raw normal matrix,
reflected facing, grip-induced local shear, later parent changes, original-factor
extreme anisotropy, input errors and 128 valid sample iterations under permanent
C++ allocation denial (zero attempts). All adapter/math/test/support TUs are
rebuilt; this is not whole-core, graphics, posed-pixel or GUI qualification.

The pending Scene 13 capsule must preserve source-local graph authority and native
parent-local affine edits separately, recurse through PolygonOrigin retained
placements, and migrate mesh/field picking, evaluated painting, normals/tangents,
culling, held previews, caches and legal factored export together. Singular or
numerically unrepresentable paths must fail atomically, not flatten source data.
