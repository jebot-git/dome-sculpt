# Current-corner colors as a Bake source

This private milestone permits the native Bake workflow to sample an authenticated
current-corner RGBA source into an ordinary target. It validates the exact ordered
REST derivative before evaluating skin/morph pose, supplies straight linear RGBA
through the existing separate PBR bake path, and preserves the complete source,
including its immutable corner basis, paint, original asset, shape source and
placement capsules. It does not manufacture creator identities or reinterpret an
Origin as current color authority.

The native source picker and detached helper use the same conservative supported
role domain. Explicit **Prepare source colors** remains required for nonidentity
source color. Current-field targets, UV1 material roles, prepared color/normal
source roles and unsupported artist-frame transfers remain refused. Selected-key
sculpt mode disables Bake until the mode is finished, so the operation cannot
silently substitute saved REST geometry for the displayed preview.

Preparation, projection, validation and callbacks run before the detached Scene
becomes authority. A final progress callback that throws or requests cancellation
leaves the original Scene and its owners unchanged. Native worker publication
continues to use the existing guarded asset transaction, one Undo/Redo and the
existing Save/reopen route. The unchanged ordinary target receives truthful
Identity color preparation; that is not used as a substitute for source corner
provenance.

## Exact proof scope

The source is based on private `a54cc76`, retaining the separately qualified cut
Apply admission fix `386be29`. No Scene/archive or public DTO layout changes are
introduced by this source adapter. The manifest records exact source and binary
hashes rather than implying that unrelated newer mainline features were tested.

- Fresh Release: five selected suites pass, including 49 native/direct checks,
  133 independent renderer/export checks, and three existing color regressions.
  A final seven-suite run also passes all source/fault fixtures after partitioning
  the precommit sweep. The two runs cover ten distinct selected test names.
- The independent renderer covers 16 inputs: four alpha modes, zero-alpha seams,
  and actual skin/morph pose. Source-to-baked-target full-image error is at most
  two encoded levels. All independent cgltf/PNG/GLSL exported-target comparisons
  have zero pixel error. Save/reopen retains complete source data exactly.
- Persistent owned `operator new` denial exercises all 858 allocations inside
  actual field packing, and all 4,393 allocations in the actual precommit phase.
  Each denied boundary checks exact Scene/owner retention and retries the full
  helper. These are two declared phase scopes, not a complete preparation sweep,
  foreign-driver failure test or universal allocation-transaction claim.
- Actual FLTK/DBusMenu checks pass: current source 9, compact 9, legacy source 9.
  Both current screenshots were inspected; the compact completed report scrolls
  while its action buttons remain reachable.
- The actual OpenXR loader fixture passes 11 checks across accepted Bake,
  cancellation and focus loss, with balanced GL image ownership for both eyes.
  It uses the synthetic loader, not a physical headset.
- Final fresh own-core/C/C++ support/render ASan/UBSan with leak detection passes
  all ten selected suites in 794.36 seconds, including every one of the 858 and
  4,393 fault boundaries and full-helper retries. External system libraries are
  not claimed instrumented. The initial unsharded precommit run hit its 600-second
  fixture timeout after 2,144 passing boundaries; four measured disjoint shards
  retain the complete original assertion set and cover 1..4,393 without gaps.
- The final independent renderer/export fixture also passes all 133 checks on
  the actual Intel Arc A770 selected explicitly by PCI address. Full-image source
  error stays at most two levels; all sixteen independent GLB comparisons are
  zero. This is hardware OpenGL with CPU computation, not a new OneAPI proof.

The FLTK build uses the verified DomeD `d271cf65c612a462fcfa6739416b06c6565be070`
compatible `toolkit-eee23b7` and `bitmap-runtime-eee23b7` prefixes. Builds run as the
regular user. This milestone does not install software or restart a session.

## Remaining target work

Current-field target preparation needs a durable geometry/corner-bound projection
receipt, real current-field sampling, exact retained-origin admission and native
Save/history/export proof. Scene20 is reserved for that work. Topology/UV/normal
transfers remain guarded until their own adapters prove complete authority; this
source milestone does not claim the full Bake goal is finished.
