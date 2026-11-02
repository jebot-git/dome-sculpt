# Current corner source Bake — private work in progress

The source-side adapter validates the actual current corner field against ordered
REST geometry before evaluating pose. It supplies that straight linear RGBA to
the existing separate-PBR bake path and leaves the complete original source
untouched. The native source picker exposes only the same conservative supported
role domain used by the current-color cut adapter. Current-field target Bake and
unsupported prepared/artist/UV1 source roles remain guarded.

A fresh regular-user CPU core build passed 35 native/direct checks, including
explicit preparation, source/capsule preservation, cancellation, posed and
zero-alpha-seam inputs, one Undo/Redo, Save/reopen and ordinary-target export.
Independent source-fidelity/render/export, actual native/controller/UI/XR and
sanitizer qualification remain unfinished. This is an implementation checkpoint,
not an accepted feature or permission to remove further guards. The next step
also inherits the separately qualified transactional cut admission fix.
