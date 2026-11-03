# Required landmark selection in fitting guides

The full frozen b32 CPU regression passed 532 of 533 suites; `rig-fit-render`
failed its independent reflected/sheared selected-marker oracle. The renderer
compared a raw landmark key such as `hips` with the UI label `hips (required)`.
Required landmarks consequently always used their unselected colour.

The renderer now uses the actual landmark identity for landmark mode. Existing
rig mode retains its joint lookup and index-based highlighting. Display labels,
world transforms, marker colours, geometry, persistent data and edit commands
are unchanged. No header, class layout, public API or schema changes.

The extended real GL fixture retains every original affine/position/colour,
bounded-upload, repeated-eye, edit, Cancel and GL-state assertion. It additionally
changes the selected required landmark, checks the old marker becomes unselected,
compares existing-rig joint markers with the same geometry under unrelated bone
display names, and restores the exact original immutable view's pixels.

Matched immutable b32 support plus the rebuilt renderer TU passes 13 checks on
software GL and 13 with whole-owned C/C++ O1 ASan/UBSan/leak support, with no
sanitizer findings. Strict actual A770 GL passes the same 13 checks. The unchanged
old renderer fails the first selected-marker oracle in all three profiles. All
1,434 tracked component inputs and each profile's eight copied support archives
remain exact through their tests. These are scoped matched-archive component
proofs, not a fresh whole-project/application or SYCL compute claim.

The frozen master's original complete 533-suite result remains a genuine
532-pass/one-failure run; it is not relabeled green. Fresh coherent integration,
appropriate affected regressions and actual native workflows are still required.
The full production goal remains incomplete. No physical HMD, user-session
restart or system installation occurred. Exact commands, input/output hashes,
negative results and scope are in validation/RIG-FIT-GUIDE.json.
