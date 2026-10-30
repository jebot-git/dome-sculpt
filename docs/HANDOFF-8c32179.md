# Native procedural sculpt candidate

The full production objective in REQUIREMENTS.md and HANDOFF-466ebd1.md remains
active and incomplete. Published main8e206fc and its exact original handoff are
preserved in HANDOFF-8e206fc.md. This candidate starts at9f54028, combining that
published ancestry with staticCOLORd3e5e61 and paged ray6c64c5d; generic Document
transaction changes and the new import/bake work are still separate.

## New usable workflow

Sculpt → Procedural detail and XR Objects → Sculpt layers → Procedural now
provide seeded raised panels, recessed cuts and mixed extrusions. Six object-local
projection faces, grid, seed, density, gap, depth and rounding are usable in native
FLTK and large joystick/ray panels. Existing protection and mirror settings apply.
A cancellable source-leased worker publishes a separate sculpt layer and one Undo;
Cancel/failure/stale/no-op leave the source exact. Existing paint/deformation/source
archives remain. Hiding the layer restores original samples. See
PROCEDURAL-SCULPT.md for algorithms, physical units and explicit resource bounds.

Ordered rounded-box CSG dispatches through CPU/OneAPI/CUDA/HIP. Only changed
4-byte distances return from24-byte GPU inputs, with a32-byte primitive upload.
Whole batches publish atomically, and launches are bounded by arithmetic work.
OneAPI drains failed/cancelled copies before freeing USM/host staging. No CPU
fallback is introduced. No Volume state member or archive schema is added.

## Qualification and honest limits

The full optimized CPU native run passed107 of109 suites initially. The two
failures were inherited cold-ray/allocation-accounting fixtures from the newly
indexed picker: tests now separately prove cold-OOM preservation and zero-allocation
WARM picking; native worker cases prime bookkeeping then CLEAR the index, retaining
cold preparation and all exact fault oracles. Both corrected suites pass2/2 in2.56s;
product C++ was unchanged. All109 names are covered across those runs, not one
invented all-green initial run. Initial duplicate backend-symbol build failures
and the corrected linker-wrapper fixtures are also retained.

CPU real-loader XR passes main150 +resolution17 +procedural11 in201.18s. OneAPI
fresh core/render/UI and new procedural checks pass4/4 in18.35s: actualArc CSG12,
sculpt33, Document/controller35 and real-loader11. Independent CSG oracle39batches/
44745samples has maxerror5.15991e-08, plus large chunk/cancellation/concurrency
fixtures. New complete C+C++ core/xatlas/Mikk/UI/render sanitizer build passes
three new suites3/3 in4.88s with ASan/UBSan/leaks; this is not a full sanitizer
suite claim for the new candidate.

New FLTK GUI9+compact4 and actual nestedDomeD Wayland9 pass. Compact top/footer
and generated geometry captures were visually inspected. Initial DomeD8-check
run stopped on a harness variable shadowing its URL; its directory is retained
byte-exact as dome-menu-procedural-initial-shadow and corrected9 passes separately.
CUDA13.4.1/sm86 and HIP7.1/gfx1030 kernels compile and link; real nonempty dispatch
explicitly rejects absent compatible devices with host data exact. No actual
AMD/NVIDIA numerical execution is claimed. Initial HIP link invocation treated
an object as source; the corrected native host link and both results are retained.

The toolkit remains DomeD282467e/FLTK1.4.5+22; remoteHEAD was rechecked unchanged
and the new CPU binary links its prepared lib64 libraries even with LD_LIBRARY_PATH
unset. No system installation or active-session restart occurred. The physical
8f4d67d headset test remains stopped; do not relaunch without a user request.

## Continue

- Integrate the transaction agent's frozen combined candidate, retaining its
  cancellable native picker and paged no-throw state swaps. The procedural
  branch changes no Volume members; preserve both sets of Document controls.
- Import13b5156 MUST take safetyfollowup633bc7b for missing authored normals;
  general affine/bind/sourceUV/glTF/VRM work stays required.
- Colored-target baking/Scene10/material3 receipts remain guarded private work;
  the independent VRM resolved-role correction is being qualified separately.
- Broaden procedural production fixtures/curved placement/device performance;
  complete all other original authoring/interoperability/accelerator rows.
- Retain main's old frame WIP stashc46d1d9 and its byte-exact tar/patch/manifest.
  Never reapply obsolete prototypes over the integrated source.

Local live work/proof is recorded in artifacts/procedural-root-status.json. Poll
specific handles or authoritative processes before restarting anything.
