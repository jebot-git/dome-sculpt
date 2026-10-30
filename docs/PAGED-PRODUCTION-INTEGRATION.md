# Paged storage with native resolution, plane cuts and DomeD

The published production milestone merges paging `a135f48` with product source
`803479d`, preserving both histories, native resolution/Volume4, plane cuts,
six-worker shutdown and the original-bind VRM expression fix. Published
resolution checkpoint `27b14d9` is retained in the ancestry. Product C++ and
headers have not changed since merge `ee6f539`; later changes are test fixtures
and documentation. The complete original authoring goal remains unfinished.

The exact uniform `.75` and difference-form affine mask interpolation are
retained; only map lookups change to paged `get`. Physical band, flat Volume4
versus Scene4 discrimination, source leases, quotas and bounded weak-root dirty
ancestry remain intact. Cut caps retain their unique GPU geometry identity.
No background desktop queue, new picking acceleration or global-full-mask
brush fast path is included here.

## Verification

All builds are regular-user optimized Release builds with the current canonical
DomeD `282467e` FLTK 1.4.5 toolkit, explicitly selected through its `share/fltk`
config. Runtime verification prepends `artifacts/toolkit-282467e/lib64`;
inherited same-SONAME libraries from `/opt` are not the verified runtime.
The actual DomeD tests use the frozen `artifacts/domed-282467e-target` binary,
private buses/displays/homes, software Vulkan and native Wayland clients.
No active desktop session was restarted and no system installation was changed.

* Complete native CPU/UI/render scope: **91/91**, 171.60 s.
* Real-loader CPU synthetic XR: **150/150** plus resolution **17/17**. The final
  explicitly status-recorded CTest invocation exits **0**, 227.62 s.
* Complete OneAPI FLTK/OpenXR build and **27/27** selected actual Arc suites,
  16.49 s. These include the previous 25 suites plus paged storage and sustained
  Document replay. The compute oracle identifies the Arc A770 and passes 184
  independent endpoints, 224 reference batches/445,088 samples, maximum error
  `8.9407e-08`; missing GPU selection fails explicitly.
* OneAPI real-loader synthetic XR: **150/150** plus resolution **17/17**;
  CTest exits **0**, 258.40 s.
* Actual Arc GL assertions: Document buffers **22**, buffer reuse **24**,
  MToon/PBR **81**, masks **38**, visibility/picking **25**. Every fixture checks
  hardware and reports Mesa Intel Arc A770/DG2.
* Native GUI: **151** assertions in 12 successful independent flows: general
  menu 53, masks 17/compact 5, resolution 12/compact 4, cuts 12/compact 6, skin 11,
  Unicode 4, actual DomeD Wayland masks 9/resolution 10/general 8. Compact
  resolution and cut screenshots were visually inspected; fixed footer actions
  remain visible and the large BDF controls scroll correctly.
* Fresh whole-core C/C++ ASan/UBSan/leak run: **91/91**, 674.80 s, explicitly
  recorded exit **0** at `7c95227`. C MikkTSpace and C++ xatlas/core/render were
  rebuilt with the same instrumentation. The later fixture-only private-font
  configuration correction passes the actual render fixture separately in
  native Release (26.06 s) and ASan/UBSan/leaks (71.31 s).

CPU combined replay retains all **25 exact numeric-state hashes** from the
frozen pre-page `2c160cf` baseline. See PAGED-SCULPT-STORAGE.md for the separate
three-trial performance comparison, resource limits and storage API contract.
Suite wall times above are CI evidence, not frame-pacing or device-performance
measurements. XR tracking is synthetic; no physical HMD validation is claimed.

## Recorded validation followups

The first CPU XR shell wrapper reported status 143 even though both CTest cases
and all 167 assertions completed successfully. No benign trailing command or
matching signal source was established. Its logs and project/report directories
are preserved with the `wrapper143` suffix. A new focused invocation used a
separate subprocess session and wrote an explicit return-code record; it passed
and exited 0. The initial observation is retained rather than relabeled.

The first whole UI/render sanitizer scope passed 89/91 and exposed missing
Fontconfig runtime cleanup in two previously uncovered standalone fixtures.
Minimal reproductions show that global initialization without finalization
retains 29,330 bytes/414 allocations; balanced initialization/finalization and
an explicitly owned configuration both exit leak-free. Inspection of the pinned
[Fontconfig 2.17.0 configuration implementation](https://github.com/fontconfig/fontconfig/blob/2.17.0/src/fccfg.c)
and [initialization implementation](https://github.com/fontconfig/fontconfig/blob/2.17.0/src/fcinit.c)
confirmed that the BDF helper borrows the current config and its Fontconfig
calls balance their temporary references. No production helper or canonical
mirror modification was needed.

Test-only `ce03b35` gives those fixtures a scoped `FcInit`/`FcFini` owner, created
after private environment setup and destroyed after all monitor threads,
Documents, GL and FreeType consumers, including unwinding. The two exact failing
fixtures then pass sanitizers (49.51 s), followed by the clean complete 91-suite
run. This uses no leak suppression or assertion relaxation.

The export helper itself performs Fontconfig discovery. Consequently its
runtime must exist before export; changing `FONTCONFIG_FILE` afterward does not
reload an already initialized configuration. An actual-helper reproducer proves
the exported file is absent before and after `setenv`, then present after
reinitialization. Test-only `fcc1739` supplies a private font cache, reloads the
exported configuration, verifies its identity and frees the enumeration list.
The original pixel/BDF/retopo/bake assertions still pass. Initial failures,
minimal sources, binaries and both focused/full results remain in the artifact
manifest; the later one-fixture followup is distinguished from the full run.

All logs, source/artifact hashes, runtime configuration, GUI reports and exact
scope are in `artifacts/paged-production-manifest-<checkpoint>.json`. Artifacts
are local and ignored; a fresh clone does not contain them. The parent publication preserves these exact proof scopes. The child performed
no push, install or HMD launch.
