# Current DomeD toolkit and conventions

The desktop now requires canonical27/poll runtime source
`6fa2624f89ddb105e482326995daec0bdbfd6643` (qualified metadata
`341152b`, on the same code). Patches25/26/27 provide responsive bitmap-scale
file pickers, actual Enter callback authority and exact X11 clipboard
reply/ownership cleanup. The canonical builder enables upstream
`FLTK_USE_POLL=ON`, avoiding the demonstrated high-descriptor select overflow.

CMake verifies all three installed patch checksums and a generated runtime
profile. That profile must bind the selected prefix, actual private poll
configuration, builder/complete patch chain and every imported toolkit
library digest. Stale cached libraries outside the selected prefix refuse.
The previous24-patch SDK and incomplete25/26 configurations intentionally refuse.
All165 public include files, FileChooser layout and old exported symbols are
retained; there is no application/public class-layout change.

Use the canonical builder from the pinned source as a regular user. Its new
Release and owned ASan/UBSan prefixes are separate. The bitmap runtime remains
exact canonical53b0fca; its sources/helper/builder are byte-identical at the new
FLTK pin. No system installation is required.

The qualified matching Sculpt evidence is an explicit SDK-only runtime delta:
coherent c092 app/core/support binaries, four actual export/Open/Save/Cancel/Close
menu runs (15 checks each), and native Wayland projection/filename/paint/history
runs (9 named checks each). This is not a new whole-app build.
See [TOOLKIT-POLL-ADOPTION.md](TOOLKIT-POLL-ADOPTION.md) and its machine receipt
for final immutable paths, exact mapped libraries, configure controls, negative
cohorts and scope. Previous lifecycle evidence below remains historical.

## Earlier24-patch lifecycle baseline

That baseline required canonical DomeD revision
`53b0fcaf533ad867300d65a4178285d0a2dbef58` and its24th
`fltk-1.4.5-process-cleanup.patch`. The native process display owner closes the
connection only after App/widgets/GL/workers and before Fontconfig cleanup.
CMake checks the builder-installed patch and its pinned checksum to reject
older same-version toolkits with unsafe/incomplete explicit-close teardown.
The configure step also requires the matching FreeType/HarfBuzz/Cairo prefix
and Cairo cache/deferred-SHM cleanup manifest. The generated launcher prepends both selected
library directories so inherited older same-SONAME libraries cannot silently
replace either configured runtime.
The source patch does not change bitmap helpers, appearance/menu conventions or
public class layouts. This requirement applies to the desktop build; standalone
headless compute configurations do not need a desktop toolkit.

Prepare the toolkit from a checkout at that exact DomeD commit, as the regular
user, with private paths. The matching bitmap runtime includes both Xlib source-cache and deferred-SHM record cleanup. All153 public
headers remain byte-identical to the previous SDK. Prepare both private prefixes:

```sh
python3 "$DOME_TOOLKIT_SOURCE/scripts/prepare-bitmap-runtime.py" \
  --work "$PWD/artifacts/bitmap-53b0fca-work" \
  --prefix "$PWD/artifacts/bitmap-53b0fca" --jobs 2
export PKG_CONFIG_PATH="$PWD/artifacts/bitmap-53b0fca/lib/pkgconfig"
export LD_LIBRARY_PATH="$PWD/artifacts/bitmap-53b0fca/lib"
python3 "$DOME_TOOLKIT_SOURCE/scripts/prepare-fltk-experiment.py" \
  --deps "$PWD/artifacts/toolkit-53b0fca-deps" \
  --prefix "$PWD/artifacts/toolkit-53b0fca"
export LD_LIBRARY_PATH="$PWD/artifacts/toolkit-53b0fca/lib:$LD_LIBRARY_PATH"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DDOME_COMPUTE_BACKEND=CPU \
  -DFLTK_DIR="$PWD/artifacts/toolkit-53b0fca/share/fltk"
cmake --build build --parallel 2
```

