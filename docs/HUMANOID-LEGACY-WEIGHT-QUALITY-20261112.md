# Independent legacy weight-quality measurement

This read-only audit measures the standalone legacy `automatic_bind` function
against the original CC0 VRoid artist weights. It does not measure the UI default,
Surface Heat or Anatomical source-binding methods. The native desktop explicitly
labels the old method “Legacy automatic bind”. Artist weights are independent
reference observations, never inputs to fitting or binding.

The frozen cc5e6aed freshly compiled owning core193 supplies the actual current
fitting and legacy binder. A separate fresh native measurement TU reads every
original POSITION/NORMAL array and semantic joint head. References use exact
skin joint/node/declared-role associations; no name or nearest-joint guessing
is used to fold unmapped controls into humanoid roles. Only referenced vertices
without positive unmapped influences enter the agreement metrics.

| CC0 base | Referenced vertices | Excluded for unmapped influences | Compared vertices | Dominant-joint agreement | Mean shared reference/predicted mass |
| --- | ---: | ---: | ---: | ---: | ---: |
| Female | 9851 | 1110 | 8741 | 56.82% | 48.97% |
| Male | 9878 | 0 | 9878 | 54.81% | 46.67% |

The lowest agreement includes head/upper chest and hands. For head-dominant
reference vertices, mean shared mass is23.2% female and24.1% male. This is an
indication to inspect support/deformation, not proof that all artist weights
are the only correct result. Both complete original position arrays (>90000
each) are consumed; unused accessor vertices are not silently removed from
fitting/binding, while comparison uses only visible indexed geometry.

Actual compile, link and native measurement exit0, with zero pinned-input drift.
This is successful measurement execution, not a weight-quality pass. The full
role breakdown, source/core hashes, logs and exclusions are in
[the receipt](validation/HUMANOID-LEGACY-WEIGHT-QUALITY-20261112.json).
Measurement source is on `work/humanoid-weight-quality-audit-20261112` at6a5f6386;
no product solver, default, schema, asset or frontend changes are admitted.

Next compare the preferred Surface Heat and Anatomical workflows on genuine
complete retained unskinned source groups. Use actual source vertex provenance
to associate their outputs to the original reference, and retain hidden parts,
source/normal/pose authority and all refusal/cancellation/resource contracts.
Native and independent bend/silhouette/normal checks are needed alongside the
numbers; the broader robust automatic-weight requirement remains incomplete.
