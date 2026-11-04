# Atomic replacement under allocation failure

The output writer used to allocate after the caller claimed commit and after
renaming the temporary file. A persistent C++ allocation failure could therefore
publish the new output and then report failure. The physical editable-export
allocation sweep exposed this with a complete 364,892-byte authoring GLB replacing
the sentinel. A standalone writer regression reproduces it at allocation prefix
8, independently of rendering, source maps or authoring codecs.

The writer now constructs both filesystem paths and opens the parent directory
before the final cancellation check and commit callback. After successful
replacement, its success path only syncs and closes the already opened directory.
An RAII descriptor closes the directory on cancellation, callback failure and
rename failure. Temporary-file durability, the caller's commit callback, atomic
replacement and parent-directory durability remain in that order. Real rename or
post-replacement directory-sync I/O errors still report errors; this change does
not promise rollback after a successful rename when a later I/O operation fails.

The standalone real-filesystem test uses a long parent path and an output larger
than one write chunk. The exact same fixture reproduces the old post-publication
failure. Release and O1 ASan/UBSan/leak builds verify persistent allocation denial
through every successful-path allocation, complete retry, cancellation during
write and immediately before claim, a throwing callback, no temporary leftovers,
and no file or directory descriptor leaks. It also asserts zero C++ allocations
between the successful commit callback and return. The registered CMake/CTest
scope is `atomic-write-allocation`.

This is a narrow header/runtime checkpoint. It does not itself qualify a rebuilt
application, all writer consumers, or the physical editable-export feature. That
integration must rebuild `export.cpp`, `scene.cpp`, `volume.cpp`, `recovery.cpp`
and their consumers, then rerun the real queued export/Save and allocation tests.
The original physical-export and standalone negative logs remain preserved in
ignored artifacts. No schema, source identity, external SDK or installed file
changes.
