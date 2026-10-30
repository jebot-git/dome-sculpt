# Current DomeD toolkit and conventions

Current canonical DomeD is `6feb62b936a9d8d2a2e6a893aebcbda9b724fe23`, checked
against remote main on 2026-10-30. Its FLTK builder and all22 patches are unchanged
from `282467e727df78c8e7f2452aa05a7747bac854a6`; every patch hash is verified.
Use the existing optimized FLTK1.4.5+22 prefix `artifacts/toolkit-282467e`, whose
libraries are in **lib64**. The old revision in that artifact name identifies
its unchanged FLTK inputs, rather than the current compositor revision.
Configure using:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DDOME_COMPUTE_BACKEND=CPU \
  -DFLTK_DIR=/home/blux/de-xr/dome-sculpt/artifacts/toolkit-282467e/share/fltk
```

DomeD6feb adds the native Unicode BDF-only HarfBuzz/Pango runtime and stricter
font export checks. The canonical `scripts/prepare-bitmap-runtime.py` built its
pinned Release runtime privately into `artifacts/bitmap-runtime-6feb62b`;
**that runtime uses lib, not lib64**. Its manifest, upstream/fork revisions,
patches and built-library hashes are recorded in third_party/sources.json.
Native BDF blob/FreeType metrics, 1x/2x, LTR/RTL, Pango exact pixels, outline/
malformed rejection, Rust font-export and actual private-library checks pass.
No Pango frontend is introduced into Dome Sculpt.

For current application/runtime validation, prepend both private directories:

```sh
export LD_LIBRARY_PATH=/home/blux/de-xr/dome-sculpt/artifacts/bitmap-runtime-6feb62b/lib:/home/blux/de-xr/dome-sculpt/artifacts/toolkit-282467e/lib64
./build/dome-sculpt
```

The Release compositor was rebuilt at6feb into
`artifacts/domed-6feb62b-target/release/dome-compositor`, preserving the old282
binary. For private native Wayland tests, set `DOME_COMPOSITOR` to that binary
and `DOME_TEST_LIBRARY_PATH` to the two directories above. The harness otherwise
clears inherited library overrides, and with the explicit path records live
compositor/client mappings in `artifacts/dome-menu/runtime-maps.json`. Those
processes load the prepared FLTK and BDF-only HarfBuzz; they do not load Pango.
The matching canonical Pango runtime is separately validated by its native test.
See HOLDING-IMPORT-INTEGRATION.md and its machine-readable qualification receipt.

Build as the regular user. No system runtime was installed and no active desktop,
compositor or display-manager session was replaced/restarted. Stage and verify
matching artifacts before any later authorized system installation.

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
