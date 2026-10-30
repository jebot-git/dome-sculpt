# Affine held-object consumers

This private extension keeps full source placement during native desktop and
physical one/two-hand object movement. It retains E/TRS/original-local factors
and stages a detached immutable preview E. Tracking uses a similarity pivot at
the full object-world origin; it does not approximate object scale/orientation.
World gesture D becomes E_new=P^-1*D*P*E_old. Native-local edits rebase that
pivot explicitly. The per-frame path performs no affine SVD or mesh rebuild.

Transaction and AuthorityTransition snapshots now cover the preview owner,
native local pose and object-navigation anchor. Failed preview publication
restores these exact values. Full preview matrices are published to ObjectInfo;
ordinary Canvas behavior keeps its existing path. Affine XR viewing/menu input
uses scene coordinates rather than calling the forbidden Canvas approximation.
XR painting remains disabled pending its evaluated-world action backend.

Renderer previews use the full current/packed world delta. The shader enables an
inverse-transpose normal uniform only for affine motion, preserving the original
legacy shader path otherwise. Tangents use the linear map and determinant sign;
front-face state follows reflection. Original rest corners/UVs remain immutable.
The native FLTK mouse-drag path calls the same absolute world-delta API.

Release checks completed: 44 actual held/controller assertions, including
one/two/one-hand rebase, world translation/scale, release, focus-loss cancellation
and exact Undo; 173 assertions across 42 persistent main-thread allocation-failure
boundaries for preview publication; 27 actual graphics checks each on software GL
and Intel Arc A770. Independent sheared-delta normal/PBR tangent pixels match,
preview/stereo perform zero geometry upload bytes, release has no visual jump,
and cancel restores exact source/pixels. The original 13 static affine graphics
checks also pass. Eleven real FLTK/universal-menu assertions cover isolation,
individual visibility, native mouse preview/cancel/accept and exact Undo/Redo.
Canonical eee23b7 FLTK was used; explicit private Xvfb test mode did not change
Wayland-first product behavior or the active desktop session.

Sanitizer and existing held/controller regression proof is pending at this source
checkpoint. Actual persistent worker-thread denial must consume the separate
b395ff3 input/file worker-publication fix; the optional old --input-oom fixture
is not qualified here. Main-thread preview fault proof does not substitute for
that work. Full XR evaluated painting, moving-object query updates, general
affine import admission and remaining metric-aware authoring consumers remain
required followups. No system install, push or HMD test occurred.
