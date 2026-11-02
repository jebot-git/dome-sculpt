# Accepted authoring and Save — private qualification

Frozen product62d4747 generalizes the existing accepted-cut endpoint to generic
SceneOperation jobs. An accepted Bake/import/rig operation can now own a following
Save on the same ordered file worker. The worker stages an immutable complete
Scene before authority publication and records committed revision/content/
generation only after the transaction commits. A dependent Save captures that
endpoint, including after a later Undo; cancellation/failure forwards the original
exception and preserves the old file. No file-worker job waits on work behind
itself. Accepted Save requests drain before worker shutdown; operations without
an accepted Save retain their previous cancellation policy. Export ordering is
unchanged. The Scene archive format remains20.

AssetAcceptedSnapshot and the existing AssetWork completion fields are shared
with the separate UV completion bridgeb265706. A derived immutable publication
flag enables the actual universal Save/Save As entries while mutation remains
fenced. Spatial Save uses the same endpoint. The existing direct caller's
transactional admission and original failure bookkeeping stay authoritative.

## Completed scopes

The optimized complete FLTK/OpenXR build and fresh owned C/C++ core/support/
renderer/frontend sanitizer compilation pass. Native authoring-save30 assertions
pass with ASan/UBSan/leaks, including a real current-target Bake, failure identity,
Cancel, replacement, later Undo, eight-entry queue and observed Close predicate.
Twelve related registered sanitizer suites pass373.98s. The related15-suite
Release run initially has14 passes and one obsolete test expecting pending Save
to refuse. Test-only20fe1ad replaces that expectation with a real accepted Save
behind every actual same-controller retry; all90 direct caller denials plus
complete retries pass Release and ownedSAN. Test-only426b878 additionally checks
the shared spatial Save and immediate Close:32 native lifecycle assertions pass
Release and ownedSAN. These scopes are separate runs, not one all-green15-suite
or full-project sanitizer claim.

The actual App/Canvas sources in a test-only native FLTK consumer pass7 universal
menu/lifecycle checks. Save stays enabled, mutations remain fenced, the old file
survives pending work, and DomeD Ctrl+W queues Close behind Save. The normal exit
and an independent native project reader verify the committed result. The fixture
contains no production test hook. No new physical headset or Wayland claim is
inferred from this X11 consumer.

The real post-Bake publication/accepted-endpoint path has9795 measured allocation
boundaries. All9795 persistent denials and actual complete-helper retries pass
Release; the corresponding same-source owned sanitizer sweep remains live as
exec68235. Its finite progress/log is preserved; do not restart it, relabel it
complete, reduce its range or substitute cached preparation. No product/header/
archive mutation is permitted while that proof remains active.

## Next combined candidate and limits

Private0c6002b in artifacts/spacenav-scene20-b197e37 combines publicb197e37 with
this product, the corrected admission test, native3Dmouse/lifecycle5e6de1d and
the independently qualified portable UV Save bridgeb265706/a2912eb. Fresh complete
Release19552 and ownedSAN97041 builds are live. This candidate has no Scene21/22/23
normal-sampling or colourUV implementation. Qualify its actual combined source
before publication; component proofs alone are not a new combined pass.

3Dmouse's inherited Wayland shared-EGL allocation report and standalone XR112-byte
driver report remain explicit open lifecycle limitations; they are not suppressed
or attributed to this Save change. Keep the accepted physical selection baseline
and all original requirements. Controller drawers remain required after basics.
No physical launch, system installation or desktop restart occurred.
