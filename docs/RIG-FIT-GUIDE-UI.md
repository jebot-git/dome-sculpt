# Integrated rig-fitting guide UI qualification

Source `eec7753284f99b65fae94a22795917efaf63f6d5` passed all 52 existing
native fitting workflow assertions against the copied coherent optimized CPU
snapshot. This is a consumer run of the parent-built binaries, without a new
build, sanitizer run or hardware run in this branch.

The selected-guide repair compares a landmark's raw role identity rather than
its decorated display label. The visible label remains `hips (required)`.
Existing-rig bone lookup and index-based selection remain unchanged. The parent
component's independent 13-check renderer fixture supplies the precise colour,
affine position and old-code negative evidence; the controls screenshots here
are observational and introduce no pixel threshold.

| Existing gate | Passed | Seconds |
| --- | ---: | ---: |
| FLTK universal menu, normal | 8 | 11.736 |
| FLTK universal menu, compact | 8 | 12.137 |
| Native Wayland with private DomeD 62b6417 | 9 | 15.044 |
| Actual OpenXR loader with explicit synthetic runtime | 11 | 14.742 |
| Native controller input fixture | 16 | 0.008 |

The original assertions cover real Apply through the ordered worker, complete
source preservation, Cancel, one global Undo/Redo, immediate Save/reopen, native
window teardown and reflected/sheared controller-coordinate mapping. The
synthetic loader also checks nonblank balanced stereo swapchains and actual
focus-loss cancellation. Controller checks retain the stale-dialog and stale
cancellation lease negatives. No harness, runtime, header, schema, assertion or
tolerance changed for these runs.

All 1,458 frozen inputs matched before and after: 1,452 tracked source files,
four copied binaries, the compositor and the private Xvfb launcher. The app hash
is `decad1cc6d3bf07388f8713883c32fafa720d8871979c89103a2cede88a56d35`.
The machine receipt records every copied binary, gate command, environment,
evidence hash and elapsed time in
[RIG-FIT-GUIDE-UI.json](validation/RIG-FIT-GUIDE-UI.json).

The runs used private D-Bus sessions, temporary HOME/XDG directories and the
existing signed, privately extracted Xvfb launcher. Native Wayland used the
private nested compositor, SHA-256
`0400d3d7629d54d63dddc65ca127b6d341ad44b1de63c1301acae0f80fba0bb6`.
The app retained the canonical bitmap/FLTK library paths. The global XR runtime
was a nonexistent sentinel; only the XR harness installed a temporary manifest
for the copied synthetic test runtime. No physical headset, user desktop,
system installation or session restart was involved. All copied app processes
had exited at the final check.

Raw logs, screenshots, reports, the runner and matching input ledgers remain in
the ignored `artifacts/rig-fit-ui-eec7753/artifacts` directory. The private
Wayland bus logged a failed GVFS mount attempt during startup; all nine native
Wayland assertions and normal App exit passed.