`DOME_TOOLKIT_SOURCE` denotes that source checkout. The prefix uses **lib**.
The builder retains the verified archive, COPYING and all24 patches under
`share/fltk-bdf-source`. The whole-toolkit sanitizer counterpart must retain the
same source notices. Current application lifecycle evidence is qualified in
[NATIVE-DISPLAY-LIFETIME.md](NATIVE-DISPLAY-LIFETIME.md); prior62b6417/application
proofs below retain their original scopes. The earlier f797 stack has clean native Wayland and X11 lifecycle evidence.
The new canonical bitmap SDK independently passes actual SHM/source-cache pixel
and teardown checks; the same sanitizer app/FLTK binaries also pass the real
package Open/Save chooser flow with clean exit. See NATIVE-PACKAGE-OPEN.md and
DomeD native/bitmap-runtime/SHM-LIFETIME.md. Coherent current-SDK application
build and broader compositor qualification are separate gates. Older800-byte
source-cache and32-byte deferred-SHM reports are preserved negatives; the
standalone-XR Mesa112-byte report remains unresolved. No suppression is used.

## Earlier23-patch toolkit baseline


Current canonical DomeD is `62b6417bb251aef8e00a1c3e8d4d1a69c043572b`, checked
against remote main on 2026-11-03. Its toolkit hooks, bitmap patch directory and
BDF helper are byte-identical to d271cf65c612a462fcfa6739416b06c6565be070.
The subsequent GTK/Qt/X11 retirement changes leave these canonical inputs
unchanged. System Xvfb/Xwayland removal is preserved; GUI tests use a signed,
private Xvfb server without installing it. All 23 FLTK patches, four bitmap patches,
16 installed runtime libraries, builder and canonical font helper were verified
against their recorded hashes. Inputs remain byte-identical to3024fb6 and
eee23b7, which added native PCF and a23rd FLTK patch. The current helper also
exports media aliases correctly and excludes generated aliases from host choices. The earlier22-patch artifacts and 6feb runtime are retained
as historical evidence; they no longer identify the latest toolkit inputs.

The canonical Release builder prepared `artifacts/toolkit-eee23b7` with all23
patches and local CPU optimization. This prefix uses **lib**, not lib64.
The new canonical native bitmap stack is in `artifacts/bitmap-runtime-eee23b7`,
also **lib**: FreeType2.14.3, HarfBuzz14.1.0, Cairo1.18.4 and Pango1.57.1. Exact
archives, patches, fork revision, licenses and built-library hashes are recorded
in third_party/sources.json and its installed runtime manifest.

Native decoded nonscalable BDF and PCF are accepted; outlines remain disabled.
The vendored `third_party/desktop/bdf_font.hpp` is byte-identical to DomeD's
`native/bdf_font.hpp` at this revision. Other qimgvr appearance/universal-menu
helpers retain their pinned bytes. No unrelated qimgvr working files are imported.
The application remains FLTK; Pango is not an added application frontend.

Configure/launch against the current local artifacts from the repository root:

```sh
export PKG_CONFIG_PATH="$PWD/artifacts/bitmap-runtime-eee23b7/lib/pkgconfig"
export LD_LIBRARY_PATH="$PWD/artifacts/bitmap-runtime-eee23b7/lib:$PWD/artifacts/toolkit-eee23b7/lib"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DDOME_COMPUTE_BACKEND=CPU \
  -DFLTK_DIR="$PWD/artifacts/toolkit-eee23b7/share/fltk"
cmake --build build --parallel 3
./build/dome-sculpt
```

For an existing build directory, clear `*DOME_BITMAP_RUNTIME*`, `*NATIVEGL*`
and `*RENDERFONTS*` cache
entries when switching runtimes. The pkg-config font packages precede GL so
FindPkgConfig resolves private FreeType/HarfBuzz before GL's system library
directory; headers and same-SONAME libraries must match. Verify actual `ldd` and
live mappings, rather than only the configured prefix. An inherited library path
can override ELF RUNPATH. The current UI configuration rejects retained
FreeType/HarfBuzz/Cairo library paths outside the selected canonical prefix,
even when the live pkg-config metadata already names the new prefix.

