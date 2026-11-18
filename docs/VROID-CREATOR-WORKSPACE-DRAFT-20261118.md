# Shared VRoid creator draft: implementation checkpoint

The original native creator already has 17 bounded scalar controls and nine
RGB tint components backed by the actual verified CC0 VRoid female/male bases.
Desktop exposes those controls. XR's Avatars page only creates neutral defaults;
it has no way to author body, face or hair settings. This slice fills that
specific workflow gap using the existing native generator and archive schema.

The new single creator page selects a native parameter and opens the existing
large ray/joystick value slider. Female/male selection, Neutral/Toon/Anime presets,
selected genuine creator-settings copying, local draft Undo/Redo and explicit
Create/Cancel share one native immutable draft. Copying parameters creates a new
avatar; it does not regenerate or replace an existing authored avatar. Existing
paint, source images, mesh/UV IDs, rest/skin and artist morph owners remain in
the old scene. The desktop dialog and quick female/male actions use the same
ordered creation/apply route. The accepted selection/18%-ghost interface remains.

Unconfirmed sliders cancel exactly. Draft history is separate from global
history; only successful worker publication adds a global creation transaction.
The draft has generation/revision/object/edit-authority and session guards.
Focus/session loss cancels unaccepted creator state; accepted work and its
immediately ordered Save retain the existing worker lifecycle.

This is an **unqualified implementation checkpoint**. Complete optimized
compilation, actual female/male per-control geometry/material witnesses,
source/UV/skin/morph preservation, deformation/export, native/controller history,
queued cancellation and stale-authority tests are next. Desktop and actual
synthetic-XR application evidence follow the native gate. No full creator,
physical-headset, arbitrary existing-avatar editing, hair guide/spring system,
polished face/eye library or main-product admission is claimed.
