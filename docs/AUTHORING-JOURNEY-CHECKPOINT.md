# Source-preserving authoring workflow checkpoint

This extends the existing native FLTK/OpenXR application. The complete production
objective remains active; polygon authoring, held-object isolation, expanded import
options and the remaining requirements are not completed by this checkpoint.

## Usable changes

A newly prepared sculpt conditions inconsistent generated normals before UV
creation. It splits only the affected face corners, retaining every exact position,
triangle and winding. Imported authored normals and existing textured meshes are
not changed. Preparation is bounded and cancellation leaves the input exact.

Canonical MikkTSpace generation still runs first. Entirely valid results are kept
bit-identical. A separately named conditional build retries invalid frames using a
double-precision cross/dot angle only when an acute projected corner angle rounded
to zero. Intrinsically singular authored normal frames still refuse. Both marked
patches, the unchanged upstream header and pinned-source reconstruction were
verified; notices and hashes are in `third_party/sources.json`.

Generated UV atlases now detect analytic triangle overlaps with the same predicate
and tolerance used by the UV editor. A bounded repair moves a colliding small face
to an independently padded chart without moving its old shared vertices or dropping
geometry. Reserved-band capacity, requested padding and explicit resolution errors
are retained. The editor continues to reject collapsed or overlapping drafts.

Bake surface options expose partial and backface projection in the native dialog
and the existing XR Bake page. Both default off. Partial projection preserves every
previous baked channel at missed texels, leaves editable paint layers intact and
reports hits, misses and retained coverage. A completely missed projection still
refuses. Accepted baking remains one Undo step; modal Cancel and focus loss retain
the original state. Native options scroll at compact double-size BDF settings while
keeping action buttons reachable; universal-menu ownership and Ctrl+W are retained.

## Evidence and scope

`tests/authoring-journey.cpp` exercises actual public Document operations:
painted source export, source-retaining import, world-scaled voxel conversion,
procedural detail, mesh preparation, projection bake, painting, explicit UV face
selection and editing, native save/restart, background glTF export/reimport and
exact independent Undo/Redo endpoints. It passes 29 checks on CPU and Intel Arc
OneAPI. Selecting an independently padded chart is intentional: scaling an arbitrary
concave island can overlap an island inside its empty area, and a sub-ULP sliver can
collapse. Those edits remain invalid rather than weakening validation to pass them.

Qualification in `artifacts/procedural-import-integration`:

- Optimized CPU full native run: 114/114 suites. Final workflow revision and UV,
  atlas and bake regressions are additionally rerun against coherent consumers.
- Entire C/C++ core, xatlas and both Mikk libraries instrumented with ASan/UBSan:
  the three new suites pass, including leak detection. This is not a claim that
  all 114 suites were rerun under sanitizers for this checkpoint.
- Native tangent fixture: 11 checks, 108 analytic corners across scale, rotation
  and mirroring; 36 originally zero corner frames recovered; ordinary frames exact.
- Native projection fixture: 15 checks including 2048 hits/2048 misses, exact
  preserved color/ORM/emission/normal bytes, strict refusal and real Undo.
- Actual FLTK controls: 7 workflow checks and 4 compact 640x360 double-size BDF
  checks; screenshots inspected. The helper's extra fixture PASS is not a GUI check.
- Real-loader synthetic OpenXR: original 150, resolution 17, procedural 11 and
  projection-policy 11 checks. Both real GL eye buffers and image ownership are
  checked. No new physical-headset test is claimed.
- OneAPI new native suites and projection OpenXR pass. Six direct hardware
  graphics fixtures pass on Intel Arc A770, covering retained normal frames,
  independent glTF/VRM consumers, source colors and buffer reuse.

Logs retain initial failures and subsequent diagnoses. One exploratory UV-quality
prototype was removed: stricter chart generation would unnecessarily reject more
low-resolution meshes. A transient atlas regression invocation used an executable
linked to that prototype; all consumers were rebuilt before the final rerun. No
sanitizer, GPU, physical-device or interoperability evidence from another source
revision should be inferred beyond the stated scopes.

## Integration

`ObjectBakeSettings` and `ObjectBakeReport` gain fields, so build every consumer
against matching headers and archives. This checkpoint adds no persistent document
schema. Integration with generic Document transactions, Scene 11 import receipts,
VRoid-only creation and the separate polygon/holding work requires its own coherent
build and regression pass before publication as the default application.
