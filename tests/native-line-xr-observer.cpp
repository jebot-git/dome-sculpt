// SPDX-License-Identifier: GPL-3.0-or-later
// Actual CLI App, unchanged entrypoint. Only journals the actual native helper
// request/result; no replacement result, altered cancellation, or scene hook.
#define main native_line_actual_application_main
#include "../src/app.cpp"
#undef main
using Cancel = std::function<bool()>;
extern "C" bool
real_native(dome::Scene &, std::span<const uint64_t>,
            const dome::SkinAuthoringOptions &, const Cancel &,
            dome::NativeVisibleSkinReport
                *) asm("__real__ZN4dome26author_native_visible_skinERNS_"
                       "5SceneESt4spanIKmLm18446744073709551615EERKNS_"
                       "20SkinAuthoringOptionsERKSt8functionIFbvEEPNS_"
                       "23NativeVisibleSkinReportE");
extern "C" bool
wrapped_native(dome::Scene &, std::span<const uint64_t>,
               const dome::SkinAuthoringOptions &, const Cancel &,
               dome::NativeVisibleSkinReport
                   *) asm("__wrap__ZN4dome26author_native_visible_skinERNS_"
                          "5SceneESt4spanIKmLm18446744073709551615EERKNS_"
                          "20SkinAuthoringOptionsERKSt8functionIFbvEEPNS_"
                          "23NativeVisibleSkinReportE");
extern "C" bool wrapped_native(dome::Scene &s, std::span<const uint64_t> ids,
                               const dome::SkinAuthoringOptions &o,
                               const Cancel &cancel,
                               dome::NativeVisibleSkinReport *report) {
  dome::NativeVisibleSkinReport measured;
  const bool changed =
      real_native(s, ids, o, cancel, report ? report : &measured);
  const auto &r = report ? *report : measured;
  if (auto path = std::getenv("DOME_NATIVE_LINE_OBSERVER"))
    std::ofstream(path)
        << nlohmann::json{{"mode", int(o.mode)},
                          {"contact", int(o.native_visible.contact_handling)},
                          {"changed", changed},
                          {"work", r.work},
                          {"rows", r.rows},
                          {"measure_zero_origins", r.measure_zero_origins}}
               .dump(2);
  return changed;
}
int main(int argc, char **argv) {
  return native_line_actual_application_main(argc, argv);
}
