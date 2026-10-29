# Current DomeD toolkit and conventions

Verified 2026-10-29 against fetched DomeD main
`742d009f446cffdeb89031664b72885ff6f16a19`, and fltk-vr main
`4f54821e61993a9212a2d9cd03635342b58acf6a`. The workspace
`/home/blux/de-xr/DESIGN_CONVENTIONS` and current DomeD README were read.
The full authoring objective remains in REQUIREMENTS.md.

Use the canonical DomeD toolkit builder, not the older fltk-vr collected
snapshot: that snapshot's prepare script references a missing patch. The
canonical builder includes22 reviewed patches, including window suspension,
touch and pen input. Exact archive, revision, ordered patch names and SHA256
values are recorded under `domed_fltk` in third_party/sources.json.

Upstream checkpoint `96bc62f` built a fresh Release toolkit into `artifacts/toolkit` using a clean
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

The reconciled checkpoint `d8e3699` used the older `/opt` build because these
artifacts were absent. The following gesture-history stage fetched the pinned
DomeD revision into `artifacts/domed-source`, verified all 22 patch hashes/order
against the manifest, and rebuilt `artifacts/toolkit` locally. Its builder uses
an ignored `artifacts/toolkit-deps/sysroot` symlink to the existing local FLTK
experiment development sysroot for GLU. The source archive comes from the
installed notice bundle and its checksum was verified by the builder.

The gesture-history build is Release and uses this new configured toolkit; ldd
with the selected environment confirms both FLTK libraries resolve from it.
The parent DESIGN_CONVENTIONS file remains absent; current DomeD README and the
committed universal-menu audit supply available conventions. No installed
prefix or active session was modified.
