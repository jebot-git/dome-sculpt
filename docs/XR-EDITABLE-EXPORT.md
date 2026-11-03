# XR editable-project export policies

Private runtime `7e5b16f` adds **Export options** to the shared joystick menu.
The page offers Strict glTF, Editable 256/512/1024, Avatar VRM, Native Save,
Cancel work, Back and Close. The three editable choices call the existing
finite-resolution PBR approximation plus exact native-project worker policies.
The bitmap page explains that the native project includes hidden objects, other
viewers receive approximate PBR, and finite textures can miss fine patterns.

Ordinary strict Export keeps its command and refusal behavior. Export policy is
captured in the accepted worker job; this change adds no stored policy, schema,
or data-layout field. Existing command identities remain intact; new commands
occupy explicit IDs 4096–4098 and page 28 extends the page count to 29. Back
returns to root without queuing work. Close and selected commands consume input
before editing resumes.

## Validation

The initial `f189f14` source built the optimized and wholly instrumented
C/C++ O1 ASan/UBSan/leak native app, core, renderer and support libraries from
scratch. Five selected tests passed in both profiles, including 65 actual
SpatialFrame/controller-to-Document worker checks. The sanitizer build finished
in 716.52 seconds and its tests in 79.26 seconds. All 1,598 tracked inputs stayed
unchanged. An actual software-GL palette capture exposed a warning overlapping
Close; that original capture is retained.

Runtime `7e5b16f` moves that warning clear of the controls and names hidden
objects explicitly. Matching affected consumers were rebuilt, and six tests
passed in optimized Release (10.34 seconds) and the same wholly owned sanitizer
profile (54.62 seconds). The final XR fixture has 69 checks in each profile:

- Actual joystick navigation reaches every editable resolution. Real worker PNG
  dimensions are 256, 512 and 1024, respectively.
- Native recovery retains the exact complete Scene, including hidden source
  objects. Document Open restores an unsaved native project without adopting the
  interchange path; native Save and a second controller export preserve it.
- The new page's Native Save command really writes the project. Held trigger
  input after Close, Save and export does not begin a stroke.
- Strict export reports its genuine unsupported painted-source conversion and
  leaves prior file bytes and live state unchanged.
- A test-only fsync wrapper gates actual durable temporary output. Controller
  Cancel work cancels running and queued snapshots, while an ordered native Save
  still drains and old output/live project data remain exact.

The regression set also covers input publication, import options, controller
controls and existing export cancellation. Real software-GL bitmap captures at
native/double scale in dark/light themes pass text-width checks and were visually
reviewed. All four captures are byte-identical across Release and sanitizer.
Eight actual GLBs are byte-identical across profiles; all 16 validations with the
pinned Khronos validator have zero errors and warnings, without issue filters.

The receipt is [validation/XR-EDITABLE-EXPORT.json](validation/XR-EDITABLE-EXPORT.json).
Ignored full logs, input/archive/binary hashes, outputs and captures are under
`artifacts/xr-export-proof`, `artifacts/xr-export-proof-final`,
`artifacts/xr-export-palette`, and the two build directories.

## Scope

This is synthetic tracked-controller input through the real native worker and
software GL, not a physical headset or desktop chooser test. No GPU lease,
installation or session change occurred. It adds access to the existing export
quality contract; it does not improve the finite PBR approximation or claim
unrestricted source-material portability. General Mirror refinement, remaining
seam-key authoring coverage and post-basic spatial drawers remain required.
The checkpoint is private until the coordinator completes coherent integration.
