# Transaction, procedural sculpt and qualified import integration

This private candidate starts at `beae6cd` (procedural `8c32179` plus source-retaining
GLB import `13b5156` and required missing-normal guard `633bc7b`) and merges the
frozen transaction/paged-picking checkpoint `5fa70fe`. All parent commits are
retained. It is another milestone toward the full objective in REQUIREMENTS.md,
not completion of that objective.

## Semantic union

The procedural CPU/OneAPI/CUDA/HIP box interface and sculpt worker remain intact,
including page18, native FLTK settings, source leases, strict bounds, masks,
symmetry, editable output layer and one Undo. `document-sculpt.cpp`, the procedural
kernel/editor, input/palette routing and import helper are byte-identical to the
qualified `beae6cd` baseline at initial integration.

The qualified GLB import lambda prepares a detached replacement Scene through
`prepare_authoring_import`; it retains its original source/material/morph data
and diagnostics. It goes through the same guarded ordered asset worker as other
Scene operations. The transaction's admission wrapper retains the queue predicate
through publication, removes rejected jobs, and restores asset ownership/status
on failure. Worker publication prepares history/state/cache before authority is
committed. The mandatory `633bc7b` guard explicitly refuses GLBs without authored
NORMAL until flat-normal/Mikk source identity is implemented; it does not promote
an arbitrary display fallback to source authority.

All 23 Volume state fields are still present in the no-throw swap, including
paged owners, authenticated history, physical band, masks, settings, gesture
starts and layer/history identities. The four-argument cancellable ray API and
its old three-argument wrapper remain. Ordered native mouse/pen work continues
to use the actual cancellation/stale lease predicate. Seven-worker lifecycle,
accepted End/Save ordering and immutable rendering are retained.

`Document::mesh_loop` remains byte-identical to `0c9a44e`, SHA256
`0c5410f115af75399f108ce0c2bea983e2d25a9f914865e6ed67100bb3a8461a`.
Normal frames, colors, Scene9/material2, source/canonical authority guards and
original geometry identities are preserved.

## Validation in progress

The initial fresh regular-user CPU and OneAPI all-target builds passed with
canonical DomeD FLTK `282467e`. These initial builds do not yet include the
parent's generated-sculpt tangent/bake cross-workflow fix. Focused combined
native tests and a complete-own-C/C++ sanitizer build are underway. Final
qualification and manifests will use the forthcoming source checkpoint, with
fresh consumers for changed bake-setting ABI. No initial build is presented as
proof for a later source revision.

No physical headset, system installation, session restart or external push is
part of this candidate. Remaining full import, colored bake receipt, source UV,
affine, production-size performance and complete native/XR authoring objectives
remain required.

The VRM resolved-role correction `6f7aa94` is also merged without a schema or
public layout change. Its independent consumer is a new private helper, so the
qualified import consumer's node-weight evaluation remains untouched. The initial
narrow four-suite CPU run passed the procedural fault mode and14+20 VRM checks;
its import case found only the codec's optional-null serialization artifact.
The corrected import/procedural scopes pass2/2 in2.50s, injecting103+77 real
publication allocations individually. The complete-own-C/C++ ASan/UBSan/leak
build passes all four scopes4/4 in48.81s.

These two actual workflow scopes intercept the real detached helper's return,
then measure/inject the existing transaction's publication allocations. They
preserve exact failed Scene, content/next-content, revision/generation, history,
redo, source cache and mesh, release the failed busy lease, and retry through the
actual import/procedural API. Retry retains the independent painted source and
is exactly one Undo/Redo transition. The successful retry's existing-object
comparison normalizes only absent-equivalent null `material_bindings` and
`retopology` fields introduced by the Scene9 codec; nonnull values and all other
payload fields remain exact. Failure comparisons retain the full exact Scene.
This scope does not inject every allocation inside detached import/CSG preparation.
