# Private VRM1 expression preview and explicit static Apply

Code `ff13b33ae335f0601cfa9048dcf709f39d18bb3b` descends from sealed
`1853cc36199679aa03326919b65d3c27fff8662d`. This is a qualified native-control
checkpoint, not main admission or complete VRM1 support. The independent
source-image comparison described below still fails its unchanged quality gate.
All finite jobs are closed and the hardware lease is released.

The existing FLTK universal menu now exposes View / Expressions: source preset
or custom selection, numeric weight, step/reset, explicit Apply static pose, and
Cancel. Native bitmap controls fit normal and 640×360/font32 desktops. The shared
controller route is Materials → View options → Expressions, page29. Authored,
CorePBR and Toon remain on the existing View page. The eight appended commands
are6144…6151; total page count is30. No new frontend or persistent Scene field is
introduced.

An immutable original Scene and validated actual source catalog are captured by
an owning ephemeral view. Source parsing and geometry/material evaluation run
on the mesh worker. Session and monotone request identities reject stale dialogs
and superseded work. Preview leaves the authored Scene/history untouched. Apply
recomputes the captured operation on the ordered native worker, creates one Undo
entry and supports queued Save; Cancel, focus loss and late cancellation preserve
the complete prior Scene and file. Ordinary Save/export cannot silently apply a
preview. Competing projector placement is excluded through the existing native
edit authority.

The public C++ ABI changes: EditorState appends the immutable view pointer, which
also changes embedded Document layout; MeshJob captures it. A new view DTO and
additive Document/catalog APIs are declared. No new serialized authority or Scene
version is used. Any semantic union with current Scene30/31 or native-float
branches requires fresh coherent consumers. This tree never combines their
headers with these archives. In particular, the separate native-float renderer
must retain its native_geometry exclusion and both private draw_range arguments.

## Actual failures corrected

The initial controller entry cancelled input before capturing its hand owner.
The native route failed after35 positive checks; the final route captures the
real owner first. The exclusive-tool menu also removed legitimate computed
Cut/Copy/Paste/SelectAll/Undo/Redo modes from its own numeric modal. Its first GUI
run failed on real disabled SelectAll. The fix preserves those canonical input
modes without enabling Scene edits during preview. Clipboard checks require the
actual application selection owner and exact copied-back numeric text.

The actual-loader test then exposed material-view changes closing expression
page29. The existing shading transaction now retains only its authenticated
existing owner/page with a new release fence. A native regression reproduces
the old loss and tests genuine Authored↔CorePBR switches. The earlier runtime
fixture's unrelated generic tracking loss/moving hand and non-rendering capture
frames are retained separately from this product failure.

Visual review found an additional real stale0% label: the palette signature
omitted expression state while the native weight correctly advanced. A new
actual-worker GL test compares the cached palette to a freshly constructed
palette for the same state. The old cache fails; the final cache matches exactly
and weight-only changes visibly refresh. Session/request/selection/readiness and
the displayed entry are now included in the signature.

## Source morph defaults

