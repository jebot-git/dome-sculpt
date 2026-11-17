# Current canonical toolkit and effective closure — 2026-11-17

Live remote DomeD main and its clean local source are still
`acc2b70ae61dedec2a23727f67c60c0303d0f76f`. Both existing Release and
sanitizer FLTK SDKs retain the qualified source, generated poll configuration,
27 installed patches, public headers and library hashes. The canonical and
vendored BDF helpers match. No SDK rebuild is required by this source check.

The collected fltk-vr repository is separately at `4eb24b66`; its compositor
snapshot is older in relevant toolkit inputs and must not replace canonical
DomeD. Its unrelated dirty session packaging files were preserved.

A broad first check of the historical Nov11 SDK receipt found all16 recorded
`/usr/lib64` bitmap paths differ or are absent on the current host. This alone
was an overly broad runtime warning: those historical paths are absent from
both the actual qualified App1bc loader pins and the final combined build plan.
It does not establish a new runtime regression. The existing explicit
`/opt/fltk-vr/bdf-runtime` relocation supplies all16 exact canonical bitmap
libraries and the unchanged manifest. No old system files were restored.

The scoped read-only check completes with27,975 matching expected file checks,
including27,111 current whole-plan system/canonical headers, all228 component
loader input pins and all72 actual recorded providers through both literal and
resolved paths. No expected hashes were overwritten. The final combined plan
already selects this effective canonical prefix and current SDK; it does not
need replanning because of unused historical system paths.

Use the existing selected environment in that plan. Continue normal post-build
loader and graphical/device validation; static hash matches do not themselves
prove a new product run or full workflow. This check runs no project product,
ldd, probe, graphics context, build, installation or session action.

[Machine summary and compressed full expected/current rows](validation/CURRENT-TOOLKIT-20261117.json)
retain both the historical system mismatches and the corrected effective scope.
