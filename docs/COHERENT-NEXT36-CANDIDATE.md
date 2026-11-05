# Next native36 authoring candidate

This source combines the genuine frame36/key export/post-key UV and bounded
frame-aware export compiler69fa717 with its test-only MatCap diagnostic correction
3065889, qualified local-cutout1231 and initial-shade factorizationa78fc022.
It also includes the qualified PBR shader79abe8e and portable native/XR keyed-UV
consumers e77ba52. Existing canonical-normal/UV/native-paint ownership guards are
preserved in all three typed VRM compiler/export entry points.

The initial-shade change appends a private Layer field. Its three complete
production TUs and every direct consumer must use the same headers. The public
native36 owner likewise requires matching whole application/core/render/support.
A new fresh Release ALL build and separate fresh owning sanitizer application,
core, render, support, runtime, inspector and selected whole consumers will
qualify this combined source. Component positives do not qualify this candidate.
Static source parity is recorded in artifacts/next36-build/MERGE-PARITY.json.

No Scene37/canonical paint codec prototype is included. Main's qualified PBR
milestone remains separate. Original69fa Release105/107 and its live sanitizer,
older full689 source regression and component failures remain untouched. No main
admission, completed combined tests, installation, headset or compute claim is
made here. The full production goal and later controller drawers remain open.
