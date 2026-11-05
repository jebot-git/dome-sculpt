# VRM1 proven MASK paint and current cutoff

Private runtime `9ef186b35e604a9a57e5c3a049f403807de8517c`, after `e84d919`,
adds painted MASK export when the actual immutable source and independent paint
prove that every supported declared expression state remains visible. It also
repairs a real source-only cutoff export mismatch. General changing cutouts and
BLEND remain required work; this stage is not full VRM1 completion.

The [existing pinned glTF alpha rules](https://github.com/KhronosGroup/glTF/blob/3cd6f59b277ed7f6540dbabdacc7841a5fdc0ded/specification/2.0/Specification.adoc#alpha-coverage)
make MASK fragments opaque after they pass their cutoff. The pinned MToon1
specification delegates alpha processing to those core rules. The compiler
bounds current and future preset/custom alpha factors in the existing native
unit-alpha domain, scans the real source role image, and combines its lower
alpha bound with every native premultiplied paint-alpha/coverage knot. Convex
samplers and bilinear reconstruction preserve that lower bound. A conservative
F32 margin decides admission. No trusted caller flag or persisted authority is
added; real roles, source samplers, UVs, independent paint and lower MASK metadata
remain intact. Complete work is forecast before scans or dense output allocation.

The source-only counter exposes a distinct writer bug: current typed cutoff .3,
original/raw and unchanged inspector cutoff .5, emitted cutoff .5. Native
rendering already used .3. The writer now applies inspector cutoff only when it
differs from the captured native reference; otherwise it preserves current typed
cutoff, matching the native renderer. Original raw bytes still contain .5 and
remain exact through Save, package Open and second-cycle export.

## Closed evidence

Counter `8774e28` preserves 14 real old-worker checks on each of two valid MASK
INPUTs. They capture actual native paint, Save and the old mode refusal with
previous file and complete caller unchanged. The source-only old writer is
separately exercised against untouched sealed `e84` core/support: 21 counter
checks Release and 23 sanitizer. Its first four independent image pairs match
within 1; the actual full expression then fails at **255 > unchanged 4**. All
old outputs and pixels are retained. They are valid standard files with wrong
current cutoff semantics, rather than malformed test inputs.

The new runtime changes only complete `vrm1-expression-layer.cpp` and
`vrm1-export.cpp`. Both profiles rebuild those TUs and whole focused consumers
on copied matching immutable owned `e84` core/support. All 95 public headers,
private layer header, layouts and schemas are unchanged. This is a matched
component scope; no fresh whole app/core, GUI or full CTest claim is made.
Release passes in 142.960 seconds and ASAN/UBSAN/LSan in 597.353 seconds, with
all 1,886 inputs unchanged.

The painted MASK worker passes 49 checks per profile: actual nonuniform paint,
all three output resolutions, original two skins/rigid part, package Open and
second cycle, one paint Undo/Redo, ordered Save, running/queued Cancel, recovery,
quotas and atomic publication. Independent cgltf/PNG/spec-shader rendering passes
175 checks / 36 pairs per profile at maximum 1, unchanged bound 4 and coverage
above 300. Wrong cutoff is a sensitive control; the consumer implements real
MASK discard rather than treating it as OPAQUE.

The actual typed-cutoff source-only worker passes 23 checks per profile,
including native Save/restart, raw .5 retention, emitted .3, package Open and
byte-exact second cycle. Its independent renderer passes 166 checks / 36 pairs,
maximum 1. Both renderers retain the original camera/shifted eye, poses 0/2,
weights 0/.35/1 and second-eye/no-op upload and exact Scene checks. Only graphics
focuses one part; packages retain all three declared avatar parts and dependencies.

A genuinely changing MASK still refuses with 18 actual worker/history checks,
leaving previous file, following queued Save, caller, paint Undo/Redo and recovery
exact. Native rendering proves actual membership change: 261 filled fragments
remain versus 3,777 at zero cutoff on the SAME current geometry/camera. That
cutoff-zero snapshot is an explicitly wrong diagnostic, never an independent
reference or exported data. The initial expectation that all fragments vanish
failed because paint fills holes; that negative is preserved. Another retained
probe correctly hit the existing Apply/Cancel-before-Save guard. The corrected
native observer passes 11 checks: actual membership, real Save refusal/prior-file
preservation, Cancel restoring all original pixels and authored morph defaults,
then successful exact native Save. Product preview policy is unchanged.

The NEW MASK factory passes **1,762 checks per profile**, including all 916
persistent C++ allocation prefixes, all 839 cancellation points, prospective
work/pixel refusal, changing membership, native source bounds, missing binding
and exact retry. It records 2,888,768 working bytes and 8,341,584 sampling units.
Old full sweeps are not repeated. All 29 ordinary prior writer/native files per
profile remain byte-identical. All 148 new PNGs and 22 native/container/input/
control files are byte-identical between profiles; the latter includes sentinels.

Strict PCI03 A770 handle `22299` passes **355 checks**: two independent cases
with 72 pairs, maximum 1, plus 12 native membership/Save/Cancel checks. Both real
binaries reject actual llvmpipe with their hardware flag. Actual device
strings, descriptors resolving to `0000:03:00.0`/8086, maps and DSO hashes are
captured, with 1,901 inputs unchanged, in 10.733 seconds. The lease is released.
No compute, controller/HMD or third-party viewer execution claim follows.

Every final writer plus actual old-counter files and authored INPUTs passes:
22 files, 166 pinned official schema instances, 0 errors; unfiltered pinned
Khronos reports 0 errors, 95 warnings, 334 infos and 84 hints. Configuration
registers 663 tests, including seven dependent MASK/cutoff tests; registration
is not execution of that full suite. Commands, source/object/archive hashes,
all negatives and device evidence are in
[the exact receipt](validation/VRM1-MASK-EXPRESSIONS.json).

## Remaining requirements

Actual changing MASK/BLEND, conservative equality/near-cutoff cases, further
native alpha overrides, initial shared shade factorization, prepared/normal/
MatCap/rim/native/absolute/plain-PBR adapters, coherent root application
integration and broader external VRM applications remain required. Existing
unsupported-owner guards stay strict. The SAME original sanitizer sweep `64560`
remains independently unfinished and is not replaced by this new branch proof.
