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
