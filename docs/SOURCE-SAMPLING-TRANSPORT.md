# Original image sampling across an editable UV layout

This private continuation implements the geometric sampling transport needed
to separate an original material from a newly authored unit atlas. It does not
yet connect that transport to live editable paint, native owning Scene history
or the import/UV command. The existing initial-atlas render failures remain open
for those workflows. No guard is lifted by this checkpoint.

`src/source-sampling-transport.cpp` verifies exact geometry, normals, ordered
corners and the explicit destination-to-original vertex map. It carries original
UV0 in a real unused coordinate set while retaining the original logical UV1 and
extra sets. Image owners, dimensions, wrapping, both filters and affine role
transforms remain exact. Current UV0 remains the independently authored atlas.
No image resampling, coordinate clamping, nearest geometry matching or source ID
fabrication occurs. Exhausted carriers, correspondence errors, cancellation and
cumulative coordinate/work bounds refuse without publishing output.

Kernel source `5ce0bca` passes 47,781 checks in optimized Release and owned
ASan/UBSan, including independent role coordinates and point samples across all
three wrap modes, six minification modes and two magnification modes. Graphics
source `ba0d2f4` adds only the consumer and its CMake target; the kernel is exact.
Release and sanitizer each pass 5,617 checks across 72 real GL cases: 2/128-pixel
low/high-frequency images, all six minification and two magnification modes,
Opaque/Mask/Blend and independent seam vertices. The original and transported
domains render with maximum difference **0**, at the unchanged tolerance 2.
These use the same production renderer with different actual coordinate
domains, rather than two independent renderer implementations.

The same 72 cases also pass on actual Intel Arc A770 with explicit PCI03
selection, maximum difference 0. Actual GLB export preserves independently
checked per-role coordinates, samplers and decoded PNG pixels. All 72 outputs
are byte-identical across Release, sanitizer and Arc runs. Pinned Khronos
validation reports 0 errors, 72 unfiltered generated-tangent-space warnings and
72 unused-UV0 infos. This does not claim an independent exported-pixel comparison
or eliminate the tangent portability warning. No sanitizer report or suppression
was used. Fresh kernel/consumer TUs use matching immutable owned core/render/
support archives; this is not a fresh whole application build.

The diagnostic evidence also rejects a tempting sampler shortcut. Forcing
non-mipmapped nearest or linear filters makes the earlier small-source views
match, but a genuine 128-pixel alternating source under minification develops
interior errors up to 55, above the fixed limit 2. The ordinary resampled atlas
also has interior errors 3–4. Original image sampling must remain independent;
neither larger output images nor altered filters solve that general problem.

Next work is the actual owning authoring adapter: independent writable paint
and source sampling, original-role GPU composition, exact native Undo/Cancel/
Save/restart, current source/rig/normal/shape authority, explicit older-reader
rejection and standard export fidelity. Scene28 is reserved for that actual
feature, independently of the agent's unfinished Scene27 HDR work; no Scene28
data is implemented or emitted here. Actual avatars, typed MToon integration and
remaining full-goal requirements remain required. See the
[receipt](validation/SOURCE-SAMPLING-TRANSPORT.json) for commands and exact scopes.
No physical headset, system installation or active session changed.
