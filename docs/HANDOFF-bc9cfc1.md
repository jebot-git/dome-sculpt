# Native authoring workflow checkpoint

The full production goal remains active. This is an extension of the existing
native FLTK/OpenXR editor. The latest user requirements also include Wings3D-style
polygon editing, individual mesh visibility and exclusive held-mesh ghost mode,
VRoid-only humanoid bases and standard FBX/OBJ import options; those separate
agent tasks remain in progress.

See [AUTHORING-JOURNEY-CHECKPOINT.md](AUTHORING-JOURNEY-CHECKPOINT.md) for the
usable source-preserving workflow, strict bake options, generated tangent/atlas
repairs and exact qualification scopes. Earlier procedural details are preserved
in [HANDOFF-8c32179.md](HANDOFF-8c32179.md). This candidate has not been installed
or physically tested in the headset; the user ended the earlier rudimentary test.

The primary worktree retains published main bd6e36a and the full requirements.
A separate integration tree combines generic Document transactions, VRM material
role fixes, VRoid-only bases and Scene 11 authored-bind/FBX/OBJ capture. Merge this
checkpoint there and rebuild every consumer because bake option/report layouts
changed. Do not publish the combined default until its own checks pass.

No system installation, session restart or automatic headset launch is authorized
by this checkpoint. Preserve the old primary-worktree stash and every separate
agent checkpoint. General colored-source baking and the other uncompleted original
requirements remain required; passing this workflow does not complete them.
