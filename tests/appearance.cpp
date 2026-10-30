// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/session-appearance.hpp"
#include "fontconfig-runtime.hpp"
#include <ft2build.h>
#include <nlohmann/json.hpp>
#include FT_FREETYPE_H
#include FT_FONT_FORMATS_H
#include <fstream>
#include <iostream>
#include <unistd.h>
using namespace dome;
int main() try {
  auto require = [](bool v, const char *s) {
    if (!v)
      throw std::runtime_error(s);
    std::cout << "PASS " << s << '\n';
  };
  auto root = std::filesystem::temp_directory_path() /
              ("dome-style-" + std::to_string(getpid()));
  std::filesystem::create_directories(root / "dome");
  std::filesystem::permissions(root, std::filesystem::perms::owner_all);
  setenv("HOME", root.c_str(), 1);
  setenv("XDG_CONFIG_HOME", root.c_str(), 1);
  setenv("XDG_CACHE_HOME", (root / "cache").c_str(), 1);
  setenv("XDG_DATA_HOME", (root / "data").c_str(), 1);
  test::FontconfigRuntime fontconfig;
  auto write = [&](nlohmann::json j) {
    auto tmp = root / "style.tmp";
    {
      std::ofstream f(tmp);
      f << j.dump();
    }
    std::filesystem::rename(tmp, root / "dome/appearance.json");
  };
  write({{"light", true},
         {"accent", "#61cfa5"},
         {"font", "Unifont / Sans-Serif"},
         {"font_size", 16},
         {"font_scale", 2},
         {"opacity", 64}});
  Document doc;
  {
    SessionAppearanceMonitor monitor(doc);
    auto style = doc.frame().state;
    require(style->light && style->accent == 0x61cfa5 && style->opacity == 64 &&
                style->font_pixels == 32,
            "Standalone XR follows DomeD light theme, accent, 25% chrome and "
            "2x BDF settings");
    FT_Library library{};
    FT_Face face{};
    require(!FT_Init_FreeType(&library) &&
                !FT_New_Face(library, style->font_file.c_str(), 0, &face),
            "The selected session face loads directly in FreeType");
    require(
        std::string(FT_Get_Font_Format(face)) == "BDF" &&
            face->num_fixed_sizes > 0,
        "Session text uses a native BDF strike without an outline fallback");
    FT_Done_Face(face);
    FT_Done_FreeType(library);
    write({{"light", false},
           {"accent", "#ff8000"},
           {"font", "Unifont / Sans-Serif"},
           {"font_size", 16},
           {"font_scale", 1},
           {"opacity", 208}});
    auto until = std::chrono::steady_clock::now() + std::chrono::seconds(4);
    while (doc.frame().state->light && std::chrono::steady_clock::now() < until)
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    style = doc.frame().state;
    require(!style->light && style->accent == 0xff8000 &&
                style->opacity == 191 && style->font_pixels == 16,
            "Appearance changes propagate live and legacy alpha snaps to "
            "DomeD's 75% preset");
    {
      std::ofstream f(root / "dome/appearance.json");
      f << "{";
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    require(
        doc.frame().state->font_pixels == 16 &&
            doc.frame().state->accent == 0xff8000,
        "An incomplete settings write retains the last valid session style");
  }
  require(chrome_opacity(64) == 64 && chrome_opacity(128) == 128 &&
              chrome_opacity(191) == 191 && chrome_opacity(255) == 255 &&
              chrome_opacity(248) == 255,
          "All four chrome-opacity presets preserve their exact alpha values");
  std::filesystem::remove_all(root);
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
