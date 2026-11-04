# Native physical geometry and worker contracts

This private child of `708283b` fixes an actual resource omission: native physical
export accepted a one-vertex limit. The first paint-only fix also missed SDF
surfaces. Current paint geometry is now checked before extraction, and actual
extracted/captured geometry is checked before native staging. Exact geometry
limits succeed without changing bytes; one below fails without publication.
Existing numeric limits remain unchanged. SDF extraction retains its existing
bounded mesher; this does not implement a predictive new SDF memory adapter.

Fresh owned Release and whole owned ASan/UBSan/leak core/support/render builds
pass four suites each: the original79 consumer checks plus native, retained and
constant-SDF contracts. Release takes24.85s; sanitizer takes433.15s, with1648
source/archive/binary hashes unchanged per cohort. Actual fsync gates prove
running and queued exports Cancel, preserve both old files and exact Scene,
release the following ordered native Save, and retry successfully. This is a
core/render/worker component, not a newly rebuilt frontend app claim.

Native-only persistent C++ denial prefixes pass (2224 on final source).
The old256 retained supplemental sweep was deliberately terminated after measured
observer cost, not passed or restarted. The genuine64 Bake→KeepPaint full sweep
remains unfinished. Stage profiling found actual production duplication in
current color quality replay; a separate bounded optimization is planned.
Do not label unfinished denial prefixes complete.

A new physical-before-UV native command succeeds with exact float tile owners,
normal metadata, Save/restart and Undo/Redo; its first independently emitted
finite image differs17 rather than the unchanged allowed2. That counterexample
remains a real quality gap, separate from this quota/transaction milestone.
Typed native physical MToon, other domain adapters, complete allocation evidence,
coherent main integration and all broader authoring requirements stay open.

See [the receipt](validation/NATIVE-PHYSICAL-CONTRACTS.json) for exact scope,
negative artifacts and current finite jobs. No system/session/HMD action occurred.
