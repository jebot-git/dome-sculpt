# Native VRoid creator draft and explicit hair recolour — 2026-11-18

The new XR Avatars / Creator controls workspace creates a distinct avatar from
one of the two bundled, hash-verified CC0 VRoid bases. It exposes the existing
bounded body/face controls and fitted-groom controls, Neutral/Toon/Anime presets,
and explicit RGB hair recolour. Copy settings uses a genuine saved creator
recipe; it does not reconstruct or replace an existing authored avatar.
Previous/Next and the existing large scalar slider keep parameters in one
workspace. Draft Undo/Redo and Cancel do not edit the Scene. Apply uses the same
ordered native worker as the existing FLTK Create dialog, with one global
history entry and ordered Save. Existing authored geometry, paint, material,
source, skin, rest and morph owners are retained.

The original hair factor is linear RGB `(0, .3967553, .6038274)` in both bases.
Its old multiplicative red tint cannot change a zero channel. Both original
1024² main/shade images are grayscale, with their authored alpha retained.
The explicit optional `hair_recolor` operation sets linear RGB base and matching
shade hue while retaining the original shade/base luminance contrast. It also
colours the fitted groom's actual strand pixels. Original image bytes and raw
material/VRM metadata remain immutable provenance. RGB edits seed from the
current original-plus-legacy tint; switching recolour off restores that recipe.
The desktop dialog has an explicit checkbox and RGB fields.

Absent recolour still encodes the exact existing controls version2. Presence
encodes version3 with `hair_recolor: {version: 1, linear_rgb: [r,g,b]}`. Existing
`hair_color` values keep their old multiplicative meaning when the override is
absent. No MakeHuman generator or external model/frontend is introduced.

Native acceptance currently passes **352 controls/export checks** across both
bases and **44 actual Document controller/workspace checks**. Every one of the
26 continuous controls has a measured real geometry, material or generated-groom
output effect. The tests retain original UV/skin/source/morph identities, test
real rest/posed/morphed output, exact save/reopen, sampler and ordered-corner UV
transport, and independently decode emitted GLB/VRM images and factors. The GLB
exporter legitimately bakes colour into PNGs with factor1; the effective colour
oracle handles that representation. Original assertion/compile/setup failures
remain preserved. Exact source roles and terminal/unit/lease receipts are in
[the native receipt](validation/creator-native-colour-workspace-20261118.json).

This is a native/controller milestone. Public GUI/XR and strict sanitizer
runtime qualification remain pending. The current positive VRM colour case is
neutral, source-backed19 parts; styled-body VRM currently refuses nonlinear
first-person/collider metadata, and the generated20th groom has no fabricated
original source/MToon identity. Creator-owned metadata refit and genuine generated
part transport are the next required implementation. This does not complete the
anime creator, arbitrary existing-avatar editing, full hair guides/physics,
age profiles, or the remaining rig/shapekey/VRM requirements.
