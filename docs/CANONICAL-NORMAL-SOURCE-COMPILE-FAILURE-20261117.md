# Canonical source initial compiler failure

The approved compile-only source2356888e run is preserved in
`artifacts/artist-canonical-normal-compile-20261117/compile-v2`.
It stopped at the first compiler failure after 28 complete TUs and 29 full-M
prechecks passed. Command28, `emission-bake-source.cpp`, failed because the new
color/HDR branch names `hdr_emission` without including its declaring private
header. This child adds that actual `emission-authority.hpp` include. No flags,
thresholds, production semantics or tests are relaxed.

The original source worktree and all logs, objects, genuine depfiles and pins
remain frozen. Status/terminal receipt records no input drift, no resource
guard and no remaining owned processes. No archive/link/products ran. This
failure is not relabeled as successful compilation. The child remains uncompiled.
Any proposed reuse of a complete prefix object must prove its full actual MD
source/header/tool footprint unchanged in this child; no partial failed object
or synthetic depfile is usable.
