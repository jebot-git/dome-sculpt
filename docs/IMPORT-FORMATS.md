# Native source capture and format foundation

This private milestone extends a34fdb7. It is a source-retaining foundation,
not completion of the full glTF/FBX/OBJ authoring task or its options UI.

`ImportOptions` is the common native/XR contract: explicit format, input units,
input axes, normal policy, and preservation choices. Preserve is the default.
Unitless OBJ From File explicitly assumes metres and records that decision.
Unsupported selected conversions fail before committing a Scene.

`ImportSourceCapture` opens the chosen directory once and reads each normalized
resource exactly once. Linux openat2 confines resources beneath that directory;
on older kernels the fallback disallows symlinks. Regular-file, size, fstat
mtime/ctime, aggregate-byte, count, UTF-8 path and cancellation checks are bounded.
Atomic pathname replacement retains the already opened old file. In-place
mutation during a read refuses with a retry diagnostic. This is per-file stable
capture, not a filesystem-wide snapshot: geometry, images, parser IDs and archive
all consume the same captured bytes, even if files were replaced between captures.

The immutable `ImportSourceReceipt` labels raw GLB, glTF JSON, OBJ or FBX bytes and
named dependencies. Canonical GLB is a separate authority for native
primitive/material/accessor IDs. Original FBX/OBJ is never relabelled GLB.
External glTF buffers are packed into a canonical BIN and existing buffer views
rebased; accessor/primitive/node/material identities stay unchanged. Unknown
extensions that could carry buffer references refuse instead of guessing.
Original root and resource bytes remain exact. Self-contained GLB keeps its
single original byte owner. Scene11 stores a converted source receipt alongside
the canonical archive, accounts both in existing per-asset/aggregate budgets,
preflights receipt binary counts before copying, and rejects receipts in older
Scene versions. Active and retained source-origin appearances participate in
minimum feature-version selection. No Scene12 polygon or Scene10 color fields
are introduced here.

The native format reader is pinned unmodified ufbx v0.23.1, commit
26a482ae66871d7de36eb722aa060bce95bce274, under its MIT option. Its bounded
allocator/progress interface and geometry/inheritance helper policies provide a
shared C reader for FBX and OBJ/MTL without a Python/proprietary SDK dependency.
See [upstream source](https://github.com/ufbx/ufbx/tree/v0.23.1),
[API](https://ufbx.github.io/reference/) and
[node conventions](https://ufbx.github.io/elements/nodes/).
[OpenFBX](https://github.com/nem0/OpenFBX) was also evaluated; ufbx's explicit
inheritance/unit policies and native OBJ reader fit the shared capture contract.

Implemented static conversion retains node hierarchy, mesh instances including
per-instance material bindings, triangulated corners with logical vertex/corner
provenance, UV0/UV1, RGBA color, authored normals/tangent handedness and basic
material/texture roles. Geometric and inheritance transforms use helper nodes.
FBX From File consumes authored unit/axis metadata; OBJ accepts explicit units
and Y-up/Z-up. FBX also accepts explicit unit/axis overrides through a separately
recorded outer conversion, preserving its raw metadata. Legacy shaders receive a stated ufbx PBR approximation. Missing
OBJ/FBX normals receive an explicitly diagnosed flat canonical normal; raw
absence remains retained. Missing-NORMAL glTF authoring is now supported for the
documented UV0 material subset; see GLTF-FLAT-NORMALS.md. PNG/JPEG texture bytes remain encoded in source/canonical storage;
native bounded decoders produce RGBA with role-specific sRGB/linear meaning
left to materials. JPEG warnings/truncation and excessive progressive scans (over64) fail across a C error boundary,
which never longjmps through C++ destructors.

The shared FLTK/XR options continuation is described in IMPORT-OPTIONS.md.
Required work still includes explicit
glTF flat/smooth overrides and FBX authored skin/blend-shape conversion,
extended/packed/transformed material roles, general affine node authoring,
source-linked rig graph, generated-flat glTF policy, PBR UV1 sampling, COLOR/UV
morphs, no-map overlapping UV authority and VRM bind-space transport. Current
unsupported cases refuse explicitly; the complete user request remains active.

Validation at this source stage: coherent regular-user headless Release;
45 source checks (44 default plus optional pinned upstream binary FBX smoke),
and existing generic-import/skin native tests pass. Source warnings that imply
parser repair currently refuse explicitly. OBJ material-library texture paths
follow the originating MTL directory, as in Blender
([primary implementation](https://github.com/blender/blender/blob/main/source/blender/io/wavefront_obj/importer/obj_import_mtl.cc));
all declared mtllib files are captured into a memory-only parser input, with
ambiguous repeated material names refused. Cases include real detached
OBJ/ASCII FBX/glTF authoring with Scene11 save/restart, independent low-level
canonical parse, units/hierarchy/instances/MTL, JPEG errors/quotas, confined paths,
exact resource retention and source replacement. Fresh C/C++ whole-core ASan/UBSan/leaks subsequently passed the three focused
source/import/skin suites and the45-check optional binary fixture on ac1b7a8.
The options/unit/N-gon continuation has separate proof in IMPORT-OPTIONS.md and
its checkpoint manifest.
