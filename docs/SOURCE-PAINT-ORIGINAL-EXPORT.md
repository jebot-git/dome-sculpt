# Original source sampling in standard export

Private runtime5f43249 compiles untouched independent Source4 paint into a
detached standard export view. It restores genuine original logical UV0 and
remaps original image-role UV1/additional arrays through the exact captured
current-to-before map. Original images, factors, samplers, normal attributes,
vertex colours, morphs and selected hierarchy remain actual source data. The
writable native atlas, metadata owners and history are not rewritten.

The actual FileJob used preview_surfaces rather than the generic surface path.
The first consumer exposed that bypass by correctly refusing a generated UV0
carrier1 as authored UV1. The additive Scene::export_surfaces method preserves
ordinary legacy preview semantics and explicitly compiles independent source
coordinates; native GLB and Avatar workers now call it. Class layouts, export
options and archive formats are unchanged. The existing per-object coordinate
staging cap is128MiB; supported validated Scene paint storage also bounds the
aggregate source-coordinate owners. This is not a whole-export memory claim.

The complete changed Scene and Document translation units are rebuilt in
optimized Release and owned ASan/UBSan against the same frozen parent core,
support and renderer. Old Scene/Document members are removed from the staged
archive. Final test-only adca44b reuses those exact unchanged runtime objects.
This is matching-component evidence, not a fresh current native application or
all-target build.

The actual background worker exports48 cases per profile: UV0 outside the unit
tile with original repeat/mips; original UV1 base/normal roles; untextured and
genuinely UV-less sources; authored/generated frames, two POSITION/NORMAL/TANGENT
morph weights, all three alpha modes and two selected instances. Independently
parsed GLB rendering matches actual native pixels within1. All387 checks pass
Release and owned sanitizer, including every freshly measured coordinate-stage
Cancel boundary and visible sensitivity to substituting wrong UV0 for real UV1.
Legal tangent absence uses the existing independent derivative generation path;
the UV1 case uses the existing per-role consumer instead of its UV0-only cousin.

Twenty-two existing registered commands also pass on21 unchanged matching test
objects relinked with the staged runtime: Release CTest26.27s/wrapper29.60s;
owned sanitizer CTest136.46s/wrapper159.10s. Original command arguments/properties
are preserved apart from isolated executable/output prefixes. All3136 captured
inputs remain exact each. This is not the full559-command regression.

Strict actual Intel Arc A770 passes the same387 checks with maximum pixel error1;
software --hardware correctly refuses. All1554 captured inputs remain exact.
All144 GLBs across Release, sanitizer and Arc are byte-identical by corresponding
case and validate with0errors/0warnings/468unfiltered infos/0hints. This is actual
GL plus Document-worker component evidence, not a whole native frontend,
selected-compute-backend or physical headset qualification.

Contributing paint still requires its real portable material bake. Both direct
compilation and the native worker explicitly refuse that unfinished path; prior
output, complete native data and immediately queued Save are independently
checked unchanged. No standard export of painted independent sampling, generic
typed-MToon GLB, Source4+HDR or general repeated authoring is admitted here.

The original full REQUIREMENTS objective remains active. Main remains0b5ada0;
the source-aware import/export work is private until complete coherent admission
and meaningful legacy-test migration. No system installation, headset launch or
active-session restart occurred. See validation/SOURCE-PAINT-ORIGINAL-EXPORT.json
and SOURCE-PAINT-NATIVE-IMPORT.md for exact source boundaries and preserved
negative runs.
