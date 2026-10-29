# Detached native skin-weight strokes

`edit_weights` applies a swept capsule to explicitly identified immutable mesh/
skin inputs. Each output carries the object ID and exact rest topology identity;
callers must check their source revision before publishing the linked outputs as
one history transaction. It does not change/restitch geometry, reorder joints,
change morph targets or alter original source provenance.

Callers provide positions in the brush's common coordinate frame. For editing a
posed avatar this must be the current skinned/morphed world-space position vector
for each part; an empty span explicitly chooses rest positions already in that
frame. The helper never guesses object/group transforms. A capsule is a volume
brush; visible-surface targeting and occlusion selection remain frontend work.

Add/subtract use pressure, strength, value and smooth radial falloff. Replace
interpolates toward value. Smooth samples the selected influence from the
original triangle neighbors and applies one Jacobi step, avoiding sequential
feedback through already edited vertices. This adjacency is local to each mesh;
linked material boundaries/UV-split vertices are not welded implicitly.

Hard joint locks are exact. Existing normalization handles pruning and influence
caps while preserving locks. If a requested assignment cannot satisfy locks or
there is no existing unlocked group to receive remaining mass, the vertex stays
exact and reports a constraint. No unrelated arbitrary bone receives the mass.
At most eight representative messages accompany exact aggregate counts.

Optional symmetry requires an explicit involutive joint map and X plane in the
same common coordinate frame. It reflects the capsule; names are never used to
infer humanoid roles. On overlapping capsules the stronger side wins; a
self-mapped joint receives the maximum falloff once. This works with authored
offset centers and asymmetrical geometry without pretending a bijective vertex
map exists. Exact topology-based mirror/nearest-surface transfer remain separate
`rig.hpp` operations with their existing validation and distance bounds.

Every input validates before output calculation. The operation bounds aggregate
vertices, part count, influences, coordinates (±1,000,000 units) and work; cancellation or resource failure
throws without changing any input or returning a partial result. The frontend
must keep detached results off the XR render/input threads and publish on its
existing guarded scene worker.

Fixtures cover geometric capsule selection, supplied posed coordinates, explicit
symmetry/offset planes, immutable smoothing, feasible/impossible locks, pressure,
identity/budget/cancellation failures and actual nineteen-part CC0 VRoid linked
weight strokes with unchanged topology/source joint mapping. This is a native
editing kernel; desktop/XR controls, brush visualization and scene history
publication are separate integration work.