To prepare fresh artifacts, run the canonical DomeD
`scripts/prepare-bitmap-runtime.py` and then `scripts/prepare-fltk-experiment.py`
with explicit private prefixes/dependency directories. Point PKG_CONFIG_PATH,
LD_LIBRARY_PATH and CMAKE_PREFIX_PATH at the new bitmap prefix while building
FLTK; this workstation's FLTK cache confirms it used that FreeType's headers and
library. Both scripts build Release as the regular user. Existing source/build
artifacts are retained, including old compositor binaries.

The latest private optimized nested compositor is
`artifacts/domed-62b6417-target/release/dome-compositor`: its full Release build
passes with all1,000 source inputs unchanged. Eight actual native Wayland/menu
checks pass on the immutable9050/e7 frontend snapshot, with six live canonical
libraries verified. This is not a fresh master42e frontend, sanitizer or hardware
claim. Read validation/TOOLKIT-62b6417.json for exact scope and file hashes.

The previously qualified private compositor is
`artifacts/domed-3024fb6-target/release/dome-compositor`. A clean latest d271cf6
Release compositor build is tracked separately in validation/TOOLKIT-d271cf6.json.
Set DOME_COMPOSITOR to
that file and DOME_TEST_LIBRARY_PATH to the two library directories above for
`tests/dome_menu.py`. The harness clears inherited overrides by default, permits
only explicitly selected private directories and records the live compositor/
client FLTK, FreeType, HarfBuzz and Cairo mappings. Runtime/checker tests also
validate the Pango companion libraries independently.

Native PCF shaping, Unicode/bidi metrics,1x/2x, exact Pango/Cairo pixels and
outline rejection are tested by the canonical runtime suite. Its lossless PCF
adapter checks57086 Unicode glyphs. The application palette's optional third
argument selects a native PCF fixture and verifies native glyph/placement/pixels,
integer2x and actual GL output against the lossless reference. The private
Unifont17.0.04 fixture URL/hashes are in artifacts/font-fixtures-eee23b7/SOURCES.json;
it is neither bundled nor installed. Exact current application qualification and
limits are recorded in GLTF-FLAT-NORMALS.md and its validation receipt.

No system runtime was installed and no active desktop, compositor or display-
manager session was replaced/restarted. Stage and verify matching artifacts
before any later authorized system installation. No physical headset test is
implied by the private compositor or synthetic OpenXR checks.

## Historical toolkit evidence

The following records describe earlier checks and artifact layouts; they do not
supersede the current revision and explicit runtime selection above.

Current source checked 2026-10-29 against DomeD main
`282467e727df78c8e7f2452aa05a7747bac854a6`. Earlier builds used
`742d009f446cffdeb89031664b72885ff6f16a19`, and fltk-vr main
`4f54821e61993a9212a2d9cd03635342b58acf6a`. The workspace
`/home/blux/de-xr/DESIGN_CONVENTIONS` and current DomeD README were read.
The full authoring objective remains in REQUIREMENTS.md.

Use the canonical DomeD toolkit builder, not the older fltk-vr collected
snapshot: that snapshot's prepare script references a missing patch. The
canonical builder includes22 reviewed patches, including window suspension,
touch and pen input. Exact archive, revision, ordered patch names and SHA256
values are recorded under `domed_fltk` in third_party/sources.json.

This task built a fresh Release toolkit into `artifacts/toolkit` using a clean
DomeD worktree at the pinned revision. Existing unrelated capture changes in
DomeD and the local qimgvr appearance edits were preserved. Committed qimgvr
helpers still match the vendored copies; no uncommitted helper was imported.
No installed toolkit or desktop/display-manager session was replaced/restarted.

Reproduce from the repo root, with a clean checkout of the pinned DomeD source
at `$DOME_SOURCE` and the verified FLTK archive available or downloadable:

```sh
python3 "$DOME_SOURCE/scripts/prepare-fltk-experiment.py" \
  --deps "$PWD/artifacts/toolkit-deps" --prefix "$PWD/artifacts/toolkit"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DDOME_COMPUTE_BACKEND=CPU \
  -DFLTK_DIR="$PWD/artifacts/toolkit/share/fltk"
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure -j2
./build/run-dome-sculpt
```

