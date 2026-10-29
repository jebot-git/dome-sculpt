# DomeD frontend and XR controls

The reference for this audit is the local DomeD compositor README at commit
`742d009f446cffdeb89031664b72885ff6f16a19`, together with qimgvr's canonical
FLTK appearance and universal-menu helpers recorded in the provenance manifest.
Shared helper files are unchanged. Dome Sculpt adds native application behavior
around them; there is no Qt, GTK or Pango frontend.

Desktop controls use the shared BDF-only Fontconfig/FreeType path, family/face,
native pixel size and 1×/2× bitmap scale. The XR palette uses that resolved BDF
file directly. Standalone `--xr` monitors DomeD appearance without depending on
FLTK's event loop; partial preference writes retain the last complete style.
The viewport background follows dark #303030 or light #d0d0d0. XR overlay space
remains transparent. Chrome uses 25%, 50%, 75% and 100% alpha (64, 128, 191, 255),
while labels remain legible and opaque. Accent colors follow the session.

The universal host receives File, Edit and application workflow menus, with
current radio/check markers and enabled states. The local bar hides when the
shared bridge associates a working host with the window. File → Close Window
uses Ctrl+W; the DomeD host owns application Quit and Window-management entries.
Native parameter dialogs cancel on Escape/Ctrl+W. Unsaved-work confirmation
starts on Cancel, including Enter activation. Document actions are isolated
from modal dialog editing. Super+X and compositor presentation shortcuts remain
reserved; the application does not bind them or XR system buttons.

Joystick press spawns an anchored twelve-sector BDF menu. Tilt and press choose
commands or open subpages. Numeric parameters open a large slider panel with a
current value, thumb and wheel feedback. Aim a controller or stylus ray and hold
the trigger to drag. The ray and crosshair show the intersection with the panel.
Horizontal stick deflection adjusts continuously, with a dead zone and elapsed-
time rate. Scalar vertical deflection is a fine wheel at one tenth that rate.
Color panels expose separate RGBA rows; vertical tilt changes the selected row,
then horizontal tilt adjusts it. Emission uses three RGB rows. Brush radius is
logarithmic, from 5 mm to 2 m, doubling per second at full horizontal deflection.
Layer order snaps to actual layer positions.

Controls wait for release/neutral after opening. Input capture prevents a slider
trigger from painting behind the panel. The original controller retains menu
ownership; the other controller/stylus can ray-drag a value. Press the owner's
stick or ray-click Done to accept and return to the parent page. The ray-clickable
Cancel button dismisses the preview entirely in headset. Material/layer
adjustments commit one undo transaction. Escape, owner tracking loss, focus loss and reference-space
cancellation restore original values. Unavailable actions are dimmed and cannot
activate. The fixed physical panel follows the existing OpenBrush layout; BDF
size/scale controls its bitmap label layout.

Native controller tests cover continuous adjustment, fine-wheel rate, ray drag,
secondary stylus operation, RGBA isolation, single-step undo and cancellation.
The real GL test checks slider movement, theme greys and 25% chrome alpha. A
private DBusMenu host checks exported menu behavior and native dialog shortcuts.
A synthetic OpenXR runtime checks the client through the real loader and GL
swapchains. Physical-device comfort, targeting and sustained frame timing remain
to be verified.

Current toolkit build selection and runtime-library verification are documented
in [TOOLKIT.md](TOOLKIT.md). The configured build launcher prevents an inherited
older FLTK installation from overriding the tested native toolkit.
