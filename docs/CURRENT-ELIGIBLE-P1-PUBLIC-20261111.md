# Current query integration: public workflows and original P1

Private source `e9931f72` passes all 196 current Release/sanitizer public checks
and the unchanged original full four-object P1 correctness, resource and timer
gates. Main product admission remains a separate review. The full goal remains
active and incomplete.

The P1 run completed 900 strokes, 360 masks, 6,000 stereo view pairs and 360 held
pairs over three repetitions, with 42 independent geometry/image/query/scalar
checkpoints. Every repetition passed exact Cancel, Undo/Redo, ordered Save/reopen
and stale-generation checks. All 6,000 GPU timer samples are valid, with no
pending, reset or disjoint result. Source, SDK, provider and all 70 mapped-file
hashes remain unchanged. The actual EGL device is Intel Arc A770,
PCI `0000:03:00.0`, i915, with live render-node FDs before and after.

**Performance is not accepted by these passes.** The new observed total is
300.18 s versus 273.16 s for b6. View and GPU upper-tail timings are slower:

| Rep | View p50 ms, new / b6 | View p95 ms, new / b6 | GPU p95 ms, new / b6 |
|---|---:|---:|---:|
| 1 | 9.590 / 9.081 | 11.311 / 10.592 | 12.280 / 9.259 |
| 2 | 9.595 / 9.086 | 11.303 / 10.464 | 12.620 / 9.308 |
| 3 | 9.448 / 9.094 | 10.993 / 10.553 | 12.661 / 9.184 |

These are observed offscreen, uncapped stereo measurements on a running desktop.
The current coordinated quiet window stopped competing agent jobs; it did not
control desktop/OS activity or establish equal historical conditions. GPU timers
cover the eye draws; view CPU/wall rows include actual controller ray and contact
queries. Full phase distributions, stage totals, raw rows and all timer samples
are retained. This is **not 90 Hz or physical-HMD performance acceptance**, and
no causal performance attribution follows from this comparison.

The original 4 objects, 0.0125 m voxels, 768×768 per eye and all workload arguments
are unchanged. The source, two helpers and target CMake match the prior qualified
benchmark exactly. Limits remain 5,400 s, 5 GiB process RSS, 2 GiB allocated output
and a 20 GiB free-space floor; internal 120 s publication and 20 s timer-drain
bounds are unchanged. Sampled peak RSS is 4,431,769,600 B; native `getrusage` peak
is 4,429,049,856 B, both below 5 GiB. Output occupies 169,820,160 B. No guard,
restart, source edit, reduced workload or relaxed quality threshold supplied the
pass.

Both current profiles pass source/plain normal desktop GUI10 and actual-loader
XR39 cases: eight cases, 196 checks. Ten complete canvas R/S pairs (excluding only
the original source-defined status28) and six complete native-file pairs match
exactly. Own producer, desktop Save and XR final endpoints match. Every actual
sanitizer app/producer exits cleanly with ASan leak detection 1 and strict UBSan,
without suppression. Desktop uses private Xvfb; its GLX canvas may be software
while the actual EGL blur worker authenticates Arc. XR uses the real loader and
actual Arc stereo with synthetic controls, not a physical headset.

Provider ownership is audited incremental: each profile has 65 current outputs,
63 fresh links and two byte-identical qualified consumers, using 307 authenticated
whole-object providers and a 192-member core with 190 unchanged members. It is
not a fresh whole build. Both actual current native/software gates pass 56/56,
plus app version and workspace27/373. Earlier failures remain untouched; they
are not relabelled by this milestone.

[The scoped receipt](validation/CURRENT-ELIGIBLE-P1-PUBLIC-20261111.json) records
commands, source/admission/provider hashes, devices, exact counts, all public
cases and verified lossless evidence archives. [Timing detail](validation/CURRENT-ELIGIBLE-P1-PUBLIC-TIMINGS-20261111.json)
retains the full phase distributions and actual query-stage measurements.
Original files remain in place. All jobs and Arc leases are closed. No product,
public header, default, installation or desktop/headset session is changed by
this docs-only checkpoint.
