# Initial original-coordinate source capture

This is a private development checkpoint on published VRM1 main `adfcb79`.
It enables bounded textured source UV0 outside the unit tile for native viewing,
restart and an actual initial editable-atlas workflow. It does **not** qualify
broader rendered atlas fidelity or complete the production goal. The new
source-to-atlas render failure below must remain visible before main admission.
Exact commands, sources, output hashes, input-map digests and negatives are in
[the receipt](validation/INITIAL-SOURCE-UV-CAPTURE.json).

## Implementation

An explicit SourceAuthored surface with original UV0 outside the first tile now
captures its actual geometry/maps without inventing a partial writable raster.
Every face is diagnosed as requiring preparation. Original coordinates, encoded
sources, image references and samplers are retained; none are clamped or replaced
with dummy images. UV0 uses the same finite10000 coordinate bound as the existing
UV1/extra-source channels. Strict writable and polygon UV policies retain their
unit-tile requirement.

The internal capture state is derived from the real UVs and policy, not trusted
from a serialized flag. Direct native painting refuses before accepting a job or
changing history, and explains the existing editable-atlas command. Bake and
forged persisted paint payloads cannot turn the capture into an implicit writable
domain. Snapshot and decode both reject allocated tiles, including hidden or
empty-looking ones, or incompatible prepared/baked data on that capture.

Explicit atlas import uses the existing source-aware baker/adopter. A pristine
source has no atlas-space layers or bake to transfer, so it need not rasterize
its original outside-tile coordinates as a legacy writable paint grid. Actual
images are sampled through their own original roles/samplers, while current
layers and their metadata move to the generated unit atlas. Inputs with actual
layers or bakes retain the existing transfer path. The Document TU was rebuilt
with the common admission predicate. No public class layout or Scene-format
field was appended.

## Evidence and source boundaries

The runtime is `128b0f7`. Merge `173ee52` absorbs published `adfcb79`, preserving
all local commits: runtime/headers are identical, provenance changes only used
prose, and CMake adds the public standalone JSON-forward-header fix. The final
selected build after this merge is a no-op. Receipt source hashes authenticate
these boundaries; documentation/receipt follow-ups do not change runtime.

- The first complete owned CPU app/core/support/render/OpenXR and13 selected
  targets build freshly in321.28s. Later coherent cached continuations include
  the final finite-UV-bound patch; no parent archives are reused in these builds.
- Final affected current-CPU10/10 passes in29.89s, including123 initial-source
  checks, the independent sampler, existing UV1 atlas/import/render, V1 export/
  worker/replay and real desktop lifetime. This is not a full regression pass.
- The final actual owned-sanitizer component passes123 controls. It freshly
  compiles paint, source-UV bake, import and Document TUs against immutable matching
  fully instrumented `f99ac01` core/support. Public layouts are unchanged; this is
  not a clean whole-frontend sanitizer build. The parent provenance-prose-only
  difference is checked separately from all API/source/SDK pins.
- Earlier matching Release7/SAN7 consumer sets pass source UV/bake/adopt/UV1/
  atlas regressions, including both real CC0 VRoid bases. Their runtime differs
  only by the subsequent finite-UV0 admission guard, separately tested above.
- The independent original-sampler oracle checks22,692 texels across repeat,
  mirrored-repeat and clamp. Every non-edge texel matches within one channel
  level;368 genuine nearest-edge ties obey the original fixed2e-5 coordinate
  bound and same channel bound. Each triangle supplies its expected raster
  interior. The initial arbitrary edge-frequency aggregate was corrected;
  individual sampling/error bounds were unchanged.
- Actual native FLTK chooser/options/atlas checkbox/worker/Save/Undo/Redo passes
  13 checks in27.80s with the final bound and matching canonical SDK. It checks
  genuine original UV0 outside the tile and new unit coordinates. The fixture
  needs Scene9, not the unrelated UV1 fixture's Scene18 assumption.
- The unmodified pinned official VRM1 Twist sample imports all13 parts in both
  original-coordinate and atlas modes. Current CPU and owned-sanitizer consumers
  each pass10 native import/restart/VRM1-export/independent-cgltf checks in
  16.62s/88.38s. Both current outputs are byte-identical across profiles and to
  the earlier independently validated ones. This does not evaluate physics.
- Seventeen retained emitted GLB/VRM outputs have0 Khronos errors,52 unfiltered
  warnings and170 infos. Two real Twist outputs pass58 pinned official schema
  instances with0 errors. Validation uses cached offline test dependencies;
  neither schemas nor official models are bundled as creation bases.

Creation remains the verified CC0 VRoid female/male models. The official sample
license/source pin is recorded in VRM1-SOURCE-EXPORT.md. No system installation,
headset launch, active desktop or display-manager change occurred.

## Genuine remaining render failure

A new software-GL fixture uses actual outside-tile raw UV0 and UV1, current morph
pose, authored normal/tangent attributes and the unchanged independent consumer.
At MASK/morph0.65, explicit512px source-to-atlas resampling has visible mean
error2.52312 above the fixed2 bound; all visible non-edge interior pixels match
exactly. Explicit1024px is a separate diagnostic and still fails at2.63049.
Both retain the original2-pixel edge footprint, mean/interior limits and rendered
images. Raising resolution did not fix the discrepancy; sampler minification/
mip behavior after reparameterization needs further investigation.

All native-to-independent-GLB comparisons reached before that failure have
maximum error0. That does not make the remaining original-to-atlas comparison
pass, qualify the unreached BLEND cohorts, or supply hardware evidence for this
new case. Original source bytes and native Undo/Restore remain retained.

Next work must preserve original per-role sampling/LOD where representable and
provide the initial independent writable authority needed for more general
cases, rather than hide the failure or assume a larger map solves it. Default
untextured/MatCap-only outside-tile captures also need a real owning adapter;
this checkpoint does not invent a white texture to admit them. The read-only
capture still reserves inactive CPU grid storage; lazy allocation remains an
explicit performance task. Full UV/VRM/HDR/physics, GPU, device and production
requirements and post-basic controller drawers remain active.

Initial malformed fixture alignment, omitted decoder guard, a correctly
alpha-masked native test point, wrong legacy fixture argv, metadata-only exact-
source refusal, missing generic Python test packages and wrong GUI schema/UV-role
assumptions are retained in local proof artifacts. Corrected tests do not relabel
those negatives as successful runs.
