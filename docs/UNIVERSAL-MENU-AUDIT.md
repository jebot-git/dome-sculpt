# Universal-menu audit — 2026-10-29

The authority is DomeD's README at compositor commit
`3565a58178795bc8a7a03f90ca91f40477206f07`, especially the universal File/Edit/
Window division, modal input, shortcuts and application identity. The canonical
qimgvr FLTK exporter remains unchanged and its source hashes are retained.

Dome Sculpt exports actual File/Edit/View/Sculpt/Scene/Mesh/Paint actions. DomeD
supplies its application and Window menus, including presentation actions. Quit
is omitted from the application's File menu in a Dome session. File → Close
Window uses Ctrl+W. Super+F10 and compositor shortcuts remain shell-owned.
The local FLTK bar hides after host/association establishment and returns when
the host disappears. Check/radio state and command availability update live.

All application commands now receive modal-aware enabled states. Source-changing
actions are disabled while an asset job runs. Brush limits match the XR range;
layer actions respect layer counts and pending work. Generate UV atlas now
publishes an editable asset rather than discarding its generated result.

Edit targets the focused FLTK field, the UV draft or document history. Cut, Copy,
Paste and Select All are real exported actions with field/selection/clipboard
state. UV Undo/Redo is local until Apply commits one document transaction.
Unrelated document, view and paint commands remain disabled in modal dialogs.

The default application identity is `org.domed.Sculpt`, including native choosers
and alerts. A matching desktop entry is installed with the application. FLTK's
transient relationships allow dialogs to inherit their parent's universal menu.
Close invokes the window's close callback: native choosers must clear their
pending filename before hiding. An event-dispatch wrapper applies Ctrl+W/Escape
cancellation before a chooser text field consumes the shortcut. Confirmation
starts on Cancel and preserves unsaved work when dismissed.

Verification on private buses/displays:

- 25 exporter/UI checks passed on X11: action/state export, modal isolation,
  clipboard commands, parameter and chooser identity/parenting, UV draft history,
  safe Save As cancellation, host loss and host return.
- Eight checks passed against a running nested DomeD compositor with the native
  FLTK Wayland frontend and software Vulkan composition. Super+F10/header/row
  navigation activated Duplicate; a focused parameter dialog inherited its
  parent's menu, Cut and Undo edited its field, File Close cancelled the dialog,
  and Window → Send Window to Desktop remained available.
- The earlier compositor-test attempt incorrectly sent Escape before opening
  each menu, cancelling its own modal fixture. The final harness preserves the
  modal and asserts its continued presence when checking text edits.

Reproduce with `tests/menu.py` and `tests/dome_menu.py` using `uv run --with
 dbus-next --with python-xlib --with pillow python ...`. The latter accepts
`DOME_COMPOSITOR` and defaults to the adjacent repository's release executable.
No Qt/GTK fixture or active user-session modification is involved. These checks
verify native desktop integration; physical headset use remains unverified.
