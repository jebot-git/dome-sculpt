# Current cold-build fixture corrections

Fixture commit `8b5dad2e` is based on exact production revision `e375b31d` and
changes five test files only. Production ownership, rendering and input routing
remain untouched. The original `c26a8573` negative output and executable hashes
are preserved in the receipt.

The authenticated local-surface fixture accepts a valid initial local/full
combination totaling two prepared objects. An initial empty-owner ancestor can
produce one local update and one full rebuild; the independent whole-scene
corner/normal oracle already proves both correct. All later locality, allocation
failure, immutable-owner and exact geometry assertions remain, and the held
snapshot's actual initial counters must remain unchanged.

Selection and isolation verify that ordinary painted editing owns a usable
material-aware native-ray picker without entering selection/object-grab mode.
They test an actual ray hit rather than expecting the picker to be absent. The
existing transparency, proximity, hierarchy, ghost authority and allocation
failure checks remain.

The protected cut fixture checks all original `.75` mask values through their
actual chunk scalar/geometry owners. It separately checks the genuine packed
cap's ordinary unmasked policy, without inventing a flat source range or
requiring an unnecessary scalar vector for an ordinary cap. Accepted outputs
must hide the protected source and retain no stale source overlay. The modal
selection fixture obtains a real placed draw corner from the owning chunk runs;
all five modal shutdown reasons still require held-trigger suppression, a real
release before editing rearms, and one exact Undo.

Five complete fixture translation units and links use the parent's qualified
cold-build core, SHA-256
`2c0e4f40211e17c6ef57c92b54c24db47d38593385e0f6266995b149dbdf911f`.
Focused Release CPU results all pass:

| Case | Checks | Seconds |
| --- | ---: | ---: |
| authenticated-local-surface | 96 | 1.126 |
| object-selection | 167 | 0.080 |
| object-isolation | 62 | 0.045 |
| cut-authoring | 69 | 1.527 |
| modal-selection-controls | 40 | 1.370 |

These are 434 checks under default allocator settings, private `/dev` without
DRI nodes, 120-second/4-GiB per-case limits and no address-space cap. Inputs
remain unchanged and no cleanup signals are needed. The completed transient
unit auto-unloaded; its later stop exit 5 is retained without rerunning tests.
This does not claim sanitizer, hardware/graphical or whole-umbrella acceptance.

The [record](validation/sculpt-cold-fixtures-20261126.json) indexes the
[evidence archive](validation/sculpt-cold-fixtures-20261126.tar.xz): 56 members,
93,820 bytes, SHA-256
`61e11a146f0c8095ed51527cdc0159af88dd0593a8ecae2b0db5ab35d6e36836`.
All archived members and local authorities were independently rehashed, and the
captured negative log prefix still matches the original append-only umbrella.
