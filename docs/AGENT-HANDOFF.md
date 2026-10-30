# Cut and sculpt protection integration — broad verification in progress

Current branch work/cut-sculpt-integration merges55dafa0 / source2ec8a5a with
main9a0cfc4 / source20c132d. Both histories and exact -55dafa0 / -9a0cfc4
handoff/validation archives are preserved. Read HANDOFF.md and REQUIREMENTS.md.
The full objective remains active and incomplete. No agents were spawned.

The fresh Release application is build-cut-sculpt with canonical toolkit prefix
artifacts/toolkit-authoring/lib. Earlier build/ and build-cut-integration binaries
are older sources. GUI scripts were copied unchanged into artifacts/cut-sculpt-ui
with its build symlink pointing at build-cut-sculpt. Current product source is
frozen while broad validation runs; inspect actual handles before restarting jobs.

Resolved integration retains Masks15, Skin14, Retopo16 and Cut17 and both actual
joystick/ray control paths. Cut previews append ordinary cap mask sentinels while
preserving original protection feedback. Mask controls, bulk work and strokes
cannot mutate a cut source. Cancel/Apply/Undo preserve Volume3 protection/mirrors;
source feedback is not baked into new shell paint. Geometry eligibility and bake
source choices exclude volumes with only mask storage and no sculpt samples.
Six-worker predicate-mutex shutdown, accepted cancellation/retry and final-save
barriers remain intact. Keep SourceUV normal-frame guards and complete Scene8 /
legacy codecs; private local-mesh/frame stashes are not part of this merge.

The expanded cut fixture exposed real constant-mask interpolation drift. Sampling
now uses a validated uniform-field shortcut plus nested difference interpolation,
which preserves constant sparse cells exactly. The original exact assertions were
not loosened; fractional and independent affine-field cases were added. Eight
focused Release suites pass2.62s. The first diagnostic failure and actual scalar
bounds are retained in artifacts/tests-cut-sculpt-first.log / cut-mask-probe.log.

Full non-XR Release, whole-core sanitizer build/run, all native GUI flows and the
combined real-loader XR script still require their terminal final results. Old
75/140 or69/127 milestone counts do not validate this combined tree. No system
install/session restart/physical XR or new compute proof is claimed. Preserve the
full scope, record exact evidence, then continue group/multi-material repeated
cuts and all remaining production requirements. Fetch origin before publishing;
never force-push newer remote work or overwrite unrelated local changes.
