# Owning sparse HDR scene data — private checkpoint

This extends the frozen `f072b86` sparse emission data prototype with actual
Scene27 persistence. It composes the independently qualified `b35b517` unlit
repair. It is a storage/ownership checkpoint, **not a usable HDR painting,
source-import, renderer, frontend or export milestone**. The full production
goal remains active. The separate runtime child owns those unfinished adapters.

Only actual physical paint selects Paint3/Scene27. Hidden and zero-opacity
physical leaves remain data. Version selection traverses current paint, material
before-paints and complete polygon/shape/rig histories, including source25 poses,
source26 raw UVs, independent writable Paint24 and normal ownership. Shared
capsules retain aliases. Aggregate float-tile storage is charged before decoding;
the existing 256 MiB scene and 128 MiB paint bounds are unchanged.

A material archive containing physical before-paint uses conditional material11
with an explicit `base_version` equal to its actual historical table shape1..9.
This capability does not authorize the separate, unmerged key/frame10 format.
The reader validates actual base features, nonempty physical reachability and
all paint/origin roots; a larger numeric marker cannot discard or invent data.
Old Paint capability overloads continue to refuse Paint3. No new public layout
was added beyond the separately rebuilt f072 prototype. Native material factors
remain unit bounded here; absolute backgrounds and source HDR admission are not
implemented in this checkpoint.

Six direct consumer executables pass in Release and whole-owned-support
ASan/UBSan/leak mode: physical paint50, new owning scene37, existing scene,
compatibility33, source25/26 version22 and normal/current-colour94. The new37
checks use genuine native factories: paired Scene24 normal/current-colour UV
preparation; raw unused UV2 import; shared first rig/shape capsules; and dormant
UV material before-paint. They verify exact CBOR restart, source bytes, immutable
normal/bake/domain owners, quotas and malformed/orphan/down-version refusals.
The50 paint controls include22 measured cancellation polls and10 actual
persistent allocation boundaries. These are direct executions, not a full
CTest, application or device qualification. The unchanged normal-colour fixture's
sanitizer compile+execution took2061.01s; it was run directly, so its configured
600-second CTest timing gate is not established here and was not increased.

The build scope is deliberately precise: four complete affected core translation
units replace their old members in a copied immutable f072 core; the six support
archives remain byte exact. Complete fixture translation units are rebuilt with
matching public headers. Dependency audits and linker maps preserve that scope.
This is not a fresh full application or renderer build. Each profile preserves
1490 source inputs, before/after artifact manifests and all command logs.

A separately compiled old/new Scene consumer writes the same ordinary legacy
emission-painted58291-byte CBOR (SHA256
`646ab857415d5d5bebb485f8d1f3a4002722ff166c476c5b02cac4aae5cf763c`).
The actual old f072 Scene reader rejects the new file as unsupported Scene27.
Preserved failures include missing physical capability during polygon policy
inspection, a legacy material preflight rejecting11, null-painted objects during
raw-source import, and a misaligned test-only UV accessor. The latter was fixed
by padding the added accessor; no parser guard was weakened. Initial missing
arguments/full-JSON-header setup failures remain separate artifacts.

The preview and UV-transfer guards from f072 remain intentional unfinished
admission boundaries. Standard HDR export precision, actual sparse GPU planes,
physical/legacy MToon blending, live native/controller controls, history/Save
workflow and actual Seed-san source admission still require implementation and
independent consumers. No GPU, physical headset, installation or session action
was performed for this checkpoint.
