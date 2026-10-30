# Combined sculpt protection and plane cuts — 2026-10-30

Application source `4755e62665c55ea6f70aa4cfda04b6c657f951d5` merges feature
`55dafa0` / source `2ec8a5a` with main `9a0cfc4` / source `20c132d`, preserving
both histories. Test-only commit `3e602ae` adds the canonical cold-BDF startup
allowance to the mask GUI harness. All 182 product source hashes remain identical
to the application source; see artifacts/cut-sculpt-product-freeze.json.
Branch: work/cut-sculpt-integration. The full production objective in
[REQUIREMENTS.md](REQUIREMENTS.md) remains incomplete. No agents were spawned.

Read [../AGENTS.md](../AGENTS.md), [AGENT-HANDOFF.md](AGENT-HANDOFF.md),
[VALIDATION.md](VALIDATION.md), [SCULPT-MASKS.md](SCULPT-MASKS.md),
[PLANE-CUTS.md](PLANE-CUTS.md) and the material/normal-frame/worker notes.
Exact prior handoffs and validation are archived in corresponding -55dafa0 and
-9a0cfc4 files. Keep all older creator/integration history as well.

## Implemented

- Native desktop/XR masks and object-local symmetry now coexist with plane cuts,
  Scene v8, source materials/UVs, skin, Unicode and cached retopology overlays.
  Masks page 15, Skin page 14, Retopo page 16 and Cut page 17 have independent commands, slider rows,
  labels, availability and confirming-controller ownership. Default desktop
  dragging remains trackball navigation. Runtime remains native FLTK/EGL/OpenXR.
- Cut previews retain exact original protection feedback; ephemeral cap vertices
  append the ordinary unmasked sentinel. Mask settings, bulk Fill/Invert and
  pressure cannot mutate an active cut source. Cancel/Apply/global Undo retain
  original Volume v3 protection/mirror settings. Static shell paint does not bake
  the display-only protection overlay, and new outputs have no stale overlay.
- Protection sampling now preserves constant scalars exactly using a validated
  uniform-field shortcut and nested difference-form interpolation. Sparse-cell
  constants and an independent affine field are tested. The original exact
  protected-cut assertion was retained rather than relaxed after exposing drift.
- Geometry availability and bake-source choices use actual sculpt samples,
  excluding mask-only empty volumes while retaining their useful authoring data.
  Cut cancellation/retry, one global publication transaction, accepted final-save
  draining and the six-worker predicate-mutex shutdown handshake are preserved.
- Volume/Brush/Sample consumers were rebuilt coherently. Nested Volume v3 and
  combined Scene v8/legacy codecs, quotas, immutable origins and live SourceUV
  normal-frame guards remain intact. No Scene v9 frame provenance was invented.

## Verification

All 76 registered Release suites pass across two runs: 75 non-XR suites in
625.60s, then openxr-client in 372.17s. Its complete real-loader synthetic report
passes 150/150, failed=0, complete=true, exit 0. It retains the upstream 140 cases
and the 10 cut checks, exercising actual GL availability, joystick/ray/pressure,
source/geometry persistence, cancellation, focus and shutdown behavior. The
isolated final run retains the original 40-second per-client timeout.

Whole-core C/C++ ASan/UBSan/leaks: 23/23 pass in 320.94s. All 125 native GUI
checks pass: masks 17+compact 5, cut 12+compact 6, retopo 12+compact 6, general 53, skin 11
and Unicode 3. Actual Intel ADL-N OpenGL additionally passes MToon 81, mask/mirror 38
and the full spatial renderer. These are graphics and synthetic tracking proofs;
no physical XR/input or new compute execution follows from them. The cut suite
has 68 assertions, including protected-source interoperation and empty-mask-source
eligibility. See VALIDATION.md for exact scopes, logs and initial diagnostics.

