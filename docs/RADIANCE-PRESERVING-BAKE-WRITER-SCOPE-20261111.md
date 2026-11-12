# Archive-bound FLOAT writer scope

The first owning Release compile from `8b514a34` completed all243 steps with
242 pinned owned outputs and no source drift. Its source and build remain
unchanged at `artifacts/radiance-preserving-bake-20261111` and
`artifacts/radiance-bake-build-20261111/release`. Compilation is not runtime
admission. Source review found its JSON feature scan could mistake opaque
retained MToon metadata for actual Baked6 paint. The child fixes that writer
rule without changing any of the103 public headers or the current8b ABI.

`BakedEmissionWriteScope` isolates validation and nested Scene encodes from
entry. It then binds to the owning Scene's actual `PaintImageArchive`.
Successful `PaintedMesh::encode` notes an emitted field only when it receives
that exact archive. Null/foreign-archive encodes used for discarded hashes or
validation cannot grant the feature. Opaque material JSON is never inspected.
Nested writer scopes restore their predecessor on normal/exceptional unwind;
no callback, cache or flag survives the owning encode. Reader authority remains
the independent explicit private capability and actual decoded-field count.

The new Baked6 payload also rejects unknown interpretation keys instead of
silently dropping them. Old Baked1–5 key behavior is otherwise unchanged.

The existing native fixture gains a bounded `--writer-controls` mode. It uses
actual retained MToon metadata (bare and forged paint-shaped/binary records),
real field serialization with exact/null/foreign archives, a nested actual
Scene encode and writer exception unwind. It also covers a FLOAT field retained
only through persisted colour-preparation history after the current bake is
cleared, and a genuine additional texture-set field with legacy primary paint.
The latter requires Scene41 around the exact Scene40/Paint7 owning record, and
rejects stripping that envelope. No trusted persisted model container holding
an entire nested Scene was identified; existing history containers serialize
their actual native paint/material/geometry pools. The direct nested encoder
control proves scope isolation without inventing a new persisted container.

Qualification must preserve the original8b compile/provenance. The authorized
focused closure may reuse only immutable current8b providers with all103 public
headers byte-exact, unchanged source/assets/SDK inputs pinned, and every TU
depending on the changed private codec header rebuilt according to actual
Ninja dependencies. Complete core members are replaced in a new archive and
all11 fixture endpoints freshly linked; unchanged member bytes are compared.
No main/a904 ABI object is eligible. Counterfactual diagnostics, actual native
outcomes, original failures and post-run pins must be recorded separately.
