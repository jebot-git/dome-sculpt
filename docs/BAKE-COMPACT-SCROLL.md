# Compact Bake report scroll oracle

The doubled-font target report was initially captured32 pixels before its actual
scroll end: the fixture sent60 wheel steps (960 pixels), while the real content
range reached992. This hid the final numeric error value in the screenshot,
although it remained reachable. Private metric instrumentation confirmed the
actual scroll range; all exploratory UI/diagnostic changes were discarded.

The harness now reaches the bounded content end, sends another wheel batch, and
requires unchanged settled content as an end/saturation acknowledgment. The
unchanged native Bake editor passes all10 compact checks, and visual inspection
confirms the complete `0.0373` value above the fixed action buttons. No product
font, wrapping, control geometry or scrolling behavior changes.

Proof uses exact c9 headers/objects/archives from the isolated target tree. Only
the unchanged Bake editor translation unit was compiled into a separate artifact;
that tree and its active core/SAN/fault binaries were read only. Commands are in
`artifacts/compile-report-ui.json`, final log `artifacts/tests-report-actual-end.log`
and capture `artifacts/menu-bake-color-target-compact/color-quality-complete.png`.
