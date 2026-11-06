# Direct immutable geometry upload integration

Main integrates exact qualified candidate `9f89fce` on `537c98b`.
The renderer uploads immutable position, normal, UV, tangent and source-field
arrays directly. A fixed table replaces mesh-sized interleaved CPU copies;
missing directions alias the normal stream and absent UV/tangent attributes use
constants restored on every draw. Plain geometry stores 24 bytes per vertex,
compared with the previous 56-byte interleaved layout. This is a storage and
copy reduction, not a frame-time benchmark. Full geometry uploads and derived
coordinate preparation remain; bounded local extraction/upload is further work.

The current-main composition freshly compiles the whole renderer, reference
renderer, XR and ordinary application providers for Release and sanitizer. It
uses the nine admitted current input/query providers and matching polygon,
weight and frontend support. Each profile passes 73 direct-upload controls,
the buffer/material/normal-frame/ghost/full-render consumers, and 13,442 current
native input controls. Six reference framebuffer pairs are exact per profile.
Across the component/application runs, 58 corresponding frame/native pairs are
exact. Ordinary applications pass 15 normal, 16 unzoomed compact and 27 software-loader
checks per profile; six corresponding saved-file pairs are exact.

The earlier Release component separately exercised the actual Arc A770 and
showed the old renderer failing the direct-array contract. Its 37-control
hardware scope is distinct from the newer 73-control software composition.
Software/UHD rejection belongs to the strict device witness, not product policy.
No new hardware, physical-headset, fresh whole-project or full-suite claim follows.

Root independently verified 20 produced provider/test objects, 14 executed
test logs, 58 frame/native pairs, 14 process mapping records and eight archived
sanitizer test closures, archive hashes and restore-tool hashes. These archives
retain full-stream and actual-original-path restore controls. Failed invocations
remain failed: the first manual component attempt reused a provider output name for a test;
the first CMake generation lacked two tracked source files. Distinct corrected
invocations pass, without changing the renderer product body.

The registered CMake target is audited through fresh canonical-SDK configuration;
matching whole consumers execute through the recorded manual recipes. This is
not a claim that CTest ran the newly generated build. Public layouts, scene
formats and canonical DomeD toolkit/menu/bitmap conventions remain unchanged.
No installation or session restart is performed. The complete production goal,
including polygon paint transport, texture sets, avatar/interoperability work,
remaining acceleration and controller drawers, remains active.

See [the root integration receipt](validation/RENDER-DIRECT-UPLOAD-INTEGRATION.json)
and [the original qualification](validation/RENDER-DIRECT-UPLOAD-CURRENT.json).
