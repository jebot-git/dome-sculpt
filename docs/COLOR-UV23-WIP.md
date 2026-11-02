# Colour UV23 implementation state — not admitted

This private child is based on raw UV `aec07c2` and merged the root's unqualified
shared accepted-Scene source `62d4747` as `a50dbca`. Raw source/proofs stay frozen
in `artifacts/current-corner-uv-2e8699f`, final docs `57c1c42`. Target Scene20
allocation completion is separately frozen `1a30217`; its original42/44 plus
four same-binary ranges complete all51598 boundaries, with the two600s timeouts
preserved. No proof tree was modified while its jobs ran. All those jobs are now
terminal, with no GPU reservation.

Root approved Scene23/material8/colour-entry3 for durable prepared-current-field
UV replay. Scene21/material7 is reserved to the palette agent's intrinsic normal
sampling domain; Scene22 to independent corner/key geometry. Neither may be
silently reused. Old Scene20/material6 output must stay identical when no UV23
node is reachable. Material8 must later compose both colour and normal contexts
at union; this standalone base has no normal21 admission.

Implemented but only syntax-checked in this tree:

- `SourceColorPreparation::uv_reprojection` and noexcept `current_descriptor()`;
  initial `polygon_colors`, projection and receipt stay immutable.
- Public `ColorUvReprojectionReceipt`, `SourceColorUvLayout` and owning
  `SourceColorUvReprojection` with current descriptor/projection and complete
  strict before paint/material/appearance/deformation.
- Bounded LE binary layout codec, exact ordered REST/source-corner validation,
  stable topology-ID comparison excluding only UV, independent field-owner
  retention, and deterministic projected-pixel/quality replay.
- `SourceColorPreparationAccess::project_uv` creates a cancellation-aware unique
  target raster with a combined working-memory bound; it records actual raster
  samples as well as projection texels and adaptive error work.
- Current render/export and polygon owner/readiness helpers use the accessor.
  Initial-quality validation explicitly uses the initial descriptor.

`SourceColorPreparation::validate()` deliberately REFUSES every nonnull UV23 node
until the complete codec/before-state/owner admission exists. No UV23 factory,
native guard lift or prepared feature is claimed. Syntax-only checks passed for
the three changed implementation units; there is no linked current ABI build.
Do not link old raw/65/756 archives to this changed header layout.

Next implementation steps are real sourceUV integration and factory, conditional
material8/entry3 shared-field codec/reachability/budget/min23 scanning, immutable
initial-field versus current-field proof separation, actual measured quality and
full all-channel/PBR/baked/normal transfer, then native/controller/GUI/XR and
adversarial/fault/export quality. The source UV input currently only accepts the
raw own-PA witness from the previous stage; prepared kind2 is still guarded.

Actual Bake always contains a canonical normal map. Existing SourceUV correctly
refuses deformed normal transfer without proven fixed axes. Preserve that guard
until the existing canonical frame machinery is genuinely adapted and qualified
under lit skin/morph poses; unlit colour-only images are insufficient. Do not
borrow normal21 metadata or imply full arbitrary UV/topology transport from the
raw helper. Internal N-gon-corner UV splitting also remains a tracked explicit
face-cut/split follow-up.

Shared completion ownership is coordinated: root `AssetAcceptedSnapshot`,
AssetWork accepted_scene/completed/failure/done/save_requested, FileJob
asset_capture and EditorState can_save_after_asset replace the former cut-only
names. Palette bridge `b265706` owns queue_uv_locked plus endpoint staging and
post-commit publication/error in process_uv_transfer; its own SAN proof was still
building when received. Consume that bridge rather than duplicate it. This UV
implementation owns only the preparation section above `Scene prepared=job.scene`;
factor component payloads for that section rather than rewriting its publication
tail. Root generic Save source and palette bridge remain unqualified dependencies
until their final receipts arrive.