The pinned [VRM1 expression rules](https://github.com/vrm-c/vrm-specification/blob/94e82dd346fa6cf0337c4421728640e5252dd38e/specification/VRMC_vrm-1.0/expressions.md)
describe resetting morph targets and accumulating expression contributions. Both
[three-vrm's bound-target implementation](https://github.com/pixiv/three-vrm/blob/3caf7a21ee68f0d5dfa363ab3d577450bbf60136/packages/three-vrm-core/src/expressions/VRMExpressionMorphTargetBind.ts)
and [UniVRM's target merger](https://github.com/vrm-c/UniVRM/blob/efd20f62f1bf0ac3fcc34de1e497324c5197d146/Packages/VRM10/Runtime/Components/Expression/MorphTargetBindingMerger/MorphTargetBindingMerger.cs)
limit that authority to targets actually referenced by bindings. They do not add
the controlled target's glTF default to the expression sum.

The old adapter also cleared unbound source morphs. The genuine fixture has mesh
default.2, node defaults.7/.3/.6 and only node1/target0 bound. Its old-core control
shows.7→0 correctly, but.3→0 incorrectly. The final adapter uses the evaluator's
complete bound-target map, including zero-valued bindings: bound.7 is replaced;
unbound current.3/.6 and native-only targets remain. Native81 verifies zero and
nonzero requests, accepted/serialized results, Cancel and one Undo/Redo. Raw
original bytes and metadata are not rewritten. Starting a zero-weight preview
can legitimately change the bound source pose; Cancel restores its exact prior
pose and pixels.

## Evidence and exact build scope

The full machine receipt is [VRM1-EXPRESSION-PREVIEW.json](validation/VRM1-EXPRESSION-PREVIEW.json).
It records source/binary/ledger hashes and every preserved negative.

- Fresh own ABI Release/SAN app/core/render/support with15 requested targets:234 build
  tasks/profile,503.376/761.146s;2072 inputs/profile unchanged. Subsequent complete
  changed-TU continuations and dependent frontend relinks are recorded separately.
  Final complete palette/consumer continuation19.264/43.026s;2056 inputs exact.
  This is not a fresh final all-target build.
- Native81 and actual cached/fresh GL palette15 pass each profile. GNU wrap is
  confined to the native test: its gate is after real detached computation and
  before publication, not a claimed internal cancellation poll.
- Persistent caller-thread C++new admission1161/profile:328 Begin,242 Configure,
  248 Cancel,343 Apply. Every denied prefix preserves complete Scene, session,
  input epochs, private history/counters and publication, then retries. Release
  120.879s; owned ASan/UBSan/leak1488.783s;1848/1851 inputs exact. This does not
  claim internal worker malloc or third-party allocator-null coverage.
- Final actual normal/compact FLTK22 each profile pass. A prior SAN compact run
  inspected immediately after asynchronous Undo Save without a publication ACK.
  The corrected driver waits for actual file replacement and keeps the same
  complete Scene predicates; it does not weaken Undo or clear errors.
- Final actual OpenXR-loader22/profile pass in44.887/75.323s: synthetic joystick
  selection/weight/view/Apply/Cancel/focus, real GL stereo and balanced swapchains,
  full native Save comparison. SAN uses the explicit staged Mesa allocator fix
  and checks actual mappings. Installed Mesa's earlier112B limitation remains
  separate; no suppression, system replacement or physical-headset claim.
- Strict PCI03 Intel Arc A770 loader22 passes34.571s; forced llvmpipe refuses1
  after actual Apply/Save/Scene/stereo checks,28.642s. Both1857 inputs exact.
- Final original affected registered7/profile pass9.471/65.324s. Configuration
  produces624 registered tests; it is not an all624 regression result.

The GUI driver is `tests/vrm1-expression-menu.py`. Its environment supplies
DOME_EXPRESSION_ROOT, BUILD, FIXTURE, EXPECTED and OUT (each prefixed
DOME_EXPRESSION_), plus COMPACT=1 for640×360. EXPECTED is the actual native
preview test's `prior.dsculpt`; canonical SDK/library paths and the private Xvfb
wrapper remain caller-owned. Run with the existing selection-gui-venv Python.
The loader driver is registered as vrm1-expression-openxr and creates a private
runtime manifest; it never uses a physical runtime. Its --hardware mode requires
actual Intel/Arc/A770 identity, with PCI selection supplied explicitly by caller.

## Unresolved independent image boundary

The same-source interactive consumer finds one PBR shifted-eye pixel7 levels
apart at the unchanged bound4: native(69,105,44,255), reference(62,105,46,255) at
(56,63). No camera, sampler, source/material data or quality bound was changed.
The exact old1853 headers/archives/binaries reproduce7, and current/old native,
reference and before-preview images are byte exact. GPU projection and a double
source-hierarchy trial still fail7. This is pre-existing, not resolved or
relabelled as a consumer-only issue.

An RGBA32F diagnostic locates the real discontinuity: native U=.3124999106 versus
reference U=.3125 on a128px nearest-filtered source, nominal LOD.336678. The
[OpenGL4.6 sampling rules](https://registry.khronos.org/OpenGL/specs/gl/glspec46.core.pdf)
place those on texel39 versus40. Native and original glTF world corners differ
only3–6e-8m through their different float skin-space paths. A diagnostic that
substitutes native-rounded world positions into the independent shader reduces
7→1, while preserving the original failing assertion. That substitution is not
an independent positive or production fix. All PNGs, raw data and unsuccessful
controls remain retained.

Full independent graphical qualification remains open. So do prepared/key-derived,
normal-UV, plainPBR and absolute-background expression authoring, expression-
preserving painted export, current-main ABI integration and broader VRM1/full-goal
requirements. Static Apply does not replace those requirements.
