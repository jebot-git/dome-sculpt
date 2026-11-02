# Prepared keys after UV reprojection — private prototype

This follow-up starts from frozen Scene19 `bb17c7f`. It is not publication-ready
or completion of the production goal. No public layout or archive changes are
introduced.

The private authoring adapter follows each immutable UV receipt back to its
owning before-model. Current morph POSITION fields use the stored source vertex
IDs rather than geometric nearest matching. It delegates actual key authoring
to the previously qualified prepared-key factory, then deeply authenticates the
new before-model through every existing normal UV replay. The output retains
current paint, baked/Clear-bake state, masks, material values, skin and pose.
An actual complete old prepared UV model survives as the source capsule.

Create, Replace, Duplicate, Mirror, Capture Mix and Rename use this path.
Capture Mix retains raw linear NORMAL/TANGENT sums. Multiple current seam
vertices mapping to one old vertex must agree; independently tearing those
vertices needs a real native corner-domain conversion and currently refuses
without averaging or changing authority. That conversion remains required.

Initial scope is a changed-whole-key-TU/test overlay against frozen `bb17c7f`
core/support/renderer archives. Include headers match byte-for-byte and linker
maps exclude the old key helper member. Thirty-three native assertions pass,
including real Document workers, all key actions, exact current paint/skin/pose,
restart, Undo/Redo, source restoration and cancellation. Fifty-two software
graphical assertions pass with independent emitted GLB/VRM consumers over
multiple mixed-skin poses/morphs and separate MToon/PBR evaluation. Independent
triangle cross products and original-UV derivatives also check actual new
NORMAL/TANGENT channels. Exact prototype artifacts are listed in
`validation/POST-UV-PREPARED-KEYS-PROTOTYPE.json`.

Fresh coherent core/support/renderer sanitizer and Release builds, hardware,
native/controller UI, broader avatar workflows and persistent allocation-failure
proof remain pending. The new worker input rebind fix must be incorporated
before controller qualification. All frozen prior proofs remain intact.
