# Combined normal-source integration draft — 2026-11-15

The integration branch now preserves main `039f158b`, native-qualified component
`ea13136d`, graphical fixture `87c5595b`, test-runtime repair `c54f66f8` and
case-local shader-cache scenario `02d17f29` through real merges. It remains an
unfrozen draft: no combined build, main runtime admission or installation is
claimed.

The production implementation matches `1bc13125`. Relative to frozen source
`3d164382`, it changes three existing production translation units
(`bake-authoring.cpp`, `bake.cpp`, `emission-bake-source.cpp`) and adds the private
`source-bake-normal.hpp`. Public headers, shaders, scene/paint formats and render
packets remain unchanged. The existing FLTK/XR Bake controls use the shared
Document worker; this extends the application rather than adding a frontend.

The three added native test registrations make the prospective selected set
90 instead of87. The prepared-result graphics executable remains a manually
coordinated target, without automatic graphics CTest execution.

## Evidence and open qualification

- Native component Release and strict sanitizer qualification is recorded in
  [ARTIST-PREPARED-SOURCE-NATIVE-20261115.md](ARTIST-PREPARED-SOURCE-NATIVE-20261115.md).
- Frozen baseline desktop qualification passes208 checks. Two PBR synthetic XR
  cases pass194 each with the explicit test-runtime repair. These results do
  not qualify this combined source.
- Corrected graphical source `87c5595b` compiles and links; its independent
  neutral-presentation CPU diagnostic passes44 checks. The new software matrix
  actually exits1: manual and imported groups pass, while female/rest MToon0
  matches the correct oracle exactly but its wrong-normal control differs by
  only1, below the unchanged required gap7. The prepared group is not reached.
  The original result, images and logs remain under
  `artifacts/artist-normal-final-link-20261115/software-final-v1` in the main
  workspace. No full software or Arc graphical pass is claimed.
- The cache-enabled stereo scenario is prepared with all original checks,
  240-second/6000-frame limits and pixel bounds. Its per-case cache starts empty;
  actual cold misses, cache creation and later fresh-process hits are required.
  Its four-case qualification remains pending; old cold-cache PBR passes and
  MToon timeout remain separate results.
- The baseline current-toolchain sanitizer completion is still running; neither
  completion nor runtime acceptance follows from its live status.

Next, resolve the genuine graphical-control sensitivity, qualify software and
actual Arc consumers, finish the stereo/sanitizer checks, then freeze a combined
source and derive its matching build and regression scope. Preserve actual
compiler dependency and provider provenance when reusing complete objects.

All remaining production authoring, interoperability, physical input/device and
accelerator requirements remain required. Controller trays and spatial editing
nodes/history remain later phases. No physical headset launch or session restart
occurred in preparing this draft.