The generated launcher prepends the configured library directory. This is
necessary because an inherited LD_LIBRARY_PATH can override ELF RUNPATH with
older FLTK libraries of the same SONAME. The OpenXR CTest uses the same explicit
runtime directory. GUI scripts should be run with that directory prepended too.
`ldd` with the selected environment verifies actual resolution, not only CMake's
configuration. GUI/Wayland tests use private buses, displays and synthetic
projects; device tests remain pending. The public FLTK file-chooser declaration
comes from `FL/Fl_File_Chooser.H`, not the absent `FL/fl_file_chooser.H` header.

Before a system installation, stage/verify the same toolkit for the installed
`/opt/fltk-vr/toolkit` runtime and use sudo only for the install step. This task
has not installed or switched any user session.

The remote input-order milestone d8e3699 was tested in another workspace using
/opt libraries; those environment notes are archived in HANDOFF-d8e3699.md.
This workspace retains the verified current toolkit above.

The UV-worker integration was performed in a separate workspace where the parent
DESIGN_CONVENTIONS file is absent. Its locally rebuilt artifacts/toolkit matches
the same pinned archive/22-patch manifest. Current source README and the committed
menu audit supplied available conventions; do not relabel upstream environment
readings or logs as fresh local evidence. See the current HANDOFF.md.

Latest canonical check: DomeD7322b0eea5d63f8f32dd68a0983a55dc5c8b81b3.
Its FLTK preparation script/patch set is unchanged from742d009; the existing
22-patch Release toolkit remains current. Latest DomeD itself was rebuilt
Release from clean artifacts/domed-current using that toolkit, with unrelated
capture work untouched. Build: artifacts/domed-current-build.log. Fresh private
Wayland/universal-menu8/8 checks pass using that binary and cf6d6b3 client
(dome-menu-avatars-pinned-final.log). No active compositor/session restart.

## Updated popup behavior at282467e

DomeD main advanced after the above verification. Commit eaeb8e0 changes the
canonical popup-done patch to keep parent menus open when closing submenus;
282467e also enlarges the compositor XR mouse-finder pulse. A clean detached
source worktree is in artifacts/domed-latest-source. The canonical builder is
successfully built the updated22-patch Release toolkit into artifacts/toolkit-282467e;
log: artifacts/build-toolkit-282467e.log. Exact new patch hashes and combined
digest are in third_party/sources.json. The client was rebuilt against this
prefix; actual linkage resolves its lib64 libraries (linkage-toolkit-282467e.log).
All47 Release suites pass, native universal menus53/53 and baked-surface GUI5/5
pass with this toolkit. The clean latest Release compositor build completed in
8m03s (artifacts/domed-282467e-compositor-build.log); binary is in
artifacts/domed-282467e-target/release/dome-compositor. Exact pinned8f4d67d
passes native menus53/53, rig/weights12/12, avatar export5/5, baked GUI5/5 and
real latest DomeD Wayland/universal-menu8/8. Run with the updated prefix's lib64
in LD_LIBRARY_PATH, since an inherited /opt path overrides ELF RUNPATH. Neither
the installed compositor nor the active desktop session was replaced/restarted.

## Combined bake/authoring checkout

This workspace rebuilt the pinned 282467e toolkit into artifacts/toolkit-authoring
from a clean artifacts/domed-authoring-source worktree. Configure this combined
checkout with `-DFLTK_DIR="$PWD/artifacts/toolkit-authoring/share/fltk"` and launch
through build/run-dome-sculpt. Its library directory here is lib (the other
workspace used lib64). The builder initially lacked the GLU development files;
artifacts/toolkit-authoring-deps/sysroot now points at the existing verified
artifacts/toolkit-deps/sysroot, supplying that dependency without installation.
All 22 patch hashes and their ordered digest match third_party/sources.json.
The successful build log is artifacts/build-toolkit-authoring-with-sysroot.log.
See VALIDATION.md for results from the combined application source.

The private optimized `d271cf6` compositor and accepted selection client now pass
eight actual native Wayland/universal-menu checks. Runtime mappings confirm the
canonical FLTK and bitmap libraries. This isolated software-Vulkan qualification
did not install system files or restart the desktop. See
`validation/TOOLKIT-d271cf6.json` for binary and evidence hashes.
