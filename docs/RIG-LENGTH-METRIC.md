# Translation-independent rig length controls — private component

Runtime `06fd0e0` extends private Source25 integration `8d5a5e3`. Existing-rig
controls now measure Bone.length along the REST frame's local Y through the
complete world linear transform. Both dialog initialization and staged-edit
comparison use that metric. Subtracting float-rounded world endpoints could
turn a valid short bone into zero and prevent configuration of the rig.

The actual old native Document probe has a ready mesh within the 100-metre
authoring workspace. At translation (90,90,-90), the displayed length is zero
while independent linear evaluation is 1.74928556404e-6 metres. The source bone
length, 2e-6 metres, satisfies existing bounds. Earlier diagnostic probes at
5,000/30,000 metres exceeded the separate rendering workspace and are not the
accepted positive fixture.

Eight cases exercise ordinary/reflected/sheared object frames, translations
and translated exact joint REST frames. Actual Document initialization,
independent matrix-column length evaluation, no invented opening edit, valid
unchanged configuration, exact one-field staged length changes and Cancel pass
56 checks in Release and 56 under ASan/UBSan/leak checks. Native scenes and
paint stay exact. Head positions, poses, inverse binds and source data are not
rewritten, and no public layout, archive format or UI control is added.

This is a matching-ABI component qualification: the two owned changed C++ TUs
and complete test are compiled before immutable, freshly built parent
core/support libraries. All 80 public headers match the parent; link maps
exclude the old copies of both implementation objects. The sanitizer owned TUs,
test and parent core/support are instrumented. Source and binary manifests
remain unchanged through each run. This is not a new whole-branch/frontend
build, physical-headset or new GPU proof. A coherent final integration remains
required. See validation/RIG-LENGTH-METRIC.json for the exact scope and hashes.
