# Current published CPU application: all 540 test commands passed

The fresh optimized CPU ALL-target build recorded in UNLIT-CPU-BUILD.json has
completed the full registered 540-command CTest run. Its actual log contains
exactly540 Passed result records, every unique test ID1..540, zero failed results
and the final `100% tests passed, 0 tests failed out of 540` summary. CTest's
reported elapsed time is6185.97 seconds. Runtime/API/CMake/test/third-party and
packaging sources are byte-identical between tested39210fca and publisheddb8fc94;
subsequent publication is documentation only.

## Wrapper termination and recovered final hash audit

The long-running unified exec session91430 returned143. Its Python wrapper did
not write the final post-hash status; the original ignored `status.json` remains
unchanged and must not be mistaken for a live process. Both wrapper14125 and
CTest14408 are gone. The actual CTest result log is complete, but the wrapper
never captured CTest's standalone wait exit. We do not invent that exit code
or relabel the wrapper as exit0.

An independent recovery audit parses all540 actual Passed records and rehashes
all1905 inputs against the original pre-run capture:1484 tracked files,418 ELF/
static application/test/support outputs, the canonical FLTK/Cairo libraries and
the wrapper script. All1905 match exactly. The actual result records and hashes
are in validation/UNLIT-CPU-REGRESSION.json; local original status, CTest log,
registered command list, before hashes, `after-recovered.json` and recovery
report remain in artifacts/vrm1-vendors-fc562/artifacts/unlit-cpu-regression.
No test was rerun to replace the completed results. This is a full command-result
pass with a transparently recovered post-run audit, not a clean wrapper exit.

## Scope

This is the current unlit/VRM1 published CPU application, using the matching
canonical f797e0f FLTK/bitmap SDK. The suite runs with software GL, private test
displays and synthetic OpenXR fixtures. It does not establish physical-headset,
positive NVIDIA/AMD execution, full sanitizer-suite, HDR/source-paint private
branch integration or complete production functionality. Existing scoped Arc
and vendor evidence retains its separate source/command boundaries. Earlier
negative full-regression runs remain historical failures. Every incomplete row
in REQUIREMENTS.md and the later controller-drawer phase remains required.
No system installation, physical session or active desktop was changed.
