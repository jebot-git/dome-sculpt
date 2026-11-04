# Private preserving-only raw HDR editable projects

This continuation of the sealed `77fa` HDR controls qualifies unpainted,
source-authored PBR HDR import, viewing, native Save and explicit Editable GLB
production without forcing an atlas. Runtime is `bcc1923`; final format/test source
is `015fa3c`. It is a private component, not main admission or full-goal completion.
The complete receipt is [RAW-HDR-PRESERVING.json](validation/RAW-HDR-PRESERVING.json).

The original immutable Scene remains the exact editable authority. The portable
view retains the actual source coordinate sets, sampler/transform distinctions,
normal frames and decoded image samples; original encoded source bytes remain in
the native project. Current linear radiance uses a bounded glTF factor plus
`KHR_materials_emissive_strength`. Original factor and strength are declared
retained only after separate canonical proof. JPEG may be losslessly recoded from
its decoded pixels to PNG; this is not a promise to preserve its standard-view
encoded bytes. The metadata does not call the portable view HDR/EXR or lossless.

A sealed NativeAuthoringCapture derives its own surfaces. Canonical GLB replay
checks the real role indices, texture/image associations, samplers, transforms,
UV accessors and encoded/decoded images rather than trusting mutually consistent
mutable DTO copies. Working storage includes bounded parser, sparse-coordinate
and sampled-image decode scratch. Unused source images remain native owners but
allocate no decode scratch. No public layout or persistent format was appended.
Ordinary strict GLB and the existing native HDR package entrypoints retain their
guards and interpretation.

The canonical parser is the owned C++ cgltf implementation. A private intrusive
allocation arena cleans partial tables when allocation or cancellation unwinds;
normal success still calls cgltf_free. This avoids its observed partial-table
null dereference without changing the third-party/global importer. The callback
has no noexcept boundary, and the owned object has exception/unwind tables.

## Closed evidence

- Matched optimized Release 7/7 and wholly owned ASan/UBSan/leak 7/7 pass. The clean
  sanitizer baseline and incremental private-header rebuild boundaries are
  explicit in the receipt; this is not a clean all-target/full-regression claim.
- The independent PBR consumer passes 299 checks over 16 sources and 32 mixed
  material/pose/morph states: UV0/1/2/unused3, transforms, reflection, generated
  UV2 tangents, skin/IBM, sparse and normalized-u16 UVs, JPEG, and shared image
  pixels with independent role samplers. All retain the existing mean<=2 and
  interior<=2 oracle; observed interior maximum is 1. Exact native recovery has
  full-frame pixel delta 0. This does not impose a global one-pixel edge bound.
- Each profile passes 18 actual export/owner/quota/cancel contracts and 6,479
  persistent C++ allocation-denial prefixes. Running and queued native file jobs
  cancel before publication, preserve old files and permit following ordered Save.
- Each profile independently passes 44 persistent parser C++new failures,
  44 actual malloc-null failures, 48 cancellation boundaries and a throwing
  callback, with zero tracked outstanding allocations and clean retries.
- A real 4095-square unused source image survives exact native roundtrip under
  the default working cap; its former conservative refusal is preserved.
- Current matched app/inspector normal 33 + compact 28 GUI checks per profile
  pass: 122 actual assertions on the final canonical 27 SDK. Normal includes real
  unprepared Import; compact 640x360/font32 covers current raw material controls,
  paint refusal, policy/export/Open/Save. It does not claim compact Import.
- Strict PCI03 Intel Arc A770 passes 299 raw + 127 atlas + 128 physical checks;
  each software-required-hardware control refuses. No physical HMD or session
  was changed. Physical 128 retains the established finite-filter counterexample,
  rather than claiming every view is lossless.
- Current R82/SAN82/Arc127/GUI4 GLBs have zero unfiltered Khronos errors, with
  13/13/21/0 warnings retained in full. Seventeen legacy LDR and three legacy HDR
  GLBs per profile are byte-identical to the qualified `790` controls.

The original product-equivalent factor/strength mutation, coordinated sampler
acceptance followed by re-export refusal, cgltf partial-allocation crash and
unused-image working refusal remain preserved with exact sources/artifacts.

## Required continuation

Direct raw texture painting still requires genuine source-aware preparation;
the actionable refusal is atomic and is not completion of direct raw authoring.
The existing explicit atlas workflow remains editable and retains original
source authority. Generic native-only Strict/SDF physical materials and retained
Paint24/normal-metadata physical domains still require genuine compiler adapters.
They must reuse actual paint/normal coordinates and float coverage, preserve the
exact native project, and provide a measured finite PBR view. They cannot borrow
an unrelated normal receipt, flatten source identity, or weaken old HDR guards.

Full avatar/VRM, general UV/refinement, later editable commands and coherent root
integration remain required. Controller spatial drawers remain a later full-goal
phase. No private proof here means these features have reached main.
