# Actual project-open worker fault coverage

Test-only commit42da762 repairs the transaction-open observer: Open uses
`load_scene_file` on the ordered file worker, so watching the older `Scene::load`
never armed replacement-publication tracking. The real loader is now wrapped.
`open` arms the existing transaction observer only after its successful return;
the new registered `transaction-open-decode` observes the actual loader itself.
Both Open scopes deny every allocation from the selected prefix onward. Worker
cleanup/error delivery remains outside the deliberately bounded injection scope.
No production runtime, public layout or archive changes.

Each profile executes five complete workflows. Release exercises57 persistent
publication prefixes and512 persistent decode prefixes; ASan/UBSan/leak exercises
57 and511 respectively. Both also pass51 queue-admission,34 Save and79 scalar
allocation points under their existing one-shot injection semantics. Every failed
Open preserves the exact encoded Scene including original source maps and paint,
content/history/redo/revision/generation/recovery tokens, source owner and cached
mesh, then releases busy ownership and retries the actual file to the exact
independent replacement. Both profiles retain2234 captured inputs without drift.

The complete changed test TU is linked over immutable matching af79 Mirror core
and c092 support archives; all product sources are checked byte-identical to the
qualified9ae milestone. Release takes9.612s and sanitizer34.467s, including compile
and link. This is scoped native component evidence, not a fresh full application,
full-suite pass, GUI/headset test or installation. The original c092 full589 result
and its zero-allocation failure remain preserved. Read
[the receipt](validation/PROJECT-OPEN-FAULTS.json) for commands and input hashes.
