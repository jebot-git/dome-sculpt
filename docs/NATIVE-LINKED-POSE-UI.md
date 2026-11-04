# Private linked-pose native UI and export follow-up

Actual female19 FLTK Rig dialog actions now pass34 checks at both ordinary and
640×360/large bitmap-font sizes. The native dropdown chooses the genuine Head
joint; real text entry and Apply affect all19 sibling-group parts. Escape and
Ctrl+W cancel staged values. One Undo/Redo and acknowledged native Save retain the
complete contextual Scene. Both saved outputs are byte-identical to the actual
Release and sanitizer worker outputs from04e6670. The App observer only reads
public state and widget metadata; XTest/D-Bus deliver real native actions.

Visual inspection found an existing compact hint overlapping Joint because it
used a fixed two-row height. The sole frontend change measures its bitmap text,
uses that height, and shortens the linked/reset labels. Both strict suites were
rerun after this change:80.40s/78.23s,2,437 captured source, application, archive,
toolkit, bitmap and actual GLX/driver inputs unchanged. The current software GLX
stack is recorded; the XR/EGL-only patched driver was unsuitable for FLTK GLX and
its first setup-only failure is retained. No device/HMD/session actions. Save
menu events follow native readiness; these GUI runs do not claim Save was clicked
while the worker was still busy. The earlier04e gated native worker tests cover
that accepted dependent-Save case independently.

Four independent parsed GLB consumers read the actual saved female/male native
linked and controller poses,19 parts each. They evaluate emitted POSITION/morph,
weights, joint/world and inverse-bind matrices independently in double precision
and compare every ordered current corner against native world geometry at the
unchanged3e-5m limit. All pass. Four pinned Khronos validations report0errors,
76warnings and8infos; no messages are filtered. The warnings concern non-root
skinned mesh nodes and the infos default matrices. These are current-field export
checks, not new artist-deformation quality oracles.

Build scope is explicit: matching765 public headers/frontend/support archives,
04e core module replacement, own Rig dialog module and the complete actual-App
read-only observer. The parent sanitizer core/support and04e native tests remain
separately qualified. These GUI/export checks are optimized CPU/software GLX;
not a new whole-project/sanitizer/GPU build. Fresh CMake configuration also passes.
Read validation/NATIVE-LINKED-POSE-UI.json for source/link maps, all cases and
preserved fixture failures.

The fixtures accept an actual completed19-part field, not an invented appearance.
Build the native-rig-pose-inspect, native-rig-pose-ui-test and native-linked-pose-
export-test targets in a fresh matching build. The Python runner accepts
DOME_POSE_APP/INSPECT/BUILD overrides, DOME_POSE_INPUT/OUT/XVFB and optional
DOME_POSE_COMPACT=1. No generic CTest registration fabricates the prerequisite.

Public typed locks and allocation coverage, dedicated Anatomical page32, complete
loader workflow and broader unskinned/prepared/relative-affine domains remain
required. No main admission or default-method change follows from this checkpoint.
