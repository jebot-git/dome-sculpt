# Independent seam-key domain: private design and detached prototype

This is a proposal on9050, not admitted authoring or a new file format. The
original native seam-conflict refusal remains in force. No public headers,
Scene/material/paint codec, CMake target, native admission or master source has
changed. The new private kernel only returns detached geometry and two maps.

## What is missing

The saved native quad has four vertices before UV editing and six afterwards.
NormalFrames already stores every base/target N/T by ordered triangle corner.
Its original native vertex domain, FrameTargetIdentity.positions, uv_origin,
normal_frame_current_authority and prepared-key recursion still require the
six current copies to contract to four prior native vertices. Different edits
on two copies cannot fit that authority. Removing the equality check or choosing
one side would destroy a real edit.

A prior native frame vertex and a raw ImportedPrimitive attribute/source vertex
are different identities. A derived six-vertex native basis must not become a
claim that the original raw asset contained six vertices. Original source IDs,
source corners, target IDs, raw bytes, and artist role arrays stay retained.

## Implemented private geometry slice

expand_corner_key_basis(current, origin) requires exact ordered triangle REST
position correspondence. It partitions vertices by (current native vertex,
prior native frame vertex), retains both maps, copies prior frame UV/N/pins and
ordered seam flags, and never chooses a nearest vertex or averages a delta.
The structural result is not an authenticated paint/material/source witness.
The eventual factory must additionally prove the owning prior frame/paint and
its corner correspondence. Unreferenced current vertices currently refuse in
this prototype because corners cannot invent their missing prior certificate.

On the actual saved4-to6 fixture, opposite +.013/-.017 edits remain independent.
The previous contraction correctly still refuses. The existing bounded
normal_frame_remap_deformation broadcasts every old key and skin influence by
the retained native map, preserving pose and weights. An independent triangle
cross-product oracle measures normal error0 and a UV-Jacobian oracle measures
tangent error5.96046e-8 for a genuinely geometric test frame. This does not prove
reauthored artist-offset, mixed-role or skinned normal pixels.

## Proposed owning representation (awaiting final API agreement)

Use a geometry-only immutable FramePositionDomain leaf with prior and derived
native bases and exact vertex/corner maps. A NormalFrameSet may reference this
leaf; its POSITION arrays and origin_vertices then name the derived native
basis, while source_vertices/source_corners keep their actual original raw
identities. Derivation copies all immutable role base/target corner arrays and
handedness exactly before the requested target is explicitly authored.

A separate owning SourceNormalKeyDerivation on the normal preparation retains
the actual before-Paint/material/deformation/appearance, domain, stable target
identity, explicit key edit and resulting frame set. It proves the old recipe
first, independently replays the derived attribute operation, and validates
current ordered REST/frame association. It does not rewrite independent colour
history or copy a private normal-metadata seal as proof of new key attributes.

Existing intrinsic/resolved normal planes, normal-specific coverage/event,
current writable attachment/layers/baked/PBR/mask grids, material roles, current
colour descriptor, current polygon and first rig source remain exact. Sampling
metadata is still proved by its genuine previous owning normal node. New key
attributes need their own derivation proof. Preview seals/cache keys must carry
both authorities before publishing the new frame set. Later UV nodes may own a
before-material with a derived key basis; later key nodes may own a UV node.
The graph remains acyclic: the geometry leaf has no Paint/material backlink.

Capture the complete old object with capture_polygon_origin before conversion.
Preserve the first shape source as a reachable immutable capsule and keep rig
sources/placement/raw source closure. Restore must return exact old geometry,
paint, keys, roles and sources. Cancel/Undo/Save use the existing detached worker
transaction and accepted endpoint bridge. No current data is published before
all replay, quotas and allocations succeed.

## Conditional format/capability requirements

Tentative versions are frame archive2, normal entry5 and material archive10,
with reserved minimum Scene22. This is not yet a final codec contract. Existing
no-key version1 frames and material1..9 bytes must remain exact. Old readers
already reject frame2/material10 and Scene22. If the reserved22 cannot safely
express the new capability, use27 instead of reinterpreting an old record.

Current readers explicitly reject22; Scene23 requires material8/polygon5 and
Scene24 requires material9 plus actual paired writable domains. Material9 also
uses exact numeric tests for Paint2 and normal metadata entry4. Adding material10
therefore requires explicit witnessed capabilities, never a maximum-version
raise or generic >= rule. Scene22 must have an actual key derivation; outer23
must still prove actual colour UV; outer24 must prove its real writable/current
colour owners; outer25/26 must recurse rig/source/extra-UV history correctly.
Material10 is emitted only for a reachable key node. Mixed older features may
coexist inside10 only with their actual owning colour/normal/paint pools.

Every current and retained Polygon/shape/rig/normal/colour before-edge needs
unique-owner resource charging, minimum-version scanning, pool alias retention,
reachability/orphan rejection, cycle/depth limits, early standalone rejection,
and allocation preflight. Existing16M paint texels,256MiB paint,8M deformation
entries (including16 per exact pose matrix) and frame/domain limits stay bounded.
Geometry keys include the domain/frame owners; UI, picking, renderer, native
worker, GLB/VRM and all changed ABI consumers require fresh coherent builds.

## Next connected slice and remaining work

After the leaf contract is settled, implement its owning native/frame factory
and direct Create/Replace on the actual seam fixture, then the versioned normal
key derivation replay. Qualify opposite seam edits with independent geometry,
role N/T and mixed pose/morph normal pixels, exact current paint/event/source,
Restore/Undo/Cancel/restart and format export. The geometric transport already
used by authored_attributes must be factored without replacing artist offsets
or changing latent PBR/MToon role conventions.

Mirror additionally needs a topology/corner-aware symmetric correspondence:
nearest-position matching can swap coincident seam copies. Freehand must edit
the genuine selected native copies through the same factory and publication
boundary. Create/CaptureMix/Duplicate must retain stable targets and current
mix, followed by all these connected paths. None is complete by a compatible
seam-only workaround or by leaving the present refusal in place.


## Owning factory WIP continuation

The next isolated child appends a universal material SourceKeyDerivation owner
as well as the optional paired prepared-normal entry handle. A no-normal path
will not need a dummy projection: the geometry leaf and replay are independent.
The first dispatch currently implements prepared Create/Replace/Rename; other
commands and the generic no-normal dispatch remain to connect. Original role
coordinates are expanded from the actual uv_origin maps/source arrays for
UV0..7, independently of the current edited atlas. The universal node keeps a
fixed actual before-material anchor plus per-target endpoints, so repeated
Replace/strokes do not add an archive edge per update. New UV/domain conversions
may keep a genuine prior domain/node. Old frame/material encoders explicitly
refuse this WIP representation. No new reader or writer is admitted yet.
