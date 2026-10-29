# Native authoring publication integration; full goal remains active

Read AGENTS.md, REQUIREMENTS.md, HANDOFF-466ebd1.md and the parent de-xr
DESIGN_CONVENTIONS. Native C++20/FLTK desktop/OpenXR, all required authoring,
interoperability and accelerator workflows remain required. Animation and final
offline rendering are excluded. Do not mark the goal complete at a checkpoint.

This isolated publication branch combines local d505e97 (deferred UV,
SIGINT/SIGTERM save/cleanup, actual Arc SYCL and scale-robust meshing) with
origin e65db2a (independent integration/worker contracts and regressions),
preserving both histories. Its canonical private queue helper order is work as
argument5, changed as argument6; every call must agree. No pre-header-change
archives can validate this source. Additional dirty v7 material/export and skin
UI work remains in the primary working tree, excluded here until tested/committed.

Prior exact evidence is intentionally separate:
-6947fb2: all50 Release suites128.14s; native menus53/53 and UV GUI12/12.
 The actual signal/save regressions are included in its full OpenXR suite.
-f7eb5b9 UV patch on7cc:5 Release,5 whole-core ASan/UBSan/leak,12 nativeGUI,
 full syntheticXR and softwareGL; see UV-SOLVE.md for exact scopes/logs.
-d505e97 numerical/SYCL candidate on6947: actualArc production core5/5,
 CPU core38 checks and18 scale cases; see COMPUTE-VALIDATION.md. Most geometry,
 paint,bake andrig work remains host-side; CUDA/AMD hardware unverified.
-Incoming e65 evidence: archived VALIDATION-e65db2a.md; not proof of this merge.

The current integration requires fresh combined CPU/GUI/OpenXR and appropriate
native sanitizer/device checks before main publication. Canonical toolkit and
compositor source282467e, artifactprefix toolkit-282467e, are retained; no system
installation or active session restart occurred. Physical8f4d67d basic WiVRn
operation was user-confirmed, then ended with recovery snapshots retained;
that does not validate every device/workflow or later source changes.

Primary-tree ownership is queried through collaboration tools, not inferred
from historical agent names. Current work covers connected Skin bind/transfer,
v7 immutable source material persistence +GPU emission composition, paintedVRM
and original normal/tangent transport. Normal-frame sourceUV restrictions stay
until skin/morph/version-specific rendering/export parity is proven. Preserve
all source images, original IDs, paint/UV/rig/morph data and accepted-worker
cancellation/save ordering. Keep every remaining REQUIREMENTS row active,
including production sculpt masks/symmetry/resolution/local meshing, retopo/cuts/
procedural/plugin equivalents, fullglTF/VRM, Unicode and physical/compute proof.
