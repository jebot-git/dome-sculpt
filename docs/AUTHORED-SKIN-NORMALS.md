# Preserve authored normals while skinning

Ordinary native skinning previously rebuilt normals from the deformed triangles.
Even a rigid or identity pose replaced artist shading and split-normal seams.
Positions still use the same linear blend. Normals now follow the existing
explicit Dome retained-frame convention: blend each joint inverse transpose
applied to the authored normal, then normalize once. This is deliberately not
inverse-transpose of the blended position matrix. Singular/cancelling normals
fail before publication. No native schema or public layout changes.

Nine native commands pass each optimized Release and owned sanitizer profile,
including 77 independent double Gauss-Jordan, split-normal, rigid/scale/reflection/
shear and real native identity-rig Bake/history/save checks. Two graphical
regressions additionally pass 127 PBR-preview and 51 prepared-normal shape checks
per profile, including actual publication failure/retry coverage.

Six actual native-worker GLBs match independent parsed-file images with maximum
pixel error 0 in Release, sanitizer and actual Intel Arc A770 runs. The fixed
bound stays 4. Substituting the former public core fails the same comparison at
59 in both profiles. Real software and UHD770 hardware controls refuse the Arc
gate. Eighteen GLBs have zero Khronos errors, 18 skinned-node warnings and 18
native-buffer infos; all messages remain. Twelve profile/device file pairs are
byte-identical. Both native applications relink from matching owned components.

This is scoped complete-TU/component evidence, not a fresh whole build or full
612-test pass. Configuration registers both new tests. The independent consumer
was extended for valid PBR files without a normal map/tangent, retaining its
strict normal-mapped requirement. Original fixture/script failures are recorded.
Read [the receipt](validation/AUTHORED-SKIN-NORMALS.json).

General normal-map and morph fidelity remains unfinished. The separate private
curved baked-normal tangent experiment now passes its static/skinned cases, but
an active curved native morph still exceeds the original bound (13 versus 4).
That exporter is excluded. No system installation or headset/session action
occurred, and every unfinished full-goal requirement remains required.
