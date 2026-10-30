# Native polygon authority and retained sources

This private foundation extends the existing native Scene codec. It is not the
complete polygon authoring UI or the completed full glTF/FBX authoring port.
The original full objective and remaining import requirements remain active.

`SculptObject::polygon` owns immutable stable-ID N-gon topology independently of
its triangulated painting/display derivative. An active nonempty authority must
match that derivative's REST positions, indices and UV0 exactly. Posed preview
positions are not used as topology authority. Authored normals, material/frame
attributes and deformation remain independently validated against that rest mesh.
An active empty authority has no painting derivative and suppresses a retained
Volume surface, including after restart. An inactive authority permits sculpt
preview while export retains the existing dormant-source refusal.

`PolygonOrigin` retains a detached full Volume authoring snapshot, optional paint,
material, source appearance, source material/frame bindings, deformation, retopology,
world transform and name. Ordinary edits reuse their first origin. Explicit later
conversion can retain a prior polygon authority, forming a bounded DAG. The
`source_object` value is provenance only: zero, deleted IDs and `UINT64_MAX` are
accepted; restoring a detached original never depends on a current object lookup.
Original retopology provenance likewise is not rebound to a live object.

## Scene12 format

The writer uses the maximum existing feature version and polygon version12;
ordinary scenes continue to select their previous schema. Scene12 requires a
complete per-object `polygon` reference/null and one `polygon_sources` archive:

- `authorities`: topology, active flag and optional origin reference;
- `origins`: complete retained authoring records and optional prior authority;
- `volumes` and `paints`: identity-deduplicated immutable source snapshots.

The archive has its own version1. Images, original GLB/canonical adapters and raw
OBJ/FBX/glTF resource receipts use the existing central image/model archives.
Material holders and first-UV origins use the existing central material archive;
normal-frame and authored inverse-bind authority remain intact. The Scene12
source tables do not replace or reinterpret those older schemas. Scene5–11
compatibility, both historical Scene7 shapes and flat Volume4 discrimination are
preserved. Older Scene declarations reject polygon fields rather than discarding
them. The optional colored preparation Scene10/material3 prototype is not merged
here; its future union must keep its own recursive receipts and resource counts.

Each graph table is capped at512 records; a retained authority chain is capped
at32. Authority vertices, edges, faces, face corners and interpolation provenance
share an eight-million-entry aggregate cap, in addition to existing per-topology
limits. All table references, complete feature shapes, reachability and cycles are
checked before decoding bulk resources. Unreachable records are refused. Aggregate
preflight includes all current and retained Volume samples/layers/masks (four
million entries), painting resolutions (16 million pixels), painting/encoded mesh
storage (256MiB), deformation (eight million entries), retained polygon/UV geometry and source
remaps (eight million each),
and retopology (one million entries/64MiB payload). Existing central image/model/
frame budgets also apply. Shared pointers are deduplicated by identity; distinct
serialized copies are charged separately. No archive version is assigned to the
ephemeral geometry journal.

## Qualification and open failure

Regular-user optimized Release, CPU, UI/XR disabled:11 focused suites pass,
including33 new archive assertions plus the retained-DAG allocation fixture.
The latter injects every measured allocation in the plain retained-DAG decoder,
checks all914 measured allocation boundaries, confirms both the input archive
and live source remain exact, and retries. Final exact-source Release11/11 passes
in7.52s; fresh whole C/C++ core and bundled Mikk/xatlas/ufbx ASan/UBSan/leak
qualification also passes11/11 in69.25s. System libraries are not instrumented.
The final checkpoint manifest records source/binary/log hashes and commands. This codec checkpoint does not imply a new GUI, Arc or headset
result. Those consumers belong to the separate polygon workflow integration.

Complete serialization/source-material OOM safety remains REQUIRED and incomplete.
The actual exhaustive negative probes are retained, not suppressed:

- `dome-sculpt-polygon-scene-faults-test --encode`: inherited Scene serialization
  terminates when nlohmann JSON's `noexcept` destructor allocates its traversal
  vector while cleaning initializer-list values (plain fixture boundary600/1917).
- `dome-sculpt-polygon-scene-faults-test --source`: the retained source-material
  decoder reaches the same library destructor during the inherited default
  `MToonMaterial` construction (mapped fixture boundary70/1134).

The negative stacks/logs remain under `artifacts/gdb-scene12-encode600.log` and
`gdb-scene12-source70.log`, with original failing fixture/log copies. The default
CTest fault fixture covers the complete plain DAG decoder; it does not claim
those separate failing source/encoder cases. A separate dependency worktree will
investigate bounded allocation-free destruction and appropriate provenance.

A distinct inherited OOM hazard was fixed narrowly: nlohmann scalar/string
comparison overloads are `noexcept` but construct allocating JSON strings. Scene
and Volume format checks, source-UV policy and material normal-frame comparisons
now use checked string references. No schema, accepted values or rendering policy
was changed by that correction.
