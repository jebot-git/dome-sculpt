# Bounded current production inventory proposal

Unexecuted; requires coordinator go. Reuse only the exact newly compiled current
d1 consumer from the successful smoke gate. No new compilation, product edit,
limit relaxation, production loop or Arc context is part of this proposal.

First caseP1 is exactly:

```sh
/tmp/dome-sculpt-sustained-d1f4-krc4t7nt/dome-sculpt-sculpt-sustained \
  --output /tmp/dome-sculpt-sustained-d1f4-krc4t7nt/P1-inventory \
  --inventory-only --device software --tier production \
  --objects 1 --voxel .0125 --width 768 --height 768
```

The output directory must be new. The software environment is the same canonical
SDK and Mesa setup as the current smoke, with LIBGL_ALWAYS_SOFTWARE=1,
LP_NUM_THREADS=2, MESA_SHADER_CACHE_DISABLE=true and the Mesa EGL vendor manifest.
Actual Mesa llvmpipe/softpipe must be authenticated; environment alone is not
device evidence. No Arc lock/context is acquired.

Inventory performs the unchanged original grouped regression, one genuine current
Document cold publication/first draw, independent whole-local geometry/scalar/
full-affine ray/nearest and exact three-mode framebuffer checks, one real native
warm-up stroke plus exact Undo, and a second independent checkpoint. It then
returns before sustained strokes, masks, stereo/held loops or long history/save
controls. The requested default sustained inputs are recorded but not executed.
Success is `production-inventory-admitted` only after the actual accepted Scene
has>=250000 visible triangles and all inventory checks pass. Expected counts are
not substituted for this measured predicate. Existing4million sample,512MiB
retained/1GiB working and all query/geometry guards remain unchanged. A quota,
size or validation refusal is retained under its real diagnostic and endsP1.

Bound the process tree to3GiB RSS,180s wall time and one inventory process. Expected
planning time is5–30s, not a qualified prediction. Disable core dumps. Recheck
workspace20GiB+root192MiB+own8MiB, tmpfs2GiB+full64MiB prospective and own cumulative
96MiB cap including the successful current-smoke outputs. Current actual smoke
metadata+tmpfs totals18.89MB, so expected additional sub2MiB raw/results stays
within that cap. No large native save or extra full-frame files are expected in
inventory-only mode. Preserve the ELF/object, current282 providers, frozen tracked
d1/SDK/header/dependency/compiler inputs and runtime mappings before/during/after.

Only afterP1's actual outcome, separately propose/admit P4 (`--objects 4 --voxel
.025`) andP16 (`--objects 16 --voxel .05`) with fresh output names. They are
independent declared cases, not automatic lower-density fallback. Any subsequent
production software or Arc timing run requires its own bounded plan and go.
The coordinated hardware lock remains `/tmp/dome-sculpt-arc-device.lock`.
