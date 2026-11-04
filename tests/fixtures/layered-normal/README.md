# Historical native normal fixtures

These are actual emitted files from the sealed private Scene31 conversion
milestone `7805e3c372e6a052a5ab53995bb5e609e1d60939`. Their hashes match its
`docs/validation/CANONICAL-NORMAL-OWNING.json` native Scene receipt. The files
were copied without rewriting the version, geometry, material or payload.

The two `original` inputs contain the native test baker's mirrored-UV target
and hidden source, with the curved/skin/morph case in variant7. `converted-7`
is the genuine historical Scene31 output, retained to exercise old-reader
semantics and explicit migration rather than manufacturing an older envelope
with today's writer. The authored test geometry is part of this GPL project.

`manifest.json` records exact sizes and SHA256 hashes. The registered native
worker/render test reads these files without modifying them, and writes all
new Scene33, image and Save/restart outputs to its build directory.
