# Native desktop 3D-mouse navigation

Dome Sculpt accepts the native spacenav Unix event stream on both Wayland and
X11. View → **3D mouse navigation** is enabled initially; without a daemon the
ordinary application works quietly. View → **3D mouse options…** stages speed,
deadzone, six individual inversions and frame-button swapping. Apply accepts
these application-session settings; Cancel, Escape and Ctrl+W discard edits.
Reconnect affects only this application's connection. No daemon sensitivity,
protocol mode, device configuration or system files are modified.

All six axes control the desktop camera: pan, distance and quaternion orbit
around the existing framed pivot. The two mapped buttons invoke the existing
Frame selection / Frame all commands. Each button must first establish a
released state. Navigation never changes geometry, source authority, authoring
history, Scene archives or the XR controller command registry.

Navigation pauses while another window/control, modal dialog, mouse gesture,
sculpt stroke, held object or spatial parameter control owns input. Release the
cap after resuming focus before moving again. Queued input, reconnects and
stalled event loops require a fresh neutral; button actions require a release
edge. The last received nonzero force expires after 150 ms. This intentional stale
stream policy means that change-only serial devices need the daemon's repeat
interval configured below 150 ms for sustained deflection. This application
never changes that global configuration. Physical-device testing remains a
separate required gate.

## Native protocol and bounded work

The client is an independent C++20 nonblocking `AF_UNIX/SOCK_STREAM` receiver;
there is no libspnav runtime or X11 event-protocol dependency. It reads native
32-byte legacy events with all six signed int32 axes and zero-based mapped
buttons. The wire period is preserved as unsigned metadata and never used as
elapsed camera time. Steady-clock integration, quaternion exponential and an
analytic normalized-screen pan/dolly solution make movement independent of
frame/packet counts. Deflection is deadzone-filtered and saturated before each
translation/rotation vector is normalized.

Discovery follows libspnav's explicit `SPNAV_SOCKET`, `/etc/spnavrc` socket,
`$XDG_RUNTIME_DIR/spnav.sock`, `/var/run/spnav.sock` conventions. An explicitly
set nonempty `SPNAV_SOCKET` is exclusive; an invalid override does not silently
connect to another daemon. Config inspection is limited to a regular file of
64 KiB. Unix paths are bounded before copying.

Each approximately 8 ms application pulse processes at most 256 complete
events and 512 recv attempts, with a single fixed partial-packet buffer. A
backlog is discarded through a drain/neutral fence, never replayed as camera
catch-up. Connect uses nonblocking sockets, zero-time poll/SO_ERROR for pending
connections, explicit AF_UNIX backlog EAGAIN retry, a 500 ms pending deadline
and 750 ms retry delay. Descriptors are CLOEXEC and closed on disable, EOF,
reconnect and destruction. Partial packets crossing an ownership boundary are
tainted and discarded without losing stream alignment. No motion callback
extracts geometry, copies meshes, reads GPU data or writes to Document.

## Producer/source audit

Primary references are pinned in `third_party/sources.json`. Libspnav
`476a17b6087146f95799e3a2a12b4d13cb4d2818` provides protocol constants and socket
conventions (BSD-3-Clause notice retained). Spacenavd
`05cc67938e77ac7ad9c5b34267984d944cfd57dd` was inspected, not compiled or copied.
Its `client.c` initializes protocol0 clients with motion/button events and
sensitivity1, so a receive-only client needs no handshake. `event.c` accumulates
axis updates, flushes on input synchronization or a button, and computes period
from unsigned wall-clock millisecond differences; zero/repeated/wrapped values
are not a reliable frame clock. `spnavd.c` repeats only nonzero devices when
configured; `cfgfile.c` defaults repeat to -1 (off). The application uses neither
wall time nor that period for integration.

Reference source:
[client defaults](https://github.com/FreeSpacenav/spacenavd/blob/05cc67938e77ac7ad9c5b34267984d944cfd57dd/src/client.c),
[event/period handling](https://github.com/FreeSpacenav/spacenavd/blob/05cc67938e77ac7ad9c5b34267984d944cfd57dd/src/event.c),
[native packet emission](https://github.com/FreeSpacenav/spacenavd/blob/05cc67938e77ac7ad9c5b34267984d944cfd57dd/src/proto_unix.c).

## Native process and GL lifetime fixes

The new clean-exit checks exposed two inherited defects independently of the
3D-mouse feature. Original 8d68dc7 App code reproduced the same Fontconfig allocation
report (11,977 bytes /275 allocations). A process-owned native Fontconfig scope
now starts before canonical desktop initialization, or before standalone XR's
Document/appearance monitor, and ends after the corresponding widgets, GL and
workers. Canonical discovery/configuration helpers remain byte-identical. Their
inherited-config discovery followed by private bitmap-config reinitialization
is preserved. Headless export/info/probe routes do not enter this scope.
Native smoke capture now returns through normal teardown; its delayed callback
is instance-owned and removed in Canvas destruction. Capture write failures
unwind with the existing failure status.

Original 8d68dc7 code also crashed on graceful Wayland close: Canvas destruction
called make_current after FLTK had destroyed its native surface. Canvas now
releases GL wrappers from its virtual hide before delegating context/native
surface destruction to FLTK. Repeated hide/show recreates wrappers normally;
unshown/partially initialized canvases have nothing to release. The actual
App/Canvas classes are exercised by desktop-context-lifetime.cpp, not a copied
lifecycle model.

## Scope and remaining gates

Final Release 4/4 (25.49 s) and selected fully instrumented desktop 3/3
(28.47 s) pass. Native protocol/camera 57, actual context lifecycle 14,
FLTK 26 Release/26 sanitizer and latest DomeD Wayland 11 checks pass.
Exact qualification and evidence hashes are recorded in
validation/DESKTOP-SPACENAV.json. The desktop X11 workflow has a graceful
ASan/UBSan/leak-clean exit. Native Wayland functionality and normal close are
qualified separately from process-wide shared EGL allocation reports.

The new standalone XR exit gate deliberately retains a failing LSan case on
this host: two 56-byte allocations map to libgallium-26.2.5.so. Original 8d68dc7 App
with balanced Fontconfig ownership reproduces the same 112 bytes at 16 frames;
current 64 frames also report 112 bytes. A diagnostic eglReleaseThread after
owned EGL teardown returns success without removing the report. A simpler
EGL triangle does not reproduce it, so this is not a claim that all Mesa use
leaks. Native Wayland's canonical FLTK global EGLDisplay initialization also
reports5,856 bytes /31 allocations after a graceful close, with no remaining
Fontconfig report. No shared EGL display is terminated by the application and
no sanitizer suppression is used. These inherited/shared-backend cases remain
open requirements; no whole-application or whole-suite sanitizer pass is
inferred from the scoped desktop gates.

Physical 3D-mouse and actual physical-device spacenavd acceptance remain open.
No system runtime, session or headset was modified. Controller-mounted spatial
drawers are a separate deferred full-goal phase after the basic workflows.

 Protocol
fixtures are real local sockets but are not proof of a physical 3D mouse or a
running physical-device spacenavd installation.
