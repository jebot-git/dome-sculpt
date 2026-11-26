# Combined9e4 clean native Wayland validation — 2026-11-26

The exact combined `9e4f79f6c37ac4c4ca52cc6a0c0f288b60454d55` application now passes **all 14 native Wayland GUI assertions and the complete wrapper**, with clean private-worker teardown. This closes the cleanup issue preserved in [the original functional observation](COMBINED9E4-NATIVE-WAYLAND-FUNCTIONAL-20261126.md); the original failure remains unchanged.

The actual run takes 45.465 seconds with 670,158,848 peak owned RSS bytes, within the unchanged 360-second/6 GiB bounds. Inputs, observer and production code stay unchanged. No signals or owned survivors remain. The explicit nonblocking device lease is taken before loader/product execution and released naturally; terminal and journal records are captured.

The cleanup fix keeps the private temporary root alive until the compositor and private server processes terminate and are waited for. All 39 assertion call ASTs across the scenario's available flows remain identical, including the ordinary 14-check flow. The succeeding run therefore verifies the actual menu/modal/text/history/desktop-side behavior and shutdown using the original ELF provider checks, without adding a deleted-file exception.

Live observations identify the application Arc A770 GL/EGL context and actual rebuilt canonical DomeD `acc2b70` Vulkan/Zink composition, PCI `0000:03:00.0`, renderD129/i915 and open device descriptors. There are 96 independently certified App ELF mappings and 95 compositor mappings. Exact App SHA-256 is `2959bb7f0c87c733edffddca5e6042b6c0a245b22124a345fd3eba229902367d`; compositor SHA-256 is `47920142c8c803dcf5adb63ba643d2e347fbdcd1e15ecd8bce7c1a8dd47c0b7e`. [The compositor build proof](COMBINED5604-NATIVE-WAYLAND-20261126.md) records its actual canonical optimized rebuild.

The [receipt](validation/COMBINED9E4-NATIVE-WAYLAND-CLEAN-20261126.json) and [archive](validation/COMBINED9E4-NATIVE-WAYLAND-CLEAN-20261126-evidence.tar.xz) retain 63 verified files, 702,728 bytes, SHA-256 `21d9973c5cd440eb0f996fdde0f0a901100770354abf59e11b38e5bccc406658`.

This remains a nested diagnostic compositor test with a presentation copy. It does not establish native DRM scanout, physical-headset acceptance or the full creator/production goal. No system installation or active user session changes.
