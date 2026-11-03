# Retained-domain key native UI

The final private GUI checkpoint passes **72 actual native FLTK checks**: normal 35 in 140.881230s and compact 37 in 140.941804s. Both use the same final harness SHA256 `5ccb64780e9a6d86752dfe26f48f7240916e40adcf27abedc676e871a04bbefb`. Reports, commands, binary/source provenance and artifact hashes are in `docs/validation/RETAINED-KEY-UI.json`.

The tested product is 9050 plus Contracts' runtime fix `e7a3c7c6931057a799dbdfcdfeb75b5583487d48`. Contracts rebuilt only `document-native.cpp` at O3, removed the old core member from a local archive copy and relinked the exact matching 9050 frontend/render/support objects. All 84 public headers and 34 parent object/archive inputs were verified against the supplied manifest before copying the fixed app and inspector. This is a matched consumer proof, not a fresh whole-application compilation. It adds no own SAN, GPU, Wayland, XR or physical-device qualification.

The input is the actual `uv24.dsculpt` from the completed broad native workflow, not a replacement generated for this UI test: 6,877,081 bytes, SHA256 `d6deb7b47165a960f92e644b68d6221e0bb3724634c267280c05021c2beb9b2a`. It owns Paint2/material9, a writable retained paint domain and independent retained normal projection/domain, current skin/pose, raw sources, an existing key and first shape history.

Both final runs perform real default-Return modal cancellation, Create and immediate accepted Save, complete retained paint/normal/skin/pose/raw-source/first-shape preservation, and exact universal-menu Undo/Redo. They then choose the existing new target through the actual Source target popup in the Rename editor; screenshots visibly show `Shape 1`. Applying its unchanged name preserves the complete Scene. One Undo still removes the real Create, proving the selection added no history entry; Redo and a real repeated selection prepare the intended slot for painting.

A genuine held Shift-left gesture changes only target 1, leaves all other targets exact, updates its real role-frame attributes and changes visible canvas geometry/shading. Current paint, normal event, skin/rest, saved mixed pose/weights, raw source bytes and the original first-shape capsule remain intact. One Undo/Redo reproduces the exact pre/post-stroke states. A second real held stroke cancelled with Escape preserves the exact Scene, saved Scene and Undo/Redo availability. Finishing preview, graceful universal Close, a new real frontend reopen, Ctrl+W cancellation of an owned Rename modal, accepted Save and final graceful Close all pass. The compact640 × 360 large-font route also proves modal fit and scrolling with fixed Cancel/Apply controls.

App, inspector, fixture and harness before/after hashes match in both final runs. Header hashes and fixed copied binaries were checked again after both terminal results. The large scene, screenshots and logs remain in ignored artifacts; the tracked receipt records their hashes and exact run statuses.

The original checkpoint `6e50d35` remains a meaningful negative, recorded unchanged in `docs/validation/RETAINED-KEY-UI-NEGATIVE.json`: 13 normal plus 15 compact positive checks followed by a genuine failed Replace. Its pre-Save status was:

> Visibility paint range omitted or fabricated its own sampling attachment

The guarded worker rolled back the complete Scene exactly. The shape preview query rebuilt its range without the owning paint attachment, normal proof or paint coordinates. The e7 runtime fix retains those owners and authenticates reflected corner winding; this branch contains only the test/docs, so the runtime fix must be composed separately.

The exact original harness was first rerun against the fixed app. Those runs correctly failed its target 1 assertion while the actual stroke completed and changed target 0: Undo had removed the new slot and clamped ephemeral selection to 0; Redo restored data, not selection. The final test explicitly selects the existing slot rather than changing product selection behavior or accepting any changed target. Intermediate keyboard-choice, destroyed-popup observation and compact pointer-offset failures, plus an earlier normal 35 pass on different harness bytes, are retained separately in the receipt. No earlier run is relabeled as final proof.

The host intentionally has no installed Xvfb/xkbcomp/libXfont2. Missing-tool and first namespace `/run` setup failures remain preserved. Root authorized private download/extraction from configured trusted Nobara repositories: signed Xvfb 21.1.25, libXfont2 2.0.9 and xkbcomp 1.5.0, with signature checks and SHA256 values recorded. Only three x86_64 payloads were extracted; two automatically selected i686 downloads remain unused. The server alone uses an unprivileged bwrap namespace, private `/run`, private xkbcomp mapping and scoped dependency library path. The tested app remains outside and uses unchanged canonical bitmap/toolkit libraries.

Each run owns a private HOME/XDG tree, D-Bus session and automatically allocated Xvfb display. Cleanup addresses only recorded children. No package installation, root access, system configuration, existing desktop/display-manager session, headset or GPU is involved.

Reusable private server PATH prefix in this workspace:

```sh
/home/blux/de-xr/dome-sculpt/artifacts/retained-key-ui-9050/artifacts/private-xvfb/bin
```

The wrapper scopes its own server library path; keep the app's canonical library path:

```sh
export LD_LIBRARY_PATH=/home/blux/de-xr/dome-sculpt/artifacts/bitmap-runtime-eee23b7/lib:/home/blux/de-xr/dome-sculpt/artifacts/toolkit-eee23b7/lib
DOME_SCULPT_TEST_BUILD=/absolute/path/to/verified-snapshot \
DOME_RETAINED_KEY_OUT=retained-key-normal \
uv run --offline --with python-xlib --with pillow --with dbus-next \
  python tests/retained-key-menu.py
```

Add `DOME_RETAINED_KEY_COMPACT=1` for the compact route. The verified directory must contain `dome-sculpt` and `dome-sculpt-prepared-normal-shapes-test`, both executable. `DOME_RETAINED_KEY_FIXTURE` accepts the actual retained input path; the harness verifies its exact hash. No physical-device authorization is implied by this test.
