# Combined painting and arena application qualification, 2026-11-11

The actual combined `27f01cad` application passes the unchanged final public blur
workflow: 39 checks in each Release/SAN real-loader XR run and 10 checks in the
normal Release desktop run. No product source, fixture scenario, native oracle,
deadline or frame limit changed for this qualification.

| Actual combined application case | Checks | Seconds | XR frames |
|---|---:|---:|---:|
| Release real-loader XR |39|12.82|164|
| SAN real-loader XR |39|40.18|389|
| Release desktop normal |10|13.30|—|

These are the newly relinked `paint-arena-build-20261111` application binaries,
with four fresh complete renderer/arena test translation units, the rebuilt
render archive and nine relinked consumers. They reuse unchanged providers from
the audited 331-output `775f92b8` owning closure per profile. This is an incremental composition;
it does not claim a whole fresh core build. Preflight and exit checks pin all
2,747 frozen source files, fresh objects, all relinked binaries, the render archive
and its members, and reused providers. All 714 product files outside the three
renderer/arena changes match 775, and the public header diff is empty. The native
inspector, producer fixtures, runtime DSOs and source-font reference remain the
previously qualified, compatible inputs. Every recorded source/provider/input
drift list is empty.

Both eyes independently recognize ROOT and all relevant page-title/checkbox
shapes with zero mismatches. Checked and unchecked Erase remain distinguishable
in each eye, and entering Filters while Erase is enabled presents the navigation
cell without an Erase checkbox. The actual public Blur command, selected Save,
Undo/Redo availability and subsequent palette disappearance reach the exact whole
native Scene endpoints. The Release and SAN final native files are byte-exact
with each other and the independent accepted oracle, SHA-256
`03fbccd73dfe743f2f316dab8d911ee73b4f8c8739880926c8cc0916346a9a3c`.
Original 6,000-frame/240-second limits and full native preservation checks remain.

The desktop run opens, blurs, saves, undoes, redoes and freshly reopens the actual
application through its public menu/key actions. In the complete source-defined
client canvas above the 28 px status line, before equals Undo, and accepted equals
Redo and fresh reopen. Blur visibly changes 234,080 RGB components, maximum 5,
within bounding box `(292,148,799,533)`. Every one of those five complete canvases
also matches its previous 775 capture byte-for-byte. Filename/status text is
outside this canvas; whole-window identity is not claimed.

Actual EGLDevice/DRM/PCI/i915 and live-FD evidence authenticate Arc A770 on
`0000:03:00.0`, renderD129. Mapped ELF witnesses pin the actual combined app,
runtime, observer and current EGL/Gallium drivers. The external query observer
returns the original query pointer and has no product-state or UI-progress
authority. SAN uses leak detection 1 and strict UB checks without suppressions;
the application exits cleanly after session and worker teardown. All device
leases are released. No additional GPU runs are planned for this scope.

The companion JSON pins recipes, source/font/runtime/fixture inputs, current
build receipts, statuses, actual captures and native/canvas comparison receipts
under `artifacts/paint-workflow-dso-build-20261111/union27f-*`. Earlier 775 failures
remain preserved in their original namespaces. The old 81/117 scenarios are
unchanged and are not relabeled as reexecuted here. Independent current renderer,
native and sustained-workload proofs retain their own scopes.

XR uses synthetic poses/actions and 320 px images; it does not establish physical
headset/controller usability. Desktop uses private Xvfb, without a native-Wayland
or shell-rendered-menu claim. Save follows Blur as the next public action; it does
not force busy-worker concurrency. The separate native pending-Save controls
retain that authority. No installation occurs here. Source-color preparation on
already authored physical-emission paint, broader resident painting/material/
baking work and physical XR testing remain open.
