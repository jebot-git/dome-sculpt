# Painted VRM1 source outlines — private qualified component

Runtime `fc7e270d3c4ebd486caf9918d825ae1ebd78409a` extends the existing native
VRM1 expression-preserving paint exporter. Opaque, unlit world-space outlines
remain on the actual original lower primitive, including its `outlineColor`
bindings. The derived paint primitive has no outline. This uses the existing
FLTK/OpenXR application and renderer; it introduces no frontend, public layout,
schema, writer, shader or asset changes. Only the complete Layer production TU
changes against qualified component `227c11c`; all other support remains matching
owning37/physical4 `5df649d`.

Genuine zero-coverage RGB extension prevents filtered black fringes without
changing coverage or positive-coverage texels. Actual upper-role invariance
allows original UV expressions to stay on the lower material while paint stays
fixed. Even upsampling can shift every native texel-center knot and smooth sharp
paint beyond the original budget. An odd aligned interior, copied clamp border
and real affine vertex carrier preserve those knots inside the requested
256/512/1024 PNG dimensions. The entire filtered footprint is still certified.

The new outlined path bounds signed biquadratic RGB/coverage error jointly using
Bernstein control hulls, with bounded de Casteljau subdivision if necessary.
The original error threshold and image, work, memory and texel-precision limits
remain. Refinement depth/work and stack scratch are prospectively bounded.
Reported RGB/coverage maxima remain separate unrefined bounds: their sum is not
the tighter accepted joint certificate. The accessor roundoff is measured exactly;
the existing 16-epsilon interpolation screening is a model qualified on the
software/Arc devices below, not an API-wide mathematical guarantee about every
GPU interpolator. Legacy non-outline sampling algorithms and output paths remain;
the generic unsupported-mode diagnostic now says opaque or masked nonanimated.

Both Release and whole-owning sanitizer profiles pass:

- 54 actual worker checks: all three export resolutions, authentic source/paint
  preservation, native package Open/export, ordered Save, cancellation and quotas.
- 181 independent render checks across 36 stereo/pose/resolution image pairs,
  maximum difference 1 within unchanged 4. Missing/wrong-width/wrong-color hulls
  and moving/tinted paint are visible counterfactuals.
- 1,922 factory checks, including all 860 persistent allocation-denial prefixes
  and 1,054 cancellation points, exact complete Scene preservation and recovery.
- 44 actual normal/compact FLTK universal-menu checks plus 22 actual-loader
  synthetic OpenXR accept/Cancel/focus checks. The app's 26 unchanged frontend
  TUs and whole inspector/helper relink with the complete new Layer object.
  Actual maps exclude the original Layer archive member; mapped ELF/input hashes
  remain exact. Private patched Mesa is used only by sanitizer synthetic XR.
- Seven prior Shift/RimMASK/Normal worker families reproduce all 77 golden data
  files per profile byte-for-byte. Across the new worker, independent images and
  selected UI native saves, 87 corresponding R/S files are also byte-identical.

Strict actual PCI03 Intel Arc A770 execution passes the 36-image domain at maximum
1, with pre-pinned ELF/input hashes and actual DRM FD/sysfs proof. The same test
binary refuses software and the actually selected UHD renderer when asked to
require Arc. The first observer incorrectly included Mesa's mutable non-ELF cache
index and remains failed; the corrected observer pins code/fixed inputs and uses
an isolated shader cache. No product GPU compute or readback path changes.

All five genuine VRM containers/profile have zero Khronos errors and pass 42
pinned official-schema instances/profile; the separate role-bearing resolver
control contributes six additional instances and two negative controls, counted
separately. Each profile retains 21 warnings, 115 infos and 12 hints. The original
source-generator missing morph POSITION bounds error remains in the V10 receipts;
V11 fixes the authored input generator and reruns the unchanged complete C++
binaries. No exported JSON or image is repaired after export. One prior-output
sentinel is explicitly excluded per profile. Fresh CMake configuration registers
843 tests including four new outline fixtures; this is not an all-843 runtime run.

This component qualifies the genuine skinned/morphed three-part cube fixture and
its tested poses/views. General reflected/single-sided source domains still need
explicit new witnesses. Normal-mapped, width-textured, lit or screen outlines and
masked/blended outlined expression paint remain guarded in this adapter. Other
owning/prepared frames require their own adapters. This is not main37 admission,
a full regression, physical headset validation or completion of the production
goal. No installation or active-session restart occurred. Latest DomeD remote
`341152b` and the canonical poll-final Release/SAN SDKs were used; local commits
were preserved.

Exact hashes, commands, maps, original failures and counts are recorded in
[the validation receipt](validation/VRM1-OUTLINE-PAINT.json). The full original
REQUIREMENTS/HANDOFF-466ebd1 objective and later controller drawers remain active.
