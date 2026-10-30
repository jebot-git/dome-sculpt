# Import source descriptor ownership

This narrow continuation of 1651b37 changes no public interface, archive version,
source identity, file format or UI. It closes two actual descriptor leaks when
C++ allocations fail persistently during source capture.

## Ownership fixes

`make_unique<State>(dup(directory.value))` evaluated `dup` before allocating its
owner. The original direct-import probe failed at allocation 43: four descriptors
became five, with actual probe exit 42. The state now allocates first; only after
success does it take the already-owned directory handle. There is no unnecessary
second descriptor, and the existing close-on-exec property remains intact.

The older-kernel `openat2` ENOSYS fallback also held a raw duplicate across
allocating filesystem-path and iterator operations. A separate State-only-fixed
executable retained that old fallback and failed the real direct-import
FD assertion at allocation 552. The complete fix owns the walker handle through
RAII, transfers ownership only after successful component opens, preserves open
failure errno and uses close-on-exec on the temporary descriptor. Every component
still uses `O_NOFOLLOW`; this deliberately stricter fallback never permits a
symlink to weaken directory confinement.

Both negatives are retained under artifacts in the corresponding frozen/private
checkouts. They are distinct from ASan's heap-leak diagnostics.

## Regression

The existing real `prepare_authoring_import` test now counts `/proc/self/fd`
through C directory operations after every allocation failure, cancellation and
retry. The three genuine source cases remain: generated-flat GLB, external
textured glTF, and authored-inverse-bind skinned/morphed GLB. The textured source
has a nested `buffers/texture-source.bin` resource and exact immutable raw receipt.
A test-only syscall link wrapper forces ENOSYS without changing production code.
Unknown direct syscalls in the executable are an explicit test failure.

Both kernel paths must preserve complete original Scene/pixels/COW owners/file
bytes, release every source descriptor and reproduce the same successful Scene
on retry. Parent paths and outside symlinks are refused. The fallback additionally
refuses internal file/directory symlinks, while normal source-capture regressions
retain the confined-symlink behavior of `openat2`.

| Actual source case | Normal allocations | Forced ENOSYS allocations | Cancellation callbacks per path |
| --- | ---: | ---: | ---: |
| Generated-flat GLB |2009 |2009 |57 |
| Nested-resource textured glTF |2203 |2206 |58 |
| Skinned/morphed GLB |2728 |2728 |83 |

Each six-case run covers 13883 persistent allocation failures and 396 cancellation
sites, with real descriptor checks after every failure and retry. These are C++
`operator new` failures, not injected libc/kernel allocation or file-open errors.

## Qualification scope

The changed complete source object and test were compiled with the exact 1651b37
Release flags, and separately with matching ASan/UBSan flags. They link before
that checkpoint's immutable core/support archives. Link maps prove the archive's
old `import-source.cpp.o` was not extracted. All 69 public/JSON files match that
checkpoint byte for byte. The sanitizer support archives are the previously
whole-C/C++-instrumented core/xatlas/Mikk/ufbx build; the changed object/test are
instrumented too. This is coherent focused qualification, not a new whole-project
build. Parent integration still requires rebuilding all consumers together.

Release passes all six direct-import cases and the existing 50-check source
capture suite (13.62s for the six direct cases). The focused ASan/UBSan/leak run
passes all six cases in 174.18s with the same failure/cancellation counts. Exact
artifact hashes are recorded in the checkpoint receipt. No GUI, GPU, installation or physical XR test
is claimed by this change.

A normal full rebuild registers the six direct tests through CMake; for example:

```sh
ctest --test-dir build -R '^import-memory-' --output-on-failure
```

Broader affine/native-format authoring and the original full sculpt goal remain
separate required work.
