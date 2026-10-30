# Model-wide source authority prerequisite

This private Scene 13 migration prerequisite introduces CreatorSourceModel and
CreatorAssetArchive.add_model()/model(). A source hierarchy can hold one immutable
canonical/raw/model/image owner independently of primitive material and vertex
remaps. It does not create a fake primitive appearance or duplicate a potentially
large source_vertices array for every group node. CreatorAppearance and the
public CreatorAssetArchive layout remain unchanged; only the private archive
entry representation changes.

The existing source_models record shape and image/model deduplication remain
unchanged. Legacy appearances and model-only references share one parsed model
owner before and after restart. A shared canonical byte owner cannot silently
adopt a different parsed asset or raw receipt. All input owners must be immutable
before publication, with no writable aliases retained by callers.

A new model-wide source witness authenticates original node local float bits,
parent/child topology, transform flags, mesh/skin references and morph weights
against the captured canonical JSON. It invokes the same cgltf local-transform
primitive as the parser; it does not reread files or reimport geometry. The
existing 100000-node/import metadata budgets apply, graph cycles/duplicate parents
refuse, and cancellation/allocations occur before either archive commits. Model
and image publication stay atomic. A source without an authored material is legal
model authority; primitive default-material authoring remains separately guarded.

Focused Release and changed-source ASan/UBSan/leak runs each pass 4680 assertions:
466 persistent add-allocation boundaries, 1083 decoder boundaries and 3 measured
add-cancellation boundaries, exact retries and immutable inputs. Existing native
creator-archive and actual CC0 VRoid consumers pass with the replacement too.

Qualification compiles the COMPLETE changed creator-archive.cpp and new test,
then links them ahead of immutable exact 84d9 CPU core/polygon/xatlas/Mikk/ufbx
Release archives. Link maps prove the old creator-archive.cpp.o was not extracted.
The sanitizer run instruments the changed complete source/test; support archives
remain Release. This is explicitly not a fresh whole-core sanitizer build. No
Scene/group/object layout or archive version changes are introduced by this
checkpoint; actual Scene 13 placement adoption and its fresh consumers follow.

A subsequent run links the same instrumented replacement source/test to the
parent's frozen exact 84 whole-C/C++ ASan/UBSan core and bundled support archives.
It passes the same 4680 assertions and 1549 persistent allocation sites. The
original changed-source/Release-support result remains retained separately.
This reuses a coherent instrumented checkpoint; it is not a new whole-project
build. Archive manifests and the old-member exclusion map are retained.
