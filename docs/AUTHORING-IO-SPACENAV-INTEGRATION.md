# Accepted authoring Save and native desktop navigation

Runtime `0c6002bdb5d3ff832125ab777867f5679d063e2d` combines the published
Scene20 authoring application with native six-axis spacenav navigation,
desktop GL/font lifetime fixes, common accepted-authoring Save and the portable
accepted-UV Save bridge. Test-only `0e623e7` adds spatial Save/Close assertions
and replaces two obsolete import tests that expected Save to refuse pending
authoring. No Scene21/22/23/24/25 data is introduced. The complete production
objective remains active; controller-mounted spatial drawers remain required
after the basic targets.

## Resulting behavior

Save and Save As can follow an already accepted background authoring operation.
The same ordered worker captures its exact committed immutable endpoint before
writing. Failure or cancellation propagates the original exception and leaves
the old file intact. A later Undo or document replacement cannot substitute a
different scene or become clean because of the earlier Save. The eight-entry
queue stays bounded. Close drains an accepted operation when a dependent Save
owns its endpoint. Native universal-menu Save, spatial Save and UV transfer use
the same completion mechanism; export retains its separate admission rules.

Native 3D-mouse input controls the existing framed desktop camera without mouse
emulation or document mutation. Six-axis settings, focus/modal ownership,
neutral/release fences, bounded socket work and reconnects are shared between
X11 and Wayland. DESKTOP-SPACENAV.md documents the daemon protocol and the
150ms stale-force policy. No daemon configuration is changed.

## Combined qualification

The qualification tree is `artifacts/spacenav-scene20-b197e37`. All owned ABI
consumers were freshly built: optimized Release alltargets and separate
ASan+UBSan C/C++ core, support, renderer, frontend and selected fixtures.
Release uses `-O3 -DNDEBUG`; instrumentation uses `-O1 -g1` with frame pointers.
System toolkit, graphics drivers and other system dependencies are not rebuilt
with sanitizers. Both builds use the canonical EEE toolkit/bitmap prefixes and
the explicit CPU compute backend; this is not accelerator qualification.

- Full Release: **373/375 passed**, 4616.37s. Both failures were the old import
  pending-Save-refusal assertion. Preserve that original log and JUnit.
- Test-only correction: the two registered import variants and the strengthened
  32-assertion authoring Save suite pass **3/3 Release** (0.13s) and **3/3 owned
  sanitizer** (0.99s). The import fixture checks actual pending Save, cancellation
  preserving old bytes, and exact accepted affine source graphs. This is not a
  new single 375/375 run.
- Selected owned sanitizer suites: **14/14**, 77.54s, including spacenav,
  framing, common/UV Save, caller admission, project persistence and worker
  publication. Separate desktop font/GL exit **1/1**, 29.40s, covers three CLI
  paths including a helper with 14 actual Canvas lifecycle assertions.
- Actual production FLTK camera/menu workflow: **26 checks Release +26 checks
  sanitizer**, private Xvfb; actual private latest DomeD native Wayland **11**.
- Actual App/universal-menu Save while the worker is blocked, followed by
  Ctrl+W and independent project read: **7 Release +7 sanitizer**. The fixture
  includes current App source and supplies a test-only worker gate; no production
  test hook is introduced. Instrumented X11 processes exit normally with leak
  detection enabled.

The narrow common-Save component, frozen `62d4747` in
`artifacts/ordered-authoring-save-5c94862`, additionally completed **all 9795
measured actual post-Bake publication/endpoint allocation denials in Release
and owned sanitizer builds**. Every denial preserves exact model, history,
content/recovery/cache and old file, propagates `bad_alloc`, and retries the
complete real Bake with one Undo/Redo. Preparation is not replayed from cache.
These are component proofs, not a combined 9795-boundary rerun or a universal
input-thread allocation guarantee. The receipt records exact helper, build,
binary and terminal-log hashes, including the initial old-header negative.

No new GPU pixel or headset result is inferred. The separately scoped Scene20
Arc761 and synthetic-loader evidence remains in its original receipt. Inherited
Wayland shared-EGL lifetime reports (5856 bytes/31 allocations) and the standalone
XR Mesa report (112 bytes/two allocations), also reproduced on the earlier
baseline, remain unresolved; no suppression or canonical-toolkit fork was used.
Functional Wayland success is not an all-routes leak-clean claim.

## Reproducibility and next work

Read `validation/AUTHORING-IO-SPACENAV-INTEGRATION.json` for source, binary,
build flags, scripts, logs, JUnit and component evidence hashes. Corrected test
sources do not change the frozen runtime. The preserving merge of main changes
only documentation. No new install, physical test or session restart occurred.

Image projection/stencils, PBR preview, retained paint domains and full rig
fitting are active separate basic tasks. Their private branches and new archive
versions are not admitted by this checkpoint. Integrate only with coherent
builds and actual composed workflow evidence. Complete every unfinished
REQUIREMENTS.md row before implementing and qualifying the additional spatial
drawer phase or declaring the overall goal complete.
