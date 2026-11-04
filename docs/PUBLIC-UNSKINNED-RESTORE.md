# Complete originally unskinned source restoration

The existing Restore-first-source action now restores an authentic capsule even
when its original mesh had no skeleton. It recovers the complete original paint,
material, shapes, masks and rig absence, rather than refusing or merely removing
current skin data. Linked fitting must be off; hidden/locked targets remain
protected. Full source validation and cancellation precede final adoption. Native
and FLTK/controller labels now say Restore original source.

This ports a5ab8c3 onto public9987 without importing private Source4/HDR/weight
runtime. Only four product TUs change; no public layout or native schema changes.
Complete changed core3+FLTK editor and ten affected native consumers are rebuilt
against current27/POLL DomeD FLTK. Other byte-matching public atomic-IO parent
core/support/frontends are copied and audited; this is not a fresh whole build.

Ten actual native commands pass each optimized Release and owned ASan/UBSan/leak:
23.97s/191.94s execution,115.72s/363.06s including compilation/relink. Each has
37 complete unskinned-source/native/controller/orderedSave/UndoRedo checks plus
134 persistent allocation-denial prefixes and exact retry, with existing rig and
real CC0 source workflows preserved. The identical fixture on old public core
really fails with its original missing-rig-source refusal; that negative is kept.

Actual native FLTK GUI passes62 assertions across Release/SAN normal and compact
640x360/font32. Of these30 exercise real originally-unskinned Restore checkboxes,
queued Save, exact original/current full-Scene Undo/Redo and clean close;32 cover
existing rig fitting. All four new-dialog capture pairs are byte-identical across
profiles and the compact controls/footer were visually reviewed. Native Wayland
existing-rig fitting passes9 checks per profile on a private nested compositor,
with currentSDK, normal cancellation/history/Save and clean teardown. This is no
physical-headset or active-session claim.

The test helper/driver shipped in tests are byte-identical to the actually used
artifact sources. Both canonical CMake helper TUs are rebuilt; full generated
fixture bytes, native inspector and expected-source payloads match the exact
GUI-qualified helpers. Initial missing-PKG_CONFIG discovery refusal and the
command observer's duplicate-source/target indexing mistakes are retained; no
product/compiler flags were changed to bypass either. Every one of the14 actual
compiled complete-TU commands has an exact target match after output-path-only
normalization.

See the [complete receipt](validation/PUBLIC-UNSKINNED-RESTORE.json) for actual
commands, strong input hashes, source boundaries and preserved negatives. The
full production objective and remaining original/additional requirements remain
active. No system installation or session restart occurred.
