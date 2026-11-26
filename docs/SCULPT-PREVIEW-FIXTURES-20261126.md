# Sculpt preview fixture corrections

Fixture commit `b060ef56`, based on `f9c20398`, corrects four broad-regression
failures without changing production code or returning Volume previews to flat
packing. The original `c26a8573` CTest log snapshot and executable identities are
preserved separately.

- `scene-authoring` checks both genuine visible chunk owners and positive
  geometry, then checks that hiding one object removes its owner.
- `native-live-preview` inspects the actual chunk geometry. It retains the
  gesture/sample identity, unchanged authoritative scene and exact cancellation
  checks, verifies a second sample changes geometry, and checks the earlier held
  preview remains unchanged.
- `sculpt-mask-document` checks every scalar packet against its exact geometry
  owner and independently samples persisted protection at every local vertex.
  It requires nonempty visible protection and verifies hiding the overlay drops
  its mask/scalar owners.
- `sculpt-mask-controller` gates the actual `SculptSceneChunks::capture` return,
  replacing a legacy `Scene::preview_surfaces` hook the worker no longer calls.
  Pressure before first display publication, controller controls, focus rollback
  and existing routing checks remain intact.

Four complete fixture translation units and four links use the parent's
qualified `f9c20398` Release core, SHA-256
`a047845528052bdd2417a2946c70a90f5ef9e8ee179e2dc54d5aa5533cbc3c23`.
No production objects are rebuilt. All five focused CPU cases pass: scene
0.636s, live preview 0.231s, regular native strokes 0.540s, mask document 0.179s
and mask controller 0.200s. The run uses default allocator settings and a private
`/dev` with no DRI nodes, requires no graphics lease, and has no input drift or
cleanup signals. The short transient unit auto-unloaded before its later stop;
that stop's exit 5 is retained rather than replacing the successful run.

This is a focused Release fixture result, rather than sanitizer, graphical or
whole-umbrella acceptance. The other broad-suite failures retain their own
investigation and validation requirements.

The [record](validation/sculpt-preview-fixtures-20261126.json) indexes the
[evidence archive](validation/sculpt-preview-fixtures-20261126.tar.xz): 50
members, 108,912 bytes, SHA-256
`91b39d40b43293a28de3f125d57622bc95462a40da6e25db00d2371318d05327`.
Every member and local authority was independently rehashed after packing. The
captured umbrella prefix was also checked against the unchanged prefix of its
original append-only log.
