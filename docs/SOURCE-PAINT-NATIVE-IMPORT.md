# Source-preserving native initial import

Private runtime50d82e2 on qualified ab87887 adopts the independent source/paint
model in the actual initial Import worker. Existing authored UV0 is retained;
only genuinely absent UVs are generated. The explicit editable-atlas choice
creates a separate strict writable atlas, without resampling original images.
Actual PBR roles, samplers, normal frames, morphs, selected hierarchy and source
bytes are retained. Standalone glTF MToon no longer depends on a VRM root.

Typed-only UV1, MatCap-only and untextured inputs need no dummy PBR image.
An optional bounded immutable before-domain authored_uv1 field preserves real
coordinates unused by PBR; it is absent when an existing genuine PBR UV1 owner
already provides them. Factory replay, native resource forecasts, cancellation
and exact original attribute pairing cover this field. Public class layouts are
unchanged. Paint4/Scene28 remain private and are not main-admitted formats.

Fresh own native app/core/support/render Release and sanitizer builds pass in
594.03s/942.81s. Subsequent matching continuations include the corrected real
fixtures, empty-role backend eligibility and final test-only fault mode fdc7cef.
The final continuations pass in12.92s/14.81s with1539 tracked inputs exact each.
The new actual Import/paint/history/queued-Save/restart/graphics fixture passes89
checks in both profiles, with all compared images exactly equal. A deliberate
wrong UV0 substitution must visibly change the actual affine MToon UV1 shade.

All22 selected registered commands pass Release236.24s and leak-enabled owned
ASan/UBSan234.08s, all1573 captured inputs exact each. Existing source faults
cover639 persistent allocation and10 cancellation boundaries; genuine empty-role
plus supplemental authored-UV1 faults cover389 allocations and11 cancellation
boundaries. This is not a full558-command regression claim.

Strict actual Intel Arc A770 runs the new89-check import component with maximum
pixel error0; software-only --hardware correctly exits1. All1551 inputs remain
exact. This is GL component evidence on the CPU build, not selected-vendor
compute, whole native application or physical headset qualification.

Actual FLTK chooser/options, checked editable-atlas toggle, Source28 physical
existing project plus imageUV0/MToonUV1 input, Save and exact Undo/Redo pass13
checks on the rebuilt native app; all1555 inputs remain exact. The actual bitmap
dialog was inspected. This uses private Xvfb/bus/HOME and an archived adaptation
of the existing GUI harness, not Wayland or physical controller evidence.

The initial malformed float alignment and invalid UV1-without-UV0 fixture
refusals remain archived. Their fixtures were corrected without weakening loader
validation, bounds or pixel comparisons. See validation/SOURCE-PAINT-NATIVE-IMPORT.json.

Standard export of independent source sampling and its painted portable bake
remain unfinished. Legacy tests expecting resampled images require migration
with actual export/consumer coverage, not deleted assertions. Repeated UV,
bake/normal/colour/rig/key composition, broader interoperability/device and every
other unfinished REQUIREMENTS row remain required. Controller drawers remain
the additional phase after basic targets. Main remains0b5ada0; no system
installation, headset launch or active-session restart occurred.
