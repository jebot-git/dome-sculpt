# Spatial palette Unicode

The spatial palette keeps DomeD's resolved BDF file and native appearance scale
as its primary font. It no longer interprets UTF-8 bytes as ASCII glyph IDs or
truncates model, source, joint, morph, layer, recovery, status and error names
with byte `substr`. Native FLTK text entry continues to supply UTF-8 labels;
the same names now reach the XR palette without losing non-ASCII characters.
This change does not add a separate XR keyboard.

The private `src/palette-text.*` helper uses FreeType with monochrome rendering,
then DomeD's canonical Unifont coverage preference and Fontconfig's
`monospace` family/spacing preference to find a font covering
an entire extended grapheme when the selected BDF lacks it. Loaded fallback
faces are reused. Bitmap fallbacks select the nearest strike and use integer
pixel replication when needed for enlarged appearance, or integer decimation
for an oversized fallback strike; scalable fallbacks use
the requested pixel size. DomeD's canonical `native/fltk_chrome.cpp` and shared
`third_party/desktop/bdf_font.hpp` provided the primary-BDF, nearest-strike,
monochrome and Unifont-coverage conventions. Fontconfig already receives the
canonical BDF catalog exported by the existing session appearance helper.
No font substitution changes the user's primary face.

ICU replaces malformed UTF-8 with U+FFFD, segments extended graphemes (including
combining marks and ZWJ sequences), and computes visual bidirectional runs.
HarfBuzz shapes script/font runs, retaining joining and combining behavior
where the installed font provides it. Clipping measures actual shaped advance,
reserves a real ellipsis, and retains complete graphemes. It uses the existing
button bounds and appearance scale; glyph ink is additionally clipped to the
label's horizontal cell. Unsupported characters use the installed font's
missing glyph only when no installed fallback covers their complete grapheme.
Neither byte omission nor ASCII question marks stand in for Unicode support.

Each input label is bounded to 4096 UTF-8 bytes; the last grapheme is discarded
when that budget cuts a sequence. Face selection is bounded to 32 live faces
and 2048 grapheme selections. The LRU glyph cache holds at most 1024 glyphs and
8 MiB of bitmap pixels. Font dimensions are limited before bitmap allocation.
Clipping measures first and rasterizes only the selected text. Name changes
invalidate the palette texture even without a geometry revision. Colors,
linear/sRGB conversion, opacity, joystick geometry and page 12/13/14 routes
retain their existing contracts.

## Dependencies and source provenance

The renderer now links native `fontconfig`, `harfbuzz`, `icu-i18n` and `icu-uc`
in addition to its existing FreeType/EGL/OpenGL dependencies. These are
pkg-config dependencies for both desktop and XR renderer builds, with no
Qt/GTK/Pango runtime. No font binaries or external text-renderer source were
copied into this project. The helper and tests are original GPL-3.0-or-later
project code. The existing shared BDF discovery helper remains unmodified.

The validation host provides Fontconfig 2.17.0, FreeType pkg-config 26.6.20,
HarfBuzz 14.1.0, ICU 77.1 and DomeD's installed Unifont 17.0.04 plus CJK BDF
faces. The test writes `palette-unicode/fonts.txt` with its exact primary source
and Fontconfig configuration. It derives a temporary ASCII-only BDF from the
resolved canonical Unifont at test time, so accented Latin/CJK names necessarily
exercise a real missing-primary-glyph fallback. Font license/distribution is
unchanged; no generated fixture is installed.

## Validation

`palette-unicode` compares original BDF ASCII bitmap/advance identity, verifies
non-ASCII Latin/CJK/RTL ink and fallback, valid suffix preservation around
malformed sequences, complete combining/ZWJ clipping, all widths in a range,
unbounded combining-input rejection and real glyph-cache eviction. It renders
actual OpenGL palette captures under software EGL, including skin page 14,
weights page 13, independent name changes, 2× light appearance, sRGB/linear
output and translucent chrome. Captures are PPM artifacts for visual review.
The existing `spatial-gpu-render`, appearance, native GUI and real-loader XR
regressions remain part of milestone verification. Exact results are recorded
with the milestone after those checks finish.

Milestone evidence uses an isolated source snapshot of committed `55f9bcf` plus
only this Unicode scope, freshly rebuilding core/renderer/UI with Release
optimization (`nice -n 15`, two build jobs) and the canonical toolkit
`282467e727df78c8e7f2452aa05a7747bac854a6`. The separate renderer AO fixture
correction supplied by the material integration owner is included only in the
private regression snapshot; it is not part of the Unicode commit.

- Appearance, existing spatial renderer and Unicode GL/layout CTests: 3/3,
  12.40 seconds; the subsequent empty-input/Fontconfig-cleanup test also passes.
- ASan/UBSan/leak validation of the changed text helper, palette, test and EGL
  transport: passes, including the 256-entry malformed-byte corpus. Existing
  core/render support archives use the matching fresh Release ABI; this is
  changed-code sanitizer coverage, not a claim of whole-core instrumentation.
  The test explicitly releases Fontconfig's process-global application-font
  scans with `FcFini()` after its last lookup.
- Real native FLTK Unicode input/universal clipboard/save/undo/redo: 3/3;
  existing universal-menu GUI regression: 53/53, private Xvfb/DBus only.
- Real-loader synthetic OpenXR regression: final exact binary 96/96,
  complete with zero failures, including signal shutdown and skin authoring.
- Software-GL screenshots inspected: dark Skin page 14, 2× light Skin page,
  Unicode model/reference names and native Unicode text field. Long static
  commands wrap at word boundaries within their unchanged joystick cells.

Artifacts: `artifacts/verify-palette-unicode/` contains source/font SHA-256
manifests, exact fallback face/size lists, PPM/PNG captures and native GUI
reports. Root-level `artifacts/tests-palette-*.log` and
`artifacts/menu-palette-*.log` contain the corresponding build/test evidence.
The canonical DomeD font reference is `native/fltk_chrome.cpp` at that toolkit
commit (SHA-256 `1e83f00910342b16f9273b8944090c40dac0d10c88d4db65a6ccef1a99c06382`);
the unmodified shared BDF helper SHA-256 is
`5738a6826aea224c96c0578d7ba1e34e43f63e86ca6a675c710ced8438bfb154`.
