# Synthetic XR menu visibility ordering

The first OneAPI frame/capacity integration run passed resolution17 but failed
its stylus palette-visibility assertion:45 balanced frames,38 stereo submissions,
real pressure edits and0 palette frames. The matching CPU run rendered2 palette
frames for that fixture. The failure logs/reports remain under the parent
artifacts directory and are not relabeled as success.

The fixture sent menu-open at scripted frame8 and the next menu click at10.
The application's input worker and immutable render snapshot acquisition are
asynchronous. Processing both clicks between two snapshot acquisitions can
produce correct input state changes without a rendered menu in that narrow
window. A fixed18ms synthetic frame sleep does not acknowledge publication or
rendering, especially during concurrent compilation or first-use device work.
This code-level scheduling dependence must not be diagnosed as a GPU frame
transport failure solely from an absent palette pixel count.

The three basic controller/stylus/sRGB fixtures now hold released input at
scripted step9 until the existing actual released-swapchain pixel inspection
observes a palette. Only then does the second click occur. All original image,
pressure, tracking/focus, reference-space, color-transport and ownership checks
remain. The fixture still must finish all45 script steps, with balanced actual
frame counts. The runtime then emits ordinary STOPPING/EXITING events.

Physical frame time advances monotonically during the hold; input script time
is separate. The reference-space event uses physical time plus its original
three-frame lead. A1200-physical-frame bound and the existing40s subprocess
timeout remain explicit failures. The OpenXR application/frame loop does not
wait for the input worker and no production source changes are involved.
Other fixtures retain their original frame-limited scripts.

Focused testing of the unchanged40ea548 OneAPI product passes all three modes:
controller45actual frames, stylus45, sRGB47; the sRGB case needed2 acknowledged
wait frames. Each renders4 palette eye images and completes balanced ownership,
focus and reference-space checks. This demonstrates an actual asynchronous wait
rather than adding an arbitrary delay or removing the visibility assertion.
The full loader regression is recorded separately after this focused proof.
