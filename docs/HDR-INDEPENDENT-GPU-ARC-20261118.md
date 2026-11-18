# Independent HDR source GPU on Intel Arc — 2026-11-18

The independent retained-HDR GPU component is now qualified on actual Intel
Arc A770. Production is `c52418bf`, fixture `2122416b`, exact unchanged ELF
`ccd275252164098501c44b19c1c573090023fa2356b3172843b9585689d026f5`.
The software component proof remains separate. This does not qualify the newly
combined application, public UI or physical headset, or complete the full goal.

## Actual retained64/current512 result

The actual product runs with `--hardware` and passes all 4,422 checks. Original
64-resolution source FLOAT coefficients survive GPU extraction bit-exactly:
maximum coefficient error is 0, and all 4,096 full-coverage alpha values remain
1. The editable paint grid remains 512. The MToon eighth packed view retains
64x64 dimensions using its existing sampler unit.

Four independent material phases compare the real renderer with an independent
original sparse-source/current-layer equation at 25 points per phase. Errors
for unedited PBR, unedited MToon, edited PBR and edited MToon are respectively
1, 0, 1, 0 under unchanged RGB bound 2. The real current512 physical layer,
opacity and existing mask coefficients, retained source background, deliberately changed target
material and typed MToon strength remain genuine inputs. No source resampling,
material substitution or threshold relaxation is used.

## Actual context and device evidence

A small separate observation library forwards each original `glReadPixels`
call unchanged, then queries current EGL context/display and GL vendor/renderer.
It does not select/create a context, change GL state, or modify readback data.
All 105 readbacks report the same non-null EGL context and display, vendor Intel
and `Mesa Intel(R) Arc(tm) A770 Graphics (DG2)`. The first is the real64x64 FLOAT
coefficient read. The remaining 104 are96x96 unsigned-byte material reads: four
actual material draws and100 independent reference draws. Phase cumulative
counts are27/53/79/105. No unrelated normal-matrix136-boundary requirement is
borrowed for this distinct fixture.

The separately pinned device-auth observer identifies PCI03:00.0,
vendor8086/device56a0, renderD129 and kernel i915. A concurrent sampler records
2,699 live observations, the actual renderD129 FD/sysfs identity and72 pinned
mapped ELFs, including the real Mesa driver, both observers and canonical bitmap
providers. Unknown DRM devices, unbound ELF/deleted mappings and software
fallback are refused. This is hardware execution, not a renderer-string guess.

The context observer has one freshly compiled optimized complete TU, its real
133 compiler-MD dependencies, a retained owned object and one fresh shared link
with20 actual linker inputs. The exact source/build commands and hashes are
retained. The product and its owning renderer/core objects are unchanged.

## Runtime, closure and preserved observer failure

Actual unit `ds-source-hdr-gpu-arc-v2-20261118.service`, Invocation
`b38c88e70ab1441780dfcd7f6846e86d`, completes0. The product takes4.595s with peak
owned RSS641,699,840 bytes under unchanged180s/4GiB bounds. The regular-user
unit retains MemoryMax4GiB/RuntimeMax300s; no address-space cap is added.

The first actual product also completed0. Its driver then failed because a
mapped filename shadowed the stage identifier used to read the log. Original
wrapper1/product0, trace, maps, device records and terminal capture remain
unchanged. A separate corrected driver uses distinct stage/path variables and
reruns the exact same product/observers with all numerical and context/device
predicates unchanged. The complete second wrapper and product both exit0.

Both units have terminal properties and journals captured before stopping.
Final drift and survivors are empty. An independent nonblocking check proves
the common lease free at1794985386.372694, device40/inode99618, before handing
to the queued creator UI run. No install, headset launch, idle lease reservation
or active-session restart occurs.

The receipt and34-member scoped archive in
`validation/hdr-independent-gpu-arc-20261118/` retain exact plans/drivers,
observer source/actual dependencies, both actual runs and failures, context/FD/
mapping records, software prerequisites and terminal/lease closure. No product,
object or observer-ELF payload is included. Combined application/UI/headset
qualification, new-domain GPU resource/fault stress and broader correspondence
support remain required.
