# Held-object authority kernel checkpoint

This private kernel checkpoint is an intermediate milestone. The native
Document/controller authority tests pass26 checks; desktop menus, held gesture
UI and GPU ghost/motion qualification follow separately. It does not claim the
complete held-object feature is finished.

ObjectInfo appends editable/ghost flags and an optional current world transform.
EditorState appends object_grab_mode/isolate_held/object_grabbing, held_object and
a monotonic edit_authority. AssetSource adds an optional edit_authority lease;
legacy three-field callers retain admission compatibility, while actual queued
jobs independently capture and verify current authority. SurfaceRange appends
worker-authored object_world/world_frame_valid for later per-range GPU motion.
These are ephemeral publication fields; no Scene/native archive schema changes.

The central object_editable_locked(id) requires a visible unlocked object and,
while held isolation is active, the held identity. invalidate_object_authority_locked()
cancels strokes, controls, UV/retopo/cut drafts and queued authoring, then advances
the epoch. Polygon integration will add its cancel hook there. New motion and
changed polygon drafts are explicitly exclusive; pinned held editing remains
available. Future affine object matrices must replace the same centralized world
transform path, not bake preview transforms into source mesh data.

The authority applies to synchronous/native strokes, preparation, sculpt/skin/
bake targets, transform/material mutation, UV acceptance and asset-worker
publication. Generic isolated Scene operations verify unchanged protected COW
owners, revisions and metadata, including group authority, without serializing
or hashing full meshes/images. Ghosts remain eligible read-only bake sources.
Dedicated individual visibility controls remain available.

Held motion keeps a local preview only, with no Scene/revision/geometry changes
per hand frame. Release commits one transform history node; cancellation retains
the source. Controller default squeeze still moves the workspace. Explicit object
mode uses one/two-hand navigation on the object and preserves workspace placement.
History restoration and document replacement release obsolete held identities.
A complete GPU/frontend milestone must prove no static-buffer uploads during
motion/ghost toggles and ordinary pixels unchanged after release.

The required colored-target bake Scene10/PBR-plane/quality receipt work remains
parked, incomplete and preserved at artifacts/color-bake-be1858c with its handoff
and hashes; COLOR/UV morph channels also remain required.
