# Current combined build milestone

Candidate `b8c469ce875eb2542c220300c3c63edbf1526570` now has fresh persistent
Release and ASan/LSan/UBSan builds using the current canonical DomeD SDKs and
bitmap-cache helper. This is new evidence, separate from the historical Nov6
checkpoint. See the [scoped receipt](validation/COMBINED-BUILD-B8C469-20261111.json).

Each profile builds the selected32-target closure and passes22 native suites,
27 extraction-workspace controls and373 placed-workspace controls. All42 native
file pairs are byte-identical;168 actual loads through both current readers
produce identical decoded scenes. Both profiles pass the unchanged five
software OpenGL consumers,1727 checks each. All94 corresponding PNG/rawRGBA
outputs are byte-identical. Sanitizer runs enable leak detection and halt on
undefined behavior, with no suppressions or reported errors in these scopes.

The ten actual GLB exports per profile have zero Khronos structural errors,
five skinned-node-parent warnings and12 informational messages. These retained
warnings and structural checks do not establish complete VRM interoperability.

The original sanitized XR saved-Redo failure is reproduced on the current
driver. The stronger fixture checks visible Save selection and history state;
its current Release/SAN qualification is being closed separately. Current
system Mesa also exposes a112-byte JIT teardown leak in the whole XR app;
the original failures remain preserved while a privately recovered dependency
restores the previously patched Mesa route. Ordinary renderer consumers above
exit cleanly on current system Mesa. No leak suppression or product workaround
is used to turn either failure into a pass.

The separate inherited-role sanitizer readers, current ordinary GUI/compositor,
hardware and production-performance regressions remain required. Main product
admission and the complete original goal are pending. Controller trays and
spatial node-based editing/history remain later requirements after basic targets.
No new installation or physical-headset acceptance is claimed here.

Builds and raw outputs reside at `artifacts/build-20261111`. Its compact control
archive was read back and all33 member hashes verified; binaries/projects/images
remain outside that archive in the persistent build directory. Initial build
logs were overwritten by the subsequent native-target build; the receipt
explicitly retains this evidence limitation rather than relabelling later logs.
