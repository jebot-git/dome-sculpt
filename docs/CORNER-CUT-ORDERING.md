# Current-corner plane cuts and accepted Save

This private working candidate extends the existing native FLTK/OpenXR plane-cut
workflow. It is not yet qualified for integration. Unsupported UV1 sampling,
prepared color, prepared normals, MToon and retained artist-frame transfers stay
explicitly guarded. The final selected-key-sculpt mode compatibility guard is
still pending in the next isolated candidate; ordinary saved skin/morph poses
are supported. Current-color Bake adaptation remains separate required work.

The shell uses authenticated actual clipped-corner barycentrics and shares the
original immutable straight linear RGBA basis. Common editable paint/material
is baked independently and current RGBA is multiplied once during shading.
Connected generated cap topology has independent material. Full original paint,
UVs, rig/morph pose, shape source and affine source capsules stay restorable.
A shared bounded work/component/provenance/raster policy and complete ordered
geometry/normal/UV/RGBA/material/preview authentication precede publication.

Accepted cut Apply now exposes a worker-owned completion token. Its complete
immutable Scene snapshot is allocated off the model lock before authority
publication. Recovery metadata and transaction history/publication are staged
before commit. No fallible snapshot allocation follows the authority commit.
Revision/content/generation/object/recovery endpoints are moved into the token;
errors retain the original exception without allocation. A dependent Save is
queued after that exact cut on the same ordered file worker. It never waits for
work behind itself, samples a later mutable Scene, or falls back to the old Scene.
Later Undo or document replacement cannot relabel an accepted snapshot as the
new document's saved state. Cancellation/failure before acceptance cancels the
dependent Save and preserves the old output.

Native Close queues behind accepted cut work and accepted Save; unconfirmed
editing retains its existing cancellation semantics. Document destruction drains
an accepted cut before stopping workers, using its actual file predicate mutex
and a nonallocating completion flag even when diagnostic allocation fails. This
is cut-only ordering, not a claim about general asset or pending-UV Save.

## Evidence at this working checkpoint

- Frozen preparation checkpoint10b1f19: all56,629 measured persistent owned-new
  preparation boundaries fail, preserve full source/owners/counters and retry
  exactly (871.51s). Source16 and raw-source assertions remain enabled in that
  fixture's actual oracles. This is separate from native publication proof.
- Independent software GL and actual A770:32 cases/257 checks, all complete-image
  appearance and independent GLB/PNG/GLSL errors at most1; covers raw gradients,
  zero-alpha seams, Opaque/Mask/Blend, PBR/unlit, artist paint/common normal bake,
  real pose/morph, reflected/sheared placement, visible caps, restart/restore.
- Fresh canonical optimized native/controller:92 checks include actual pending
  Save, one Undo/Redo, cancellation/retry, exact selected-shell/read-only-cap
  authority, no-op, new generation, accepted endpoint after Undo, original
  exception/old-output retention, and predicate-gated destructor drain.
- Actual FLTK/DBusMenu:13 normal +6 compact checks, screenshots reviewed; actual
  real-loader/OpenXR GL:12 colored-cut checks including accepted and unconfirmed
  standalone Save, actual joystick/ray slider and balanced two-eye lifecycle.
- Rich-source publication has10,008 actual measured post-helper/pre-commit
  allocation sites. A test-only wrapper replays one actual immutable prepared
  result only after complete encoded input, every source/geometry/paint/recipe
  owner, ordered buffers/preview and admitted generation/revision/authority agree.
  Recipients have distinct mutable COW metadata. A separate frozen COW witness
  checks cached output metadata/const payload owners after every phase, plus full
  encoded output equality at completion. All original source/history/redo/content/
  revision/cache/draft/lease, original-exception, old-file and exact retry oracles
  remain. Five deterministic ranges cover the measured count; qualification is
  pending. Initial ranges0/2/3 collided with obsolete private fixture output
  files after an old CTest manager was not stopped correctly; affected logs are
  retained and clean reruns are in progress. This was a harness-management error,
  not grounds to relax any product assertion.
- Fresh own core, C/C++ support and renderer sanitizer build completed; selected
  native/render/transaction/source regressions and publication faults are running.
  No sanitizer pass is claimed here before those jobs finish.

Dependency453a532/baf6501 repairs are independently documented in
XATLAS-SHUTDOWN.md and already published by the parent. Latest public6b64e22 is
merged into this candidate; all private cut sources and the physically accepted
selection ancestry are retained. Canonical FLTK/BDF headers and runtime prefixes
are the eee23b7 assets verified unchanged for current DomeD d271cf6. No system
installation, active session restart or physical headset launch occurred.
