# Radiance-preserving Bake: typed field milestone

This private branch starts from `653074b40990da7e43efc44647af974619994873`.
The first milestone implements and tests an immutable FLOAT field. It does
**not** admit physical Bake in the application yet: existing guards remain.
Worker adoption, native scene capabilities, rendering, export and actual
public workflows require the subsequent integration milestones.

## Existing ownership and factors

`BakedTextures` currently owns RGB8 color/ORM/emission, optional resolved PBR
color, common R8 coverage, and a separate normal interpretation/authority or
retained normal field. Editable layers remain separate, including hidden
layers, independent physical-emission tiles, masks and opacity. Geometric
`BakeResult.coverage` is a status map (hit/padding/missed/protected), not FLOAT
emission replacement coverage. None of these owners may be repurposed as the
new emission authority, and none may be silently flattened or discarded.

Old `bake.cpp` applies source material factors once to imported maps
(`maps_are_composited=false`), while composited RGB8 maps already contain their
material factors. That old RGB8 Bake output remains its existing format.
On typed/overlay emission paths, retained RGB8 baked emission is interpreted
as a legacy-relative coefficient times the actual typed material strength;
ordinary RGB8 composites keep their existing behavior. Changing these old
interpretations is not part of this extension. A legacy-relative coefficient
must not be relabelled as absolute radiance during a new/old base merge.

New physical Bake output will store the evaluated source radiance as an
**absolute** replacement term. Its source factors are already applied. The
target's later legacy material strength, including zero, must not multiply
that absolute term. The original source binding, actual current geometry/UV
and normal frames remain sampling authorities. An old RGB8 preview is not a
permitted substitute for the actual physical source field.

## Version-1 FLOAT field

`BakedEmissionField` owns seven little-endian IEEE FLOAT32 coefficients per
texel: premultiplied legacy-relative RGB, premultiplied absolute RGB, then
independent FLOAT coverage. Version-1 wire records contain an eight-byte
`DOMEEMIT` magic, version, square resolution, sample count and payload length.
Exact lengths, version, dimensions, budgets and all coefficients are checked
before allocating decoded retained storage. No caller buffer aliases the
immutable field. Encoded bytes preserve FLOAT bits exactly.

For coverage `a`, relative coefficients lie in `[0,a]`. Absolute coefficients
are finite and nonnegative, bounded by the correctly FLOAT-rounded product
`FLT_MAX*a`. This is resolved radiance, not an editable paint value: the
editable physical-paint bound of 10000 and old interchange's resolved bound
of 30000 must not clamp it. The owning adapters must retain this distinction.
Coverage zero forbids hidden energy. Invalid/overflowing results fail before
publication; no epsilon repairs, energy clipping, invented coverage or
UNORM8 conversion are used. At an unrepresentable subnormal interpolation
boundary the operation refuses rather than dropping energy.

Resolution remains 8–4096, independently of the native PaintedMesh 64–2048
range. Dense retained samples count against the unchanged 128 MiB paint cap;
over-budget fields refuse before allocation. Other retained channels still
consume the same owning cap, not a second field allowance. Grid 2049 remains
supported by this standalone type when it fits; a dense 4096 field cannot fit
the existing cap. No new blanket resolution cap is introduced.

Bilinear sampling uses clamp-to-edge texel centers and filters all seven
premultiplied coefficients. Over-composition uses `above + below*(1-a)`.
Resolution evaluates `background*(1-a) + relative*strength + absolute` using
checked binary64 intermediates and a final FLOAT range check. Source strength
may be any finite nonnegative FLOAT; valid relative multiplication alone is
bounded, but composite addition can overflow and is explicitly checked.

## Integration obligations

The planned baked payload6 must retain the exact old color/normal payload1–5
interpretation and independent normal identity, with a genuine native scene
capability (Scene41 is currently unallocated by other lanes). Scene40 texture
sets and Paint7 capabilities must compose through actual owner predicates,
not version-only exemptions. Native preflight/storage accounting must include
FLOAT bytes and reject unexpected/fake capability records.

Actual Bake must sample physical source fields and frames, construct absolute
FLOAT output only for projected/padded hits, preserve protected/missed regions,
and retain unrelated channels, normals and all editable/hidden layer owners.
Base merging must preserve relative and absolute terms and independent FLOAT
coverage; common R8 coverage cannot become emission authority. Prepared-source
and retained-frame guards remain until their actual adapters exist.

CPU sampling, immutable preview packets, actual GPU uploads/shaders and ledger
budgets, native worker/source/generation/cancel/adoption/history/ordered Save,
reopen and supported export must agree. EXR must use true FLOAT32 without
half conversion/clamping, or refuse unsupported data before publication.
All owning consumers must be freshly rebuilt after public ABI changes; old
core objects cannot provide conforming evidence even when layout is unchanged.
No GPU/application qualification has been performed for this milestone.
