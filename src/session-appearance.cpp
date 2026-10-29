// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/session-appearance.hpp"
#include "bdf_font.hpp"
#include <fstream>
#include <nlohmann/json.hpp>
namespace dome {
namespace {
std::filesystem::path path() {
  auto config = getenv("XDG_CONFIG_HOME"), home = getenv("HOME");
  return std::filesystem::path(config ? config
                               : home ? std::string(home) + "/.config"
                                      : "/tmp") /
         "dome/appearance.json";
}
nlohmann::json preferences() {
  std::ifstream file(path());
  nlohmann::json j = nlohmann::json::object();
  if (file)
    file >> j;
  if (!j.is_object())
    j = nlohmann::json::object();
  return j;
}
int integer(const nlohmann::json &j, const char *key, int fallback, int lo,
            int hi) {
  auto it = j.find(key);
  if (it == j.end() || !it->is_number_integer())
    return fallback;
  if (it->is_number_unsigned() && it->get<uint64_t>() > uint64_t(hi))
    return fallback;
  auto n = it->get<int64_t>();
  return n >= lo && n <= hi ? int(n) : fallback;
}
SessionAppearance resolve(const nlohmann::json &j) {
  SessionAppearance style;
  style.light =
      j.contains("light") && j["light"].is_boolean() && j["light"].get<bool>();
  if (j.contains("accent") && j["accent"].is_string()) {
    auto value = j["accent"].get<std::string>();
    if (value.size() == 7 && value[0] == '#' &&
        std::all_of(value.begin() + 1, value.end(),
                    [](unsigned char c) { return std::isxdigit(c); }))
      style.accent = std::stoul(value.substr(1), nullptr, 16);
  }
  style.opacity = chrome_opacity(integer(j, "opacity", 255, 0, 255));
  auto name = j.contains("font") && j["font"].is_string()
                  ? j["font"].get<std::string>()
                  : "Unifont / Sans-Serif";
  auto slash = name.find(" / ");
  auto family = name.substr(0, slash), face = slash == std::string::npos
                                                  ? std::string()
                                                  : name.substr(slash + 3);
  int pixels = integer(j, "font_size", 16, 4, 64),
      scale = integer(j, "font_scale", 1, 1, 2);
  FcInitReinitialize();
  bdf_fonts::discover(true);
  bdf_fonts::Face selected;
  try {
    selected = bdf_fonts::resolve(family, face, pixels, scale);
  } catch (const std::exception &) {
    try {
      selected = bdf_fonts::resolve("Unifont", "Sans-Serif", 16, scale);
    } catch (const std::exception &) {
      auto fonts = bdf_fonts::discover();
      if (fonts.empty())
        throw std::runtime_error("Dome Sculpt requires an installed BDF font");
      selected = bdf_fonts::scaled(fonts.front(), scale);
    }
  }
  style.file = selected.file;
  style.pixels = selected.pixels;
  return style;
}
std::string signature(const nlohmann::json &j) {
  std::string result = j.dump();
  const char *config = getenv("FONTCONFIG_FILE");
  if (config) {
    std::error_code error;
    auto time = std::filesystem::last_write_time(config, error);
    result += config;
    if (!error)
      result += std::to_string(time.time_since_epoch().count());
  }
  return result;
}
void apply(Document &document, const SessionAppearance &style) {
  document.appearance(style.file, style.pixels, style.light, style.accent,
                      style.opacity);
}
} // namespace
int chrome_opacity(int value) {
  if (value < 64 || value > 255)
    return 255;
  constexpr int presets[] = {64, 128, 191, 255};
  return *std::min_element(
      std::begin(presets), std::end(presets),
      [&](int a, int b) { return std::abs(a - value) < std::abs(b - value); });
}
SessionAppearance session_appearance() { return resolve(preferences()); }
SessionAppearanceMonitor::SessionAppearanceMonitor(Document &document) {
  auto config = bdf_fonts::export_config();
  setenv("FONTCONFIG_FILE", config.c_str(), 1);
  std::string last;
  try {
    auto j = preferences();
    apply(document, resolve(j));
    last = signature(j);
  } catch (const std::exception &) {
    apply(document, resolve(nlohmann::json::object()));
  }
  worker_ = std::jthread(
      [&document, last = std::move(last)](std::stop_token stop) mutable {
        std::mutex mutex;
        std::condition_variable_any changed;
        std::unique_lock lock(mutex);
        while (!stop.stop_requested()) {
          changed.wait_for(lock, stop, std::chrono::milliseconds(300),
                           [] { return false; });
          if (stop.stop_requested())
            break;
          try {
            auto j = preferences();
            auto current = signature(j);
            if (current != last) {
              apply(document, resolve(j));
              last = signature(j);
            }
          } catch (const std::exception
                       &) { /* Keep the last complete valid appearance. */
          }
        }
      });
}
} // namespace dome
