# Sculpt, polygon-field and texture-set composition

This source-only child combines the coherent Release candidate77e882bc with
the local-coordinate sculpt checkpoint8a05e5f3. Both original qualified trees
and their providers remain unchanged. This composition has not produced a new
executable or runtime qualification.

The real worker now captures eligible volume chunks while retaining owning
polygon39 and texture40 bundles for the packed stream. The Scene capture API
keeps texture-bundle authorization and sculpt-capture predicates as separate
arguments, avoiding accidental pointer-to-bool conversion or volume flattening.
The renderer retains tuple-keyed materials, polygon-field normal sampling,
local/scalar caches, authored/held placement uniforms and original Scene order.

Mixed material exclusion has an explicit typed-hit API. It preserves chunk
ownership and independent material identities on packed objects. The legacy
untyped API rejects a mixed namespace and checks its packed query before access.
The new regression creates actual field39, texture40 and generated-sculpt actors,
then tests sculpt/material exclusions, a sibling set remaining visible, and a
chunk-only scene without a fabricated packed BVH. These new controls have not
executed yet.

Six complete current units pass syntax: Scene, Document, material visibility,
renderer, sculpt capture and the mixed regression. The first observer stopped
after four passes because a new helper had no entry in the older compilation
database; the remaining helper uses the same core flags and both remaining
units pass separately. Inputs remain unchanged. This is not a build, sanitizer,
GPU or application pass. Fresh whole-owned providers and relevant native,
rendering, application and sanitizer checks remain required before admission.

The complete production objective remains active, including source-backed
per-set fields/history, resolution resampling, broader authoring, interoperability,
accelerators, physical devices and controller drawers after the basic targets.
