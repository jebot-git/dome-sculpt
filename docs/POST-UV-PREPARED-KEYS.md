# Prepared keys after UV reprojection

This follow-up starts from frozen Scene19 `bb17c7f` and incorporates the narrow
controller input publication repair `0057749` as `ceb9893`. It is a scoped
authoring checkpoint, not completion of the production goal. No public layout
or archive changes are introduced. The initial prototype proof remains recorded
separately below.

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

Initial scope was a changed-whole-key-TU/test overlay against frozen `bb17c7f`
core/support/renderer archives. Include headers match byte-for-byte and linker
maps exclude the old key helper member. Thirty-three native assertions pass,
including real Document workers, all key actions, exact current paint/skin/pose,
restart, Undo/Redo, source restoration and cancellation. Fifty-two software
graphical assertions pass with independent emitted GLB/VRM consumers over
multiple mixed-skin poses/morphs and separate MToon/PBR evaluation. Independent
triangle cross products and original-UV derivatives also check actual new
NORMAL/TANGENT channels. Exact prototype artifacts are listed in
`validation/POST-UV-PREPARED-KEYS-PROTOTYPE.json`.

## Coherent qualification

Final hashes, exact commands and limits are recorded in
`validation/POST-UV-PREPARED-KEYS.json`.

- A fresh native FLTK/OpenXR Release build passes ten selected suites. A fresh
  whole C/C++ core/support/renderer O1 ASan/UBSan/leak build passes the matching
  ten suites. These include the new33 native and52 independent graphical
  assertions, prior UV/key regressions and the real input-publication gate.
- The52 graphical checks also pass on actual Arc A770. An independent geometric
  oracle derives the new full NORMAL target using triangle cross products and
  exclusive-corner TANGENT targets from original frame UV derivatives. Parsed
  GLB/VRM consumers then evaluate three anisotropic mixed-skin poses and morph
  weights0/.35/1 with separate PBR/legacy MToon policies. Maximum byte errors
  are0 for PBR and1 for MToon. Newly authored targets and normal maps have
  explicit visible-effect checks.
- Dense289-vertex desktop/controller sculpting has29 additional Release and
  sanitizer checks: actual native strokes, exact paint/saved mix, one Undo/Redo,
  cancellation, immediate ordered Save, ray/pressure editing, focus loss and
  opening/finishing the shape tools after a committed stroke.
- Actual native FLTK/DBusMenu tests pass17 normal and19 compact/large-bitmap-font
  checks. Creation, Duplicate, default-cancel Return, modal Close, native
  Shift/left target strokes, visible viewport changes, ordered Save and exact
  global Undo/Redo use real controls and retain the current paint/skin/pose.
- All18 actual emitted files have0 Khronos errors. The27 warnings are18
  retained-parent skinned-mesh warnings and9 legacy VRM extension-name warnings.
  All9 VRM files pass the15 pinned official VRM0 schemas.
- Both Release and fully instrumented core/support sanitizer runs pass persistent
  ordinary C++ `new` denial at13,555 reached boundaries, from13,556 measured
  allocations. One cache-dependent boundary is not reached after warmup and is
  explicitly excluded. Every reached failure preserves the entire current and
  retained UV graph; an allocation-enabled retry reproduces the exact complete
  accepted model. This is a direct authoring operation proof, not a new claim
  about every worker admission or external allocator.

The shader comparisons qualify native/export parity for these current models;
they are not a general all-pose conversion bound for arbitrary original normal
frames. No physical headset launch or system installation occurred.

## Required follow-up

Actual bundled avatar UV/key workflows and the full representable native
corner-domain conversion for independently edited seam vertices remain required.
Prepared-normal source baking, broader frame/sampler/UV1 support, COLOR/UV morph
attributes and the rest of REQUIREMENTS.md remain open. Existing guards are
staging boundaries, not completion by refusal. All frozen prior proofs remain
intact.
