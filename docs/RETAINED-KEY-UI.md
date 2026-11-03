# Retained-domain key native UI regression

This private checkpoint adds `tests/retained-key-menu.py`, an actual FLTK universal-menu/XTest test of the retained Scene24 key workflow. Its unchanged 9050 application fails the freehand Replace gate. It does not qualify the complete UI workflow.

The input is the actual `uv24.dsculpt` from the terminal broad native workflow, not a replacement generated for the UI test: 6,877,081 bytes, SHA256 `d6deb7b47165a960f92e644b68d6221e0bb3724634c267280c05021c2beb9b2a`. The fixture owns Paint2/material9, a writable retained paint domain and independently retained normal projection/domain, current skin/pose, raw sources, one existing key and first shape history. The copied application and inspector come from Root's coherent immutable 9050 snapshot; their exact hashes appear in the receipt.

Normal run `retained-key-normal-r4` exits **1 in 54.292499 s** with 13 positive checks. Compact run `retained-key-compact-r1` exits **1 in 54.013903 s** with 15 positive checks. Both perform actual default Return modal cancellation, native Create, immediate accepted Save, complete paint/normal/skin/pose/raw source/first-shape preservation, and exact universal-menu Undo/Redo. The compact run additionally proves the canonical large bitmap-font modal fits and scrolls with fixed Cancel/Apply controls. These are 28 positive checks in two failing executions.

The real held Shift-left gesture obtains and releases native editing ownership, then leaves the selected key unchanged. The immediate pre-Save screenshot shows the worker status:

> Visibility paint range omitted or fabricated its own sampling attachment

The entire saved Scene equals the exact pre-stroke `created.json`, including targets and source/history pools. App, inspector, fixture and harness before/after hashes match in both diagnostic runs. This establishes safe rollback and a real failed editing route; ownership alone is not counted as a successful hit or Replace.

Read-only inspection of 9050 identifies the shape preview refresh in `src/document-native.cpp`: its new `SurfaceRange` drops the owning paint attachment, normal proof and paint corner coordinates that the initial query supplies. `src/material-visibility.cpp` rejects the missing attachment against the material's retained paint node. Contracts owns the isolated runtime repair and reflected-winding audit. This checkpoint changes no runtime source and does not weaken that guard.

Replace, its exact Undo/Redo, held-gesture Cancel, graceful Close, a fresh frontend reopen, and post-reopen owned-modal cancellation remain present in the harness after the failing assertion. They have not yet been reached in these executions. The same harness must pass against the repaired app before claiming the full GUI workflow. No screenshot-only success, refusal-as-success, SAN, GPU, XR or physical-device qualification is claimed here.

The host intentionally has no Xvfb/xkbcomp/libXfont2 installation. Initial missing-tool attempts and the first private namespace `/run` setup failure are retained. Root authorized an unprivileged private download/extraction from the configured trusted Nobara repositories: signed Xvfb21.1.25, libXfont2 2.0.9 and xkbcomp1.5.0, with all RPM signature checks and SHA256 values recorded. Only the three x86_64 payloads are extracted. Two automatically selected i686 downloads are unused.

The server alone runs in an unprivileged bwrap namespace with private `/run` and private xkbcomp mapping. Its library path is scoped to the private server dependencies. The tested app remains outside that namespace and uses the unchanged canonical `bitmap-runtime-eee23b7/lib` and `toolkit-eee23b7/lib`. Runs own a private HOME/XDG tree, D-Bus session and automatically allocated Xvfb display; cleanup addresses only recorded children. No package install, root access, system configuration, existing desktop/display-manager session, headset or GPU is involved.

Run using the canonical library path, a usable private/system Xvfb on PATH and the verified application/inspector directory:

```sh
DOME_SCULPT_TEST_BUILD=/absolute/path/to/verified-snapshot \
DOME_RETAINED_KEY_OUT=retained-key-normal \
uv run --offline --with python-xlib --with pillow --with dbus-next \
  python tests/retained-key-menu.py
```

Add `DOME_RETAINED_KEY_COMPACT=1` for the 640×360 large-font route. `DOME_RETAINED_KEY_FIXTURE` accepts the actual retained input path; the harness verifies its exact qualified hash. Full local evidence and signed-tool provenance are in `docs/validation/RETAINED-KEY-UI-NEGATIVE.json`; the large scene, screenshots, logs and downloaded tools remain in ignored artifacts.
