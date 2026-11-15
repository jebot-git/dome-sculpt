# Final87 strict sanitizer current-runtime boundary — 2026-11-15

The goal has resumed. Frozen source `3d1643827dd56e1b2ef1d57b8a9c6ff1e4211a2c`
remains unchanged. Its final combined 87-case application has not yet been
materialized or accepted by this milestone.

The first new sanitizer prepare-only audit failed its original expected-input
check before CMake configuration. Session43894 closed1; zero configured project
objects and no CMake cache were created. Historical SDK418 pins expose16 changed
system paths after the pause:13 existing FreeType/HarfBuzz/Pango libraries changed
content, and3 old Cairo2.11804.4 paths no longer exist. Current system Cairo
SONAMEs resolve2.11806.6. This failure is preserved and is not relabeled a pass.

Every one of those16 immutable historical expected library payloads remains
available byte-exact in `/opt/fltk-vr/bdf-runtime/lib/` under the same basename.
Independent full418 verification proves402 unchanged paths plus16 explicit
same-payload relocations. The canonical DomeD manifest checker passes for that
private prefix with its library path selected. The historical SDK manifest,
provider status, failed audit and frozen source are untouched. No system files
were installed or replaced.

A new derived prepare-v2 expected-input map redirects only those16 specific
historical paths after checking their expected hashes. The derived receipt and
original manifest bytes are pinned. CMake selects `/opt` pkg-config metadata and
private libraries explicitly. This relocation does not make include/compiler/link
commands equivalent: actual new paths are compared literally, all `-D` values
are preserved, and changed complete owners and affected consumers must rebuild.
Only genuine original strictSAN Ninja/compiler/dependency witnesses and exact
completed5c whole providers are eligible; no synthetic dependency records or
Release objects enter the sanitizer closure.

The new read-only audit is running independently in
`artifacts/guard87-final-owning-sanitize-plan-20261115/`, tag
`sanitize-3d1643-private-v2`. Plan counts, DAG closure and source/provider checks
must be reviewed before any materialization. No native, graphical or headset
runtime has run for this milestone. The executor retains strict ASan/UBSan,
7200 seconds,2 active tasks,8GiB descendant RSS, and600-second initial/final
input audits; only the explicit private runtime library path changes.

See [exact418/16 boundary](validation/guard87-sanitize-canonical-relocation-20261115.json),
[original failure](validation/guard87-sanitize-original-audit-failure-20261115.json)
and [planner delta](validation/guard87-sanitize-private-prefix-20261115.diff).
This is a boundary/provenance milestone, not final87 build or full-goal acceptance.
