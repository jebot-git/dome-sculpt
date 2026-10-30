# Real-loader affine contact qualification

Private product c638fee now has real OpenXR loader/stereo GL qualification with
synthetic head/controller tracking. The test-only runtime drives grip contact
on a reflected/sheared Scene13 source, then separately actual focus loss/regain.
Accepted contact paints original UV center; only painted state changes. Focus
cancellation preserves the complete original Scene. Both eyes contain actual
nonblank GL images with balanced acquire/wait/release counts.

Ten assertions pass on llvmpipe and ten on actual Intel Arc A770. The fixture
captures GL_VENDOR and GL_RENDERER from the active swapchain context and rejects
a mismatched requested device. On this host DRI_PRIME=0 selected A770 atPCI03:00.0;
that ordinal is host-specific. The initial DRI_PRIME=1 attempt selected UHD770
and failed the strict device assertion; its log is retained separately. The
stable PCI selector is preferable for later host reruns. No product graphics
selector or active desktop session was changed.

The actual optimized frontend was built coherently from c638fee. Test-only
helper/runtime replacement compilation used matching public and JSON headers,
exact CMake flags/support archives and recorded link maps; archive hashes are
unchanged. This adds the ordinary affine-contact-openxr CTest registration for
fresh complete parent builds. Product core/source files are unchanged.

This is no physical-headset or pacing proof. Contact on static placed geometry
is the graphical scope; current-pose world metric and moving-held endpoint are
separately qualified native/controller and whole-core sanitizer fixtures atc638.
General affine import/source hierarchy, remaining metric authoring and prepared
normal interpretation remain active required work. No push or system install.
