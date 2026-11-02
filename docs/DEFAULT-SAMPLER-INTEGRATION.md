# Default-material and painted-sampler integration

Exact source `5ec250b` combines the qualified Scene17 implicit-material adapter
with painted color/emission sampler preservation. The four Scene17 runtime
files match its component receipt byte-for-byte. Its expected manifest deltas
are the additional sampler test and equivalent float zeros in the shared
POSITION-morph bounds; the latter changes raw fixture JSON bytes. This proof
uses fresh worktree `artifacts/default-sampler-5ec250b`, with fresh build folders,
and does not relabel earlier component binaries or logs.

The fresh regular-user optimized CPU build includes the canonical FLTK frontend
and OpenXR support. Fifteen selected suites pass in9.03s: the default-material
twelve, sampler/GLB/source-color three and affine-export, with the GLB suite
shared between those sets. A separately configured fresh own C/C++ core,
renderer and support ASan/UBSan/leak build passes the same fifteen in40.98s.
Sanitizer configuration enables UI and disables XR; external Mesa, FLTK,
image/system and LAPACK libraries are uninstrumented.

Finalized hardware binaries pass default66 and painted-sampler145 assertions
on Intel Arc A770 (DG2), explicitly selected with
`DRI_PRIME=pci-0000_03_00_0`. All48 independent image comparisons have maximum
error0 at unchanged tolerance2. Device logs record vendor and renderer. The
processes use private HOME/state/config/cache; no active desktop or headset
session is changed.

Pinned Khronos validator2.0.0-dev.3.10 checks all60 raw and worker-exported GLB
files with0errors. All44 warnings are retained:16 skinned mesh non-root,
8 ignored local skin transforms,8 alpha-cutoff fields unused by the alpha mode,
6 ignored tangents without normals and6 generated tangent-space notices.
Independent cgltf/accessor/image tests remain enabled. No issue filters or
tolerance changes were introduced. Source, harness and evidence hashes are in
validation/DEFAULT-SAMPLER-INTEGRATION.json.

This qualifies the selected combined regression scope, not every configured
suite or the full production goal. No fresh native GUI, OpenXR-loader or physical
headset run is claimed here; the existing component/interface receipts retain
their own scopes. Owned UV1/native atlas admission, unmapped overlapping-UV
authoring, general VRM import/export and FBX deformation remain required.
There is no implementation change in this qualification branch, installation
or agent-owned publication.
