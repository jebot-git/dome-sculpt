# Private retained-normal core-PBR export checkpoint

This checkpoint compiles a Scene21 retained normal sampling domain into standard
GLB texture coordinates. It is a qualified private component on dc64acb, not an
admission of Source21 into production or completion of colour/UV interoperability.
The full production requirements remain active.

The exporter validates the owning normal recipe, retains its intrinsic or
pre-filter resolved normal image as appropriate, and follows the same complete
frame expansion map as positions, normals, tangents, skin, morphs and source
attributes. Current authored UV0 and genuine source UV1 remain intact. Normal
coordinates share an accessor only when their values agree exactly; otherwise
the compiler emits TEXCOORD_1 or TEXCOORD_2. That export-local association does
not extend native PaintSourceTexture authority or rewrite the authored MToon
material. Core-PBR pixels are compared with the actual Document PBR command.
The legacy shared-UV VRM route remains guarded.

Fresh coherent optimized core/support/renderer builds pass three suites in
5.53s. Fresh owned C/C++ core/support/renderer ASan/UBSan/leak builds pass the
same three suites in 95.48s. Independent cgltf/PNG/GLSL consumers pass 99 checks
over 27 current-state images: generic and typed sources, with and without
genuine source UV1, three poses and morph weights 0/.35/1. Software error is
zero; actual PCI03 Intel Arc A770 error is at most one channel value. The
wrong-atlas normal-sampling negative remains sensitive. All 54 successful
original/current GLBs have zero Khronos errors and 54 unfiltered warnings
(skinned mesh beneath a parent); cancelled old text files are excluded explicitly.
Each variant proves before_commit cancellation preserves the old file and exact
owning scene. Source and binaries stayed unchanged through hardware validation.

The strict original-before-UV colour/alpha fixture still fails at error 68
(89 with normals disabled), despite identical original normal-image bytes. This
is a real appearance-preservation gap, separate from current-state GLB parity.
Native import of the emitted third UV set still rejects it with “Unsupported
texture coordinate set/transform”; preserving that third channel on round-trip
remains required. The current fixture does not qualify actual-avatar retained
domain GLB export, reflected/affine placement, Clear/rebake format pixels,
Paint24 composition, compact material compilation or VRM sampling export.
Those scopes must receive their own implementation and independent evidence.

Exact source/binary and evidence hashes are recorded in
[the receipt](validation/NORMAL-DOMAIN-GLB.json). Local full-source manifests,
logs and generated files remain under artifacts/normal-domain-export-*.
