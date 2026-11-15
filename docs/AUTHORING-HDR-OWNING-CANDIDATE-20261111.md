# Combined authoring and HDR Bake owning candidate

This private integration composes main `6e431f9d`, the independently reviewed
empty-flat renderer fix `82257f3b`, radiance-preserving Bake product `2238315b`
(with Release receipt `5bb34971`), and test-only public workflow prototype
`305d2cb7`. It preserves their commits and the complete original objective.

The component tests do not qualify this combined application. Public paint and
scene layouts changed, so every owning project object must be compiled against
this combined source. No old core, renderer, application, helper DSO or test
object may be borrowed across that ABI. The latest verified DomeD toolkit is
`acc2b70a`; runtime UI remains native FLTK/OpenXR with its bitmap/menu conventions.

The next gate is an optimized Release build from zero project objects, including
all configured targets and current desktop/OpenXR owning consumers, followed by
selected original and new native/software controls. Native workflow fixture
prototypes have not yet been compiled or run in this composition. Actual public
Bake desktop/real-loader XR, fresh combined sanitizer, hardware, interoperability
and sustained production gates remain separate. Preserve failures and original
limits. A successful build alone does not qualify the combined runtime.

The original e993 full workload passes correctness/resource/timer checks but
has slower observed timing tails than b6. No 90 Hz or physical-headset performance
acceptance follows. The empty-flat patch removes unnecessary GL work; no measured
frame-rate improvement is claimed. HDR typed-source, frame, normal, native-paint,
consumer and broader authoring requirements remain open. Controller trays and
spatial node-based editing/history remain required after basic targets.

No main admission, installation, headset launch or session change is made by
this private composition or its build.
