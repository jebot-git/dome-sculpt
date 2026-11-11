# Composed authoring candidate

`work/combined-authoring-20261111` combines the restored b8c469ce authoring source,
main746b3d58 milestone records and opt-in ArmorPaint experiment, the additional
spatial node/history requirement, and the two qualified XR fixture corrections.
All local commits and historical failures are retained. This is a reviewable
integration candidate, not main product admission or full-goal completion.

The composition at a3552bd4 was checked against the frozen source manifest used
by the current matched builds: all726 existing production, provider, header,
build, asset and packaging files are byte-identical. Its only other changes are
documentation, provenance, the independent opt-in experiment and the two test
files. Both test files match qualified fixture source17cd3b66 exactly. Read the
[composition receipt](validation/COMBINED-COMPOSITION-20261111.json).

Build evidence remains attributed to its actual b8c469ce source and binary
hashes; this source comparison is not relabelled as a fresh combined build.
The application is unchanged by the fixture and experimental additions.
The matching native/softwareGL and synthetic XR evidence is described in
COMBINED-BUILD-20261111.md and XR-SAVED-REDO-20261111.md. All production sources
remain frozen while current inherited-role sanitizer, ordinary GUI and strict
GPU checks use those exact owning binaries. The canonical SDK/helper remains
the verified DomeD acc2b70 version; no system installation or session restart.

Before main admission, close those current regression scopes and review their
failures and limitations. The independent ArmorPaint experiment does not supply
the full painting backend. Real active-layer GPU authoring is being implemented
separately through the native worker/document/history boundary; preview
composites cannot replace editable layer authority. Broader production,
interoperability, acceleration, physical acceptance and the remaining original
requirements continue after this integration milestone. Controller trays and
spatial node/history remain later requirements after basic authoring targets.
