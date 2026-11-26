# Independent full-size sculpt workflow — 2026-11-26

The native Document workflow now passes all 167 checks with four independently
authored production objects. The objects retain the original .0125-metre voxel
size, complete negative interiors, separate editable layers and sparse masks,
and over one million real triangles. This is scoped Release evidence, not full
application, strict sanitizer, sustained Arc or physical-headset acceptance.

## Owning source and actual result

Production is `6d3ca25e49692ce777f65c280a7deffb616b729f`, including shared-storage
fix `3aa7aea8`. Two fixture-only children correct setup and asynchronous waiting:
`87f6b41f` and `fe94b416`. Production `src/`, `include/` and third-party bytes
remain unchanged by those children. This source predates the separate sampler
integration; these sculpt results do not qualify unrelated normal workflows.

The actual optimized executable is
`6dafb5897f6f46816d687201abf74459dd852eb6d9c87eec81e0939713a5318e`.
Invocation `cecaf926339948ca9e3ce0ae11c2d66d` terminates successfully in
49.304 seconds, with sampled peak owned RSS 3,847,172,096 bytes below the
unchanged 4-GiB limit and within the original 900-second deadline. There is no
address-space cap. Actual executable/maps and 24 loader providers are recorded;
source/provider drift and owned-survivor lists are empty. The lease is released
at epoch 1795658933.9532776, device 40/inode 99618.

The workflow exercises actual native strokes and cancellation; independent
source-local whole-mesher position/normal bits; layer and mask editing; exact
Undo/Redo; ordered Save; fresh Document.open; durable recovery service, discovery
and actual Document restoration; one-byte-tight quota refusal; cancellation; and
continued validity of a previously acquired immutable geometry owner. Accepted
and recovery files and their hashes are recorded. No lowered-density, duplicate-
only, shortened-workload or relaxed-quota substitute is used.

## Preserved initial failures

The first native attempt exits with `Invalid sculpt brush`: the fixture reused
blend .1 after reducing its mask radius to .09. The unchanged product correctly
rejects blend greater than radius. Child `87f6b41f` gives mask setup a valid
blend, without changing production validation or the geometric workload.

The next actual product exits after 155.697 seconds at its internal 120-second
mesh wait. Recovery catalogue discovery is file-only; the fixture waited for an
unrequested initial mesh on a fresh empty Document and did not await the actual
catalogue predicate. Child `fe94b416` waits for recovery completion/errors within
the same 120-second bound. The full mesh wait and independent four-object oracle
still run after actual restoration. A separate nonblocking lease refusal occurs
before any loader/product execution and is not a functional test result.

## Build and pending qualification

Missing Nov18 worktrees/builds were reconstructed from Git. The fresh Release
core has 197 real archive members. The selected build plus two existing sculpt
regression consumers has 220 unique compiler owners. One support TU initially
compiled as an implicit generator prerequisite before header pins; that early
owner is not admitted. Its complete TU is explicitly recompiled after pins, and
its archive and actual dependent executables are relinked. Original attempts,
real MD files and old/new writer roles remain preserved. The corrected fixtures
rebuild only their affected complete TUs against the qualified unchanged core.

The matched strict native executable also builds and passes its ownership audit:
10 fresh changed/resource-context core owners plus 187 qualified unchanged
ASan/UBSan parent owners, with all 197 actual archive payloads checked. The parent
is the separately qualified fresh `05f5d1cc` strict build; original object/MD
paths remain intact. Strict runtime remains unrun. Two existing native sculpt
regressions are built but unrun. The original sustained independent Arc workload
(900 strokes, 360 masks, 6,000 stereo pairs across three repetitions) remains
required and unrun. No system installation or session restart occurs.

The machine receipt and compressed scoped evidence are in
`docs/validation/sculpt-independent-native-20261126.json` and
`docs/validation/sculpt-independent-native-20261126.tar.xz`.
