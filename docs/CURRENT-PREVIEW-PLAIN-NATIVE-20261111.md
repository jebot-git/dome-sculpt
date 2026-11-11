# Genuine plain preview fixture: native qualification

The separate plain producer passes 67,677 controls in Release and strict SAN on
actual Arc hardware. This closes native fixture construction before the final
owning application build. It does not qualify current-grid GPU resolve/draw or
the application desktop/XR interaction yet.

| Producer | Controls | Native seconds | Peak RSS |
|---|---:|---:|---:|
| Release |67,677|0.191|76,447,744 bytes|
| SAN |67,677|0.407|520,036,352 bytes|

Frozen test source is `b795e0e7`: the earlier fixture work merged with frozen COW
source `b6e366fd`, preserving product bytes exactly. Each entire producer TU is
freshly compiled and linked against its audited current-TextureMaps-ABI b6 thin
core and matching current f473 dependent libraries. The core qualification
reserves 191 exact existing objects plus the complete changed COW TU. There is no
775/27f core reuse, whole fresh core/application claim, or private product edit.
All build and run input/provider/output drift lists are empty.

The input is an actual plain strictUV64 PaintedMesh, with genuine nonuniform RGB,
authored alpha distinct from coverage, metallic/roughness, independent mask,
mixed legacy/physical emission, measured subUNORM8 FLOAT HDR coverage and a hidden
zero-opacity layer. It does not fabricate imported or projected material/rig
authority to pass eligibility. The original source-backed producer body/options
and original native preservation checks remain unchanged and separate.

Offline native checks authenticate the actual Document's current-grid FLOAT
render packet, exact actor/material/ordered UVs and absent CPU image payloads
before Blur, after acceptance, after Undo/Redo, and after reopening both complete
native endpoints. The public default Flow1 Blur command executes on hardware;
the original independent per-texel kernel oracle and unchanged typed/hidden/mask/
source-owner preservation checks pass. Ordered Save and one exact whole-scene
Undo/Redo/reopen pass. These native packet assertions do not inspect or authorize
progress in a running GUI/XR app.

Both original native files match byte-for-byte across profiles, as do both
accepted files. The accepted SHA-256 is
`4ee109172943e381108637404c16558b0d49bd1569e70fb542d55fa98bc1bb1a`.
Each manifest records 67,676 checks before its final checked write; the final
native stdout count is 67,677. No oracle count or deadline was reduced.

The actual current EGL display/context and worker thread query authenticate Arc
A770, renderD129, PCI `0000:03:00.0`, `8086:56a0`, i915 and current mapped
consumer/observer/EGL/Gallium ELFs. The existing external query observer returns
the unchanged GL result. At the real current context it fstats live descriptors,
requires selected-node character-device rdev matches, logs their symlink targets,
and records context maps. Both profiles record live FDs3–6 on renderD129 through
that direct context witness.

Release's independent external /proc sampler also captures those descriptors.
SAN's sampler records an empty startup sample and misses the short context while
hashing the 451 MB ELF. Its in-context fstat/symlink and context-map evidence still
authenticates the live descriptors; an external SAN descriptor observation or
global absence of other nodes is not claimed. The distinct witnesses are retained
explicitly without rerunning or changing native work.

SAN uses leak detection1 and strict UBSan without suppressions, exits0 and reports
no sanitizer error. The external producer bounds remain 180 seconds and 4 GiB;
original native worker deadlines stay unchanged. Both short runs hold and release
the common Arc lock. No producer/compiler job or device lease remains.

Raw build/run/device/native evidence is in
`artifacts/current-preview-public-proof-20261111`; the companion JSON pins exact
recipes, providers, ELFs, source, native files, context maps and sampling limits.
The earlier source-only plan remains a historical planning record. Final recovered
owning app/inspector/font/runtime builds and actual source-plus-plain public
desktop/XR scenarios are still required. No such app run, installation or physical
headset test is performed here.