The first concurrent XR attempt reached 102 successful checks and 56 completed
cut frames before its client wall-clock limit. Its complete failure trace is
preserved. The final full isolated replay passes without a timeout/oracle change.
A misconfigured initial GUI compact flag and 30-second cold startup failures are
also retained; only the startup wait was aligned with the existing 90-second BDF
allowance. Interaction/publication checks remain unchanged.

## Build and continuation

Use build-cut-sculpt for this source. Older build/ and build-cut-integration
executables belong to previous stages. GUI scripts were copied unchanged into
artifacts/cut-sculpt-ui/tests, with build symlinked to build-cut-sculpt. The
combined core and selected executables were rebuilt in build-sanitize with
UI/XR OFF and C/C++ instrumentation, including Mikk/xatlas.

The unchanged canonical toolkit is DomeD 282467e / 22 patches, local prefix
artifacts/toolkit-authoring, runtime directory lib. All 30 manifest hashes and
the canonical qimgvr BDF helper match. Upstream's lib64 prefix is absent here.
The parent DESIGN_CONVENTIONS file is absent in this environment; use the
committed audits/references and canonical helpers without claiming a fresh
parent-file inspection. Preserve unrelated de-xr repositories.

```sh
cmake -S . -B build-cut-sculpt -DCMAKE_BUILD_TYPE=Release \
  -DDOME_COMPUTE_BACKEND=CPU \
  -DFLTK_DIR="$PWD/artifacts/toolkit-authoring/share/fltk"
cmake --build build-cut-sculpt --parallel 2
LIBGL_ALWAYS_SOFTWARE=1 LP_NUM_THREADS=2 LD_LIBRARY_PATH="$PWD/artifacts/toolkit-authoring/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
  ctest --test-dir build-cut-sculpt --output-on-failure -j2
./build-cut-sculpt/run-dome-sculpt
```

All stage build/test/app handles were terminal and the final process inventory
found none remaining. Artifacts/build directories are ignored local evidence,
absent in a fresh clone. No system installation or active-session restart was
performed. No fresh physical XR, external Khronos or compute-backend execution
is claimed. Local SDK discovery found no icpx/sycl-ls/hipcc/nvcc; older Arc/ONEAPI,
HIP compilation and actual Wayland-host results stay scoped to their archived
sources. No Python runtime or Qt/GTK/Pango frontend was introduced.

## Next work and full objective

Proceed to repeated group/multi-material cuts. Current commands affect only the
active object: capped mode rejects its open shell and open trim leaves sibling
caps untouched. Extend coherent source membership/material provenance, preview,
and one-transaction publication; do not infer group semantics from names.
General Booleans, stronger contours, source compaction/GPU preview caching,
durable recipes, quality controls, full new material/skin/morph transfer and
unconfirmed-draft recovery remain unfinished. Outputs currently use static
512-pixel PBR atlases while originals retain their complete source payload.

Every incomplete REQUIREMENTS row remains required: production resolution/local
storage/meshing and sustained VR; projection/stencils/texture sets and advanced
baking; retopo bridging/extrusion and procedural/plugin workflows; interactive
rig/landmarks/fingers/robust automatic weights and morph authoring; complete
avatar/MToon/glTF/VRM and independent consumer fidelity; physical controllers,
stylus/3D mouse/headsets, reconnect/session loss/performance, and eligible
OneAPI/ROCm/CUDA workloads. This checkpoint is not goal completion.

Private local-mesh and original-frame/creator prototypes referenced by upstream
are not incorporated here. Do not reapply old stashes or remove SourceUV/export
guards. Legacy UvDerivative pixels cannot be relabelled original-frame data;
future versioned frame work needs runtime/persistence/export and consumer proof.

Fetch and inspect status/remotes/collaboration before overlapping work. Public
repository: https://github.com/jebot-git/dome-sculpt at
/home/blux/de-xr/dome-sculpt. The parent is not a repository. Preserve all source
histories and unrelated projects; never force-push over newer remote work.
