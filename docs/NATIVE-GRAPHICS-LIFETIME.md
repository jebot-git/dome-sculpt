# Native EGL acquisition ownership

The windowless graphics owner now releases successfully acquired EGL resources
when setup throws. Previously a failed API bind, configuration selection, context,
surface or make-current operation bypassed the destructor and retained part of
the acquisition. The actual old implementation fails the independent vendor
resource-accounting test in both Release and ASan/UBSan builds.

One nonthrowing cleanup path handles partial construction and ordinary
destruction. It releases only this instance's resources and unbinds only when
its own context is current. Failed make-current and later explicit switches to
an unrelated current context no longer unbind that context. Copy construction
and copy assignment are deleted, preventing duplicate destruction of EGL handles.
Class size, alignment and all four existing field offsets remain unchanged;
there are no new Scene fields, formats, frontend controls or toolkit changes.

The registered `native-graphics-lifetime` fixture compiles the production owner
against a deterministic vendor API. Its31 checks pass in optimized Release and
O1 ASan/UBSan/leak builds. They cover nine failed stages, successful primary and
device-fallback setup, six partial device-fallback acquisitions, resource order,
unrelated current contexts and noncopyability. This is vendor-response fault
coverage, not physical device-loss verification.

The actual software-GL guide fixture passes13 checks in Release and13 with
whole-owned sanitizer support. The real native application passes three
standalone synthetic-OpenXR exit/Save/info checks in Release, and two desktop
capture/error-unwind checks in each profile. These consumers replace only the
graphics implementation TU against immutable matching `eec7753` application/core/
renderer support. The synthetic sanitizer runtime is freshly instrumented.
This is not a clean whole-project rebuild or hardware-headset qualification.
All1,453 captured source inputs and115 support inputs remain unchanged during
the final proof; exact commands, durations and hashes are in
[the validation receipt](validation/NATIVE-GRAPHICS-LIFETIME.json).

The sanitizer standalone-XR application still reports112 bytes in two inherited
Mesa allocations after completing16 frames and destroying its session. That
negative is retained, with leak detection enabled and no suppression. The
inherited shared-Wayland EGL5,856-byte report is neither requalified nor fixed by
this component. Earlier observation setup failures (missing private Xvfb PATH,
then an assumed unbuilt parent runtime) are retained separately from the
corrected explicit-mode runs. No system package, active session or physical
headset was changed. The complete production goal remains incomplete.

The subsequent [native display integration](NATIVE-DISPLAY-LIFETIME.md) qualifies
the actual desktop connection owner with the corrected canonical FLTK/Cairo
stack. Its stronger current native Wayland scope is separate from this original
EGL-only component; the standalone-XR Mesa report remains open.
