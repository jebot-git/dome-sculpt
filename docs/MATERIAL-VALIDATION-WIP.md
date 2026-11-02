# Shared material validation — private resource checkpoint

The per-top-level-call ledger is confined to synchronous validation. It reserves
one bounded record before descending, records successful cheap/deep completion,
checks the active path before consulting proofs, and clears every raw pointer on
normal or exceptional root exit. It has256 unique-binding and64 active-binding
limits. Deep requests cannot reuse a completed cheap proof. No proof survives a
call; no allocation or unbounded container is added by the ledger. Association,
receipt, geometry and owner validations are unchanged.

Fresh coherent Release7/7 passed156.09s: existing colour/normal scopes, actual
joint normal/current-colour native94 and the new real DAG fixture. For the same
genuine five-node graph the new implementation enters11 cheap/18 deep binding
validations, versus30/84 old. The strengthened fixture requires at most four
entries per unique node for that specific shared linear graph and fails against
the frozen old binary (30>20). It also checks a mutation after a completed call,
cycle rejection and exact retry. This is a particular meaningful call-count
oracle, not a universal claim that every possible recipe graph has four edges.

Fresh wholly owned C/C++O1 ASan/UBSan/leak build completed. Registered7 scopes
finished6 pass plus the actual joint94 timeout600.13s, total1173.21s. The real DAG
fixture passed7 checks in560.26s. Original logs/timeouts are retained, including
prior d56f215 sanitizer5/6 with joint94 timeout. No7/7 sanitizer claim is made.

A further cheap-path audit found SourceNormalPreparation::validate(false)
accidentally uses the one-argument projection validation, which scans retained
normal geometry. Deep-flag forwarding and explicit geometry-call/forgery and
same-call proof-upgrade probes remain the next narrow followup. Colour sampling
visual quality is still incomplete, independently recorded in
NORMAL-COLOR-UV-COMPOSITION.md; this ledger does not fix that failure.
