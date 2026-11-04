// SPDX-License-Identifier: GPL-3.0-or-later
// Actual native App; test-only observation and a gate inside real preparation.
#define main dome_application_main
#include "../src/app.cpp"
#undef main
#include <fstream>
#include <thread>
namespace {
using Cancel = std::function<bool()>;
constexpr const char *gate_env = "DOME_NATIVE_WEIGHT_GATE";
} // namespace
extern "C" bool
real_native(dome::Scene &, std::span<const uint64_t>,
            const dome::SkinAuthoringOptions &, const Cancel &,
            dome::NativeVisibleSkinReport
                *) asm("__real__ZN4dome26author_native_visible_skinERNS_"
                       "5SceneESt4spanIKmLm18446744073709551615EERKNS_"
                       "20SkinAuthoringOptionsERKSt8functionIFbvEEPNS_"
                       "23NativeVisibleSkinReportE");
extern "C" bool wrapped_native(
    dome::Scene &scene, std::span<const uint64_t> targets,
    const dome::SkinAuthoringOptions &options, const Cancel &cancelled,
    dome::NativeVisibleSkinReport
        *report) asm("__wrap__ZN4dome26author_native_visible_skinERNS_"
                     "5SceneESt4spanIKmLm18446744073709551615EERKNS_"
                     "20SkinAuthoringOptionsERKSt8functionIFbvEEPNS_"
                     "23NativeVisibleSkinReportE");
extern "C" bool wrapped_native(dome::Scene &scene,
                               std::span<const uint64_t> targets,
                               const dome::SkinAuthoringOptions &options,
                               const Cancel &cancelled,
                               dome::NativeVisibleSkinReport *report) {
  const char *value = std::getenv(gate_env);
  const std::string gate = value ? value : "";
  size_t calls = 0;
  bool entered = false;
  auto check = [&] {
    ++calls;
    if (!entered && calls == 64 && !gate.empty() &&
        std::filesystem::exists(gate + ".arm")) {
      entered = true;
      std::ofstream(gate + ".entered") << calls << '\n';
      const auto deadline =
          std::chrono::steady_clock::now() + std::chrono::seconds(90);
      while (!std::filesystem::exists(gate + ".release")) {
        if (cancelled && cancelled()) {
          std::ofstream(gate + ".cancelled") << calls << '\n';
          return true;
        }
        if (std::chrono::steady_clock::now() > deadline)
          throw std::runtime_error(
              "Test-only native preparation gate timed out");
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
      }
    }
    return cancelled && cancelled();
  };
  return real_native(scene, targets, options, check, report);
}
struct NativeObserver {
  App &app;
  std::string path;
  static void tick(void *p) {
    auto &self = *static_cast<NativeObserver *>(p);
    const auto state = self.app.canvas.document.frame().state;
    std::ofstream(self.path + ".ui.json") << nlohmann::json{
        {"saving", bool(self.app.saving)},
        {"queued_commands", self.app.native_commands.size()},
        {"busy", state->asset_busy},
        {"can_save", state->can_save_after_asset},
        {"can_cancel", state->can_cancel_asset},
        {"error", state->error},
        {"focused_label",
         Fl::focus() && Fl::focus()->label() ? Fl::focus()->label() : ""},
        {"native_support_context",
         int(state->skin_options.native_visible.support_context)},
        {"native_contact_policy",
         int(state->skin_options.native_visible
                 .contact_handling)}}.dump();
    Fl::repeat_timeout(.02, tick, p);
  }
  NativeObserver(App &a, std::string p) : app(a), path(std::move(p)) {
    Fl::add_timeout(.02, tick, this);
  }
  ~NativeObserver() { Fl::remove_timeout(tick, this); }
};
int main(int argc, char **argv) try {
  if (argc != 2 || !std::getenv(gate_env))
    throw std::runtime_error("Choose native project and a scoped worker gate");
  desktop::NativeFontconfigRuntime fontconfig;
  desktop::NativeDisplayRuntime display;
  Fl_Window::default_xclass("org.domed.Sculpt");
  dx::init();
  App app;
  app.canvas.document.open_async(argv[1]);
  app.update_status();
  NativeObserver observer(app, std::getenv(gate_env));
  app.window.show();
  return Fl::run();
} catch (const std::exception &error) {
  std::cerr << error.what() << '\n';
  return 1;
}
