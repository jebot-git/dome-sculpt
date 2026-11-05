# Affine polygon admission

Main merge37c46b9 admits qualified8750f20 with product source0567. Runtime,
headers, tests and CMake are byte-identical to that qualified composition.

Polygon placement, selection, drag planes, extrusion/inset distances, framing
and normal rendering now use the actual affine world metric, including shear,
reflection, pose and held placement. The original retained source, local/global
history and ordered Apply/Save/Restore remain intact alongside the transactional
retopology worker.

Both profiles pass248 combined native checks and16 exact native file pairs.
Ordinary Release and sanitizer applications each pass77 normal and77 compact
GUI checks, covering both tool orders in one application. The same ordinary
Release application passes actual Arc stereo retopology and polygon Apply/Restore,
world-point and both-eye ownership checks. Software and UHD runs are rejected by
the hardware-qualification observer; the ordinary application supports them.
Sources, inputs and observed immutable ELF mappings remain exact.

Private PolygonJob/PolygonAccept payloads change while Document, EditorState and
Scene outer sizes remain unchanged. The constructor/destructor and all accessing
providers are complete matched objects, with field/layout/reference audits and
old archive-member exclusion. No partial-function override or old-size facade is
used. One complete qualified current retopology-worker object replaces its prior
provider. This is a component relink, not a fresh full-project build.

Original layout/viewport/one-frame observer failures remain recorded. Closed
sanitizer proof ELFs are losslessly archived with full original-byte, ownership,
permission, nanosecond-mtime and actual-path restoration controls. Shared source,
objects, libraries and fixtures remain available.

Caller-input, weighting and remesh continuations still require coherent current
source qualification. Whole-project regression, remaining authoring,
interoperability, physical devices and accelerators remain part of the full goal.
No installation, headset launch or session restart occurs at this admission.

See [the admission receipt](validation/POLYGON-AFFINE-ADMISSION.json) and the
[unchanged qualification receipt](validation/POLYGON-RETOPO-WORKER9DD.json).
