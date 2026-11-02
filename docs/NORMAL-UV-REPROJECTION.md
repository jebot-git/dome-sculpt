# Prepared-normal sampling atlas changes

This private checkpoint extends the original native FLTK/OpenXR application.
It does not complete the production requirements or the normal-authoring work.

The existing UV Apply worker can now transfer an explicitly prepared normal
model to a different valid UV atlas. Original PBR/MToon normal and tangent
attributes, every directional target, stable target identity, current skin and
pose remain authoritative. Atlas changes affect sampling; they do not replace
these axes with derivatives of the new atlas.

The transfer reprojects the independent intrinsic PBR/MToon planes and retains
paired baked detail under `PreparedAttributes`. Editable layer identities,
channel coverage, masks, hidden layers and material parameters follow the
existing source-aware transfer. Raw encoded vectors retain their magnitude
before filtering; changing the atlas must not silently normalize each texel.

Each immutable `SourceNormalUvReprojection` owns its exact before-paint,
material, appearance and deformation, an ordered-corner/vertex correspondence,
the target UV layout and the resulting planes. Its bounded sampled-quality
receipt records the accepted/observed angular error and actual work counters.
Deep admission replays those exact inputs and compares resulting plane bytes
and counters. Equal-looking unrelated paint owners cannot replace the captured
source lease. A repeated UV change retains both replay steps: resampling twice
is not silently interpreted as a direct resample of the first source.

Persistence uses Scene19, material archive5 and normal-preparation entry2.
Existing normal-preparation entry1 and archive4 semantics are unchanged. The
writer retains the first raw original, initial planes, fixed attributes and
all intermediate before-states. Decode checks versions, graph ordering,
dimensions, indices, flags and resource limits before publishing. The layout
archive has a global128MiB bound; existing paint/image/deformation limits remain
in force. Layer-transfer scratch rasters are retired before allocating the
separately budgeted normal-projection workspace. Nested appearance17 and
owned-UV1 paint18 minimum-version detection
is retained. This does not admit UV1 normal preparation.

UV transfer cancellation is the separately qualified `f63108f` fix from public
824f6cd. The worker uses a real `AssetWork` token, checks it before final model
publication and retires only its matching token. Scene19 tests exercise actual
mid-transfer cancellation, replacement, ready-draft cancellation, Save/reopen,
one Undo/Redo and held-only editable authority. No stored visibility, locks or
materials are changed by holding/isolation.

## Qualification

Exact source, binary, archive and log hashes are in
`validation/NORMAL-UV-REPROJECTION.json`.

- Twenty-one adoption/archive assertions include duplicated seam vertices with
  exact skin rows and every POSITION target, restart, malformed identities and
  versions, source-owner rejection and late cancellation inside uncached source
  layout replay. Worker callbacks reach new replay validation before generic
  binding validation can populate its immutable proof cache.
- Twenty-three native/controller assertions exercise the real UV worker and
  joystick input dispatch. Seven selected CPU/software suites pass; eight fresh
  whole core/support/renderer O1 ASan/UBSan/leak suites pass; the receipt records
  final timings and exact binaries.
- Forty-six independent graphical assertions pass with software GL and actual
  Arc A770. Two real UV Apply/Save/reopen steps precede export. Three joint
  poses and morph weights0/.35/1 compare curved native PBR, legacy MToon0 and
  the core PBR fallback of the same VRM. The27 role/pose comparisons have
  maximum byte errors0 for PBR and1 for MToon. Separate normal-map sensitivity
  checks prevent a vacuous comparison inside a saturated toon band.
- All18 final emitted files have0 Khronos errors. Their27 warnings are18
  retained-parent skinned-mesh warnings and9 legacy VRM extension-name warnings.
  All9 VRM files pass the15 pinned official VRM0 schemas.
- Thirteen actual native FLTK/DBusMenu checks pass under a private Xvfb using
  the canonical bitmap runtime/toolkit. These cover Unwrap, local Undo,
  Apply/Save with a new owning receipt, exact global Undo/Redo, Seam/Stitch and
  Ctrl+W cancellation. The starting chart is deliberately nonconformal and the
  seam point belongs to its internal diagonal; the ordinary fixture defaults
  in `tests/uv-menu.py` remain unchanged.

The graphical consumer parses emitted files and independently evaluates image
sampling, morphs, mixed inverse-transpose joint normals and named shader
interpolation policies. It does not use the production frame/skin evaluator.
These comparisons establish native/export parity for the tested current model;
the sampled local angular receipt covers intrinsic planes. It is not an
all-pose error bound against the pre-resampling image, an additional bound for
inherited baked-detail resampling, or a guarantee for arbitrary rendering
engines.

## Required follow-up

The scoped prepared MToon input remains static, UV0, linear and clamped. Unknown
sampling/frame policies and reprojections outside the accepted quality/resource
contract refuse without changing the source. Historical canonical normal
payloads are not relabelled as this model. Prepared meshes as normal-bake
sources still need their real source-frame adapter.

Creating/replacing a key after a normal UV reprojection still requires the
source-retaining key adapter. The current guard is temporary staging, not
completion. That adapter must preserve the full old prepared state, stable
target IDs and current paint/detail planes while authentically rebuilding the
owning key/UV receipt chain, including seam remaps. Arbitrary curved/mixed-skin
normal conversion, broader UV1 sampling, actual avatar atlas workflows and all
remaining rows of REQUIREMENTS.md stay required.
