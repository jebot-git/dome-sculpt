# Reflected import consumer correction

This resumes private0b65130 on the physically accepted main8d46dfb baseline. The
original54 and18 pixel-error logs and proof trees remain unchanged. Production
renderer/direction code and the pixel tolerance2 are unchanged.

The parsed-file reference already transformed normal/tangent vectors into world
space, but it kept the source-local tangent W. That reverses the reconstructed
bitangent under reflected node placement. The independent consumer now applies
the node-global determinant sign when constructing its world-space frame. Its
front-face sign continues to follow the core glTF mesh-instantiation rule.

The new13-case CPU witness derives the transformed plane normal from parametric
geometry, transforms its tangent/bitangent directions independently, and checks
both handedness signs under positive/reflected anisotropy and shear. Omitting
the determinant multiplier explicitly produces the opposite bitangent in all
reflected cases. This uses no production frame helper. Singular frames refuse.
It does not establish an extra per-bone reflection or universal MToon skin rule.

Fresh coherent canonical FLTK Release passes9 selected suites in8.43s, including
821 original-chain source/bind/current-pose/export checks. All12 original-file
versus-native affine skin/morph/alpha comparisons are maxerror0; baseline12
comparisons also remain maxerror0. Each graphical suite has24 assertions. The
actual Arc A770 repeats both suites with maxerror0, using the explicit PCI03:00.0
selector and captured GL_VENDOR/GL_RENDERER. Software used llvmpipe. Toolkit
inputs at latest DomeD d271cf6 were verified unchanged from the canonicaleee
23-patch toolkit before this build.

This is a scoped private milestone, not full import completion. Native workflow,
GUI/controller and current-core sanitizer qualification remain next. Implicit
default-material identity and UV1 source sampling still retain explicit guards;
Scene17 is reserved for witnessed default material and Scene18 for UV1 work.
Source transforms/IBMs/raw archives remain immutable; the ordinary similarity
route stays intact. No physical headset, system installation or session change.
