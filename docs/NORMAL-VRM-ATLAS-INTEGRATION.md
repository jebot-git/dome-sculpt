# VRM import and retained normal UV authoring

This milestone combines generic external VRM0/1 import with the published
editable-atlas/UV1/cut/input implementation and the retained normal/key lineage.
It extends the existing native C++20 FLTK/OpenXR application. The complete
production objective remains active and incomplete.

Native import now retains exact original source bytes and node/material/skin/
morph identities for the implemented external VRM0/1 subset. Typed PBR and MToon
sampling remain independent. MatCap-only sources no longer require mesh UVs:
the pinned [MToon specification](https://github.com/vrm-c/vrm-specification/blob/94e82dd346fa6cf0337c4421728640e5252dd38e/specification/VRMC_materials_mtoon-1.0/README.md)
defines view-dependent MatCap coordinates. Ordinary sampled roles still require
their declared original coordinate streams. The new native fixture covers both
versions, with and without explicit atlas preparation.

A separate typed-only VRM1 shade fixture has UV1 and no original UV0 or PBR maps.
An independent analytic image/UV oracle matches its prepared atlas within
2/255 in linear channels (observed maximum0.00195503). Real native painting,
immediately ordered accepted Save, separate import/paint Undo/Redo and native
restart pass13 assertions. No fabricated PBR map supplies its source authority.

The prepared-normal/key implementation reauthors supported original artist
frames. Normal-only UV editing now owns its before-state, layout, intrinsic
planes and bounded quality/replay receipt through Scene19/material archive5.
It preserves initial source attributes and can restore them after restart.
Post-UV key authoring remains a separate next component; the old source model
is not relabeled to bypass its guards.

The coherent optimized UI/OpenXR/all-target build passes all298 configured
Release suites in1057.82s, including the complete772.82s cut allocation sweep.
Fresh owned C/C++core/support/renderer sanitizers pass15 selected suites in119.72s.
Five actual A770 consumer batches pass542 assertions. Native FLTK VRM11,
normalUV13 and atlas12 checks pass, followed by8 actual native Wayland/universal
menu checks against the latest private d271cf6 compositor. No active desktop
or display-manager session was restarted.

After that full run, a pure test registration adds the actual-avatar XR import
fixture. It separately passes1/1 suite with20 assertions: real OpenXR loader and
GL swapchains, synthetic joystick controls, all19 VRoid parts/88groups, explicit
64px atlas preparation, exact original bytes and cancellation. This is separate
from physical headset acceptance. The later merge preserves public27aa478;
product source, headers, assets and runtime bytes match the full-run source9081677.
The result is298/298 plus the new1/1, not a relabeled single299-suite run.

All104 generated GLB/VRM files pass unfiltered, unlimited Khronos validation with
0errors/439warnings. All27 emitted VRM0 files pass the15 hash-verified pinned
official schemas. Raw original legacy source diagnostics retain their separate
component scope; original bytes are not rewritten to manufacture clean inputs.
Exact source/report/binary hashes are in validation/NORMAL-VRM-ATLAS-INTEGRATION.json.

Generic source-space VRM re-export, broader expressions/physics/constraints,
post-UV keys, current-field source/target baking and the remaining requirements
are still incomplete. Screenshot review also found that the initial desktop
view crops an imported avatar; usable Frame all/selection and orbit framing are
a concrete follow-up. The accepted physical selection UI and saved headset
session remain preserved. No new physical test or system installation occurred.
