# Independent layered-normal glTF export

Eight real saved native states now export through the actual Document worker:
active conversion, original rest/curved skin/morph data, partial rebake,
clear-then-rebake, cleared detail, explicit old31 upgrade and old31 Clear.
Three skin poses and three morph weights give72 views per profile. An
independent glTF/PNG/PBR/skin/morph consumer matches every actual native image
with maximum channel error **zero**, under the unchanged bound4.

Release and ASan/UBSan each pass160 assertions. Actual strict Intel Arc A770
passes161, including observed device identity; forced llvmpipe refuses the
hardware gate. These are graphics/worker/file checks, not compute acceleration
or physical-headset evidence. Native second-eye drawing adds no texture
uploads or source-pack builds. Active images require the actual normal texture;
cleared cases have no normal texture. Editable GLB extraction recovers the
complete Scene33 source/material/field history and current pose exactly.

All72 emitted file triples are byte-identical across Release, sanitizer and
Arc. The pinned Khronos2.0.0-dev.3.10 validator checks216 actual emitted GLBs
with zero errors,108 warnings and297 informational messages. Every issue is
retained with `maxIssues:0`, and none is truncated. These are newly validated
files, not reports reused from earlier bytes.

The new consumer is a complete test translation unit over the exact matching
fresh1ce plus qualified bake-guard core/render/support cohort. Product/layout
bytes do not change in this export checkpoint. The CMake graph configures624
tests with the native fixture preceding the independent export consumer; this
is configure-only, not a complete624-test invocation or a new ALL build.
Read [the receipt](validation/LAYERED-NORMAL-INDEPENDENT-EXPORT.json) for actual
commands, hashes, logs, all validator reports and72 exact file triples.

This private milestone extends the native runtime/codec proof without claiming
main admission. Full owning archive/caller fault coverage, broad typed/source/
corner/UV/key/current-captured-frame adapters, new actual FLTK/Wayland/OpenXR
controls and coherent agent/main qualification remain. The entire original
production goal and later controller drawers remain required. No system or
physical-headset/session action occurred.
