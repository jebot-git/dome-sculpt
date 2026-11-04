# Native XR shapekey Mirror controls

This private milestone extends the existing shapekey workflow. In the XR
Shapekeys page, Mirror opens the shared control panel with Method, object-local
X plane (metres), and match tolerance (metres). The default method mirrors
vertex positions. Exact authored frames and relative artist-frame conversion
are explicit alternatives with their existing stricter geometric contracts.
The plane covers ±1,000,000 metres; tolerance covers 0.000001–1 metre.

Joystick row/value navigation and direct ray/trigger slider dragging share the
same controls. The named target is captured when opening the panel. Only the
explicit Mirror/Press action queues the existing background shape worker.
Cancel, focus loss, document replacement and a legitimately replaced source
cannot publish a stale Mirror operation. Save does not implicitly accept it.
One successful operation remains one undoable transaction, with the existing
first-source and named artist-checkpoint Restore semantics.

The implementation adds a named enum value without changing existing values,
class layouts or native archive/schema versions. A private helper validates
and decodes the normalized controls. The hidden fourth control value stores
one bounded target index; pointer/joystick edits reach only the three visible
components. Input handling does not scan geometry.

## Qualification

The complete original optimized CPU and instrumented app/core/support/render
builds succeeded on the current canonical DomeD FLTK patch27/POLL SDK. Their
first selected regression runs each passed5/6: the new fixture's authored UV
frames were not genuinely mirrorable. Those failures remain recorded.
Authentic reflected UVs and native normal-map capture fixed the test fixture;
no production authored-frame guard or numerical bound was relaxed.

Persistent allocation failure then exposed a real admission bug: copying a
long Unicode title could consume the draft control lease before an allocation
exception. Moving the saved control state with compile-time noexcept checks
now restores the exact draft and input lease. The old failing prefix1 is
retained. Complete changed document TU and complete native consumers are
qualified against the freshly owned parent support/frontend artifacts, with
byte-identical remaining product/API inputs. This is a matching component
qualification, not a second fresh whole build.

Release and sanitizer each pass59 native workflow assertions plus all48
persistent admission-denial prefixes and clean one-Undo retries. Six affected
legacy shapekey/Mirror commands also pass each profile, including the actual
`key-mirror-relative-kernel` registration.

The real OpenXR loader, EGL and OpenGL application passes40 assertions using
synthetic controller actions for all three methods, Cancel and focus loss.
Both eyes render, swapchain ownership balances, the off-centre rest-position
mirror oracle passes, existing keys and paint/skin/material/placement remain
exact, and actual Restore reaches the correct named checkpoint and first source.
A second complete runtime-TU run passes the same40 checks at1024×1024 per eye.
The captured settings panel was visually inspected: Unicode target title,
method, signed plane, tolerance, navigation hints and both actions are visible.
The first high-resolution probe retained an obsolete320px submission assertion;
its error-39 is retained. The corrected runtime checks each submitted eye's
actual swapchain identity, offset and dimensions rather than assuming320px.

Actual standalone sanitizer XR still exits with the unsuppressed Mesa112-byte,
two-allocation report. Native/model sanitizer passes do not override that failed
application check. The private Mesa investigation is separate; this milestone
changes no graphics-system libraries. There is no physical-headset or positive
GPU qualification of these new controls, full-suite pass, system installation
or main-branch admission claim.

See validation/XR-MIRROR-CONTROLS.json for immutable source/artifact manifests,
commands, results and preserved counterexamples. All original production
requirements and the later controller-mounted drawers remain required.
