# Target-colour sanitizer boundary completion

The original frozen `65b0ae9` allocation sanitizer run completed42/44 registered
shards in7629.84s. The only failures were the unchanged600s time limits for
precommit shards6/7; they reached8192 and9248 respectively without a sanitizer or
assertion failure. The original source, binary, test registrations and log remain
unchanged. This is not represented as an original44/44 pass.

A separate timeout-only driver verified the exact original sanitizer executable
SHA256 before and after execution. Its existing16-way mode completed the entire
two affected ranges in four disjoint cases, each retaining the600s budget:

| Mode | Complete unique range |
| --- | --- |
| precommit12/16 | 7117–7709 |
| precommit13/16 | 7710–8302 |
| precommit14/16 | 8303–8895 |
| precommit15/16 | 8896–9488 |

All2372 denied allocations and real complete-helper retries pass. Preparation is
not replayed. Scene/paint/current-field/basis/shape/material/source owners and
immutable input/prepared baselines retain their original assertions.

The complete unique sanitizer ledger is4290 packed-field allocations,9488
precommit allocations (original1–7116 plus the completed2372),20669 Scene20 encode
allocations and17151 Scene20 decode allocations:51598 measured boundaries. This
is the original42/44 plus four precise completion cases, with ASan/UBSan and leak
checking, not a new whole-project or latest-source build.

Only future registered precommit6/7 time budgets change from600 to1200 seconds.
No executable, range, assertion, helper or preparation policy changes; no extra
full suite was run to manufacture one green batch. All earlier functional,
renderer, native FLTK and real-loader scopes stay separately recorded. The new
raw/colourUV23 implementation is unrelated and remains private.
