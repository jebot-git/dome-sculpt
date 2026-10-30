// SPDX-License-Identifier: GPL-3.0-or-later
#include "palette-text.hpp"
#include "bdf_font.hpp"
#include "dome/palette.hpp"
#include "graphics.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_FONT_FORMATS_H
#include <unicode/ustring.h>
#include <unistd.h>
using namespace dome;
namespace {
void require(bool value, const char *message) {
  if (!value) throw std::runtime_error(message);
}
bool valid_utf8(const std::string &text) {
  UErrorCode error = U_ZERO_ERROR; int32_t length = 0;
  u_strFromUTF8(nullptr, 0, &length, text.data(), text.size(), &error);
  return error == U_BUFFER_OVERFLOW_ERROR || U_SUCCESS(error);
}
std::string only_ascii(const bdf_fonts::Face &face, const std::filesystem::path &directory) {
  auto source = bdf_fonts::read(face.file);
  auto chars = source.find("\nCHARS "); auto first = source.find("\nSTARTCHAR ", chars);
  require(chars != std::string::npos && first != std::string::npos, "Invalid source BDF");
  std::string kept; int count = 0;
  for (size_t at = first + 1; at < source.size();) {
    auto end = source.find("ENDCHAR", at); if (end == std::string::npos) break;
    end = source.find('\n', end); if (end == std::string::npos) break;
    auto block = source.substr(at, end - at + 1); auto encoding = block.find("ENCODING ");
    if (encoding != std::string::npos) {
      auto cp = std::stoi(block.substr(encoding + 9));
      if (cp >= 32 && cp < 127) { kept += block; ++count; }
    }
    at = source.find("STARTCHAR ", end); if (at == std::string::npos) break;
  }
  require(count == 95, "Need complete ASCII primary fixture");
  auto path = directory / "selected-ascii.bdf";
  std::ofstream(path) << source.substr(0, chars) << "\nCHARS " << count << '\n' << kept << "ENDFONT\n";
  return path.string();
}
size_t ink(const palette_text::Line &line) {
  size_t count = 0;
  for (auto &placed : line.glyphs)
    for (auto pixel : placed.bitmap->alpha) count += pixel != 0;
  return count;
}
}
int main(int argc, char **argv) try {
  if (argc != 2) return 2;
  auto directory = std::filesystem::absolute(argv[1]); std::filesystem::create_directories(directory);
  auto home = directory / ("runtime-" + std::to_string(getpid()));
  std::filesystem::create_directories(home);
  std::filesystem::permissions(home, std::filesystem::perms::owner_all);
  setenv("XDG_RUNTIME_DIR", home.c_str(), 1);
  auto config = bdf_fonts::export_config(); setenv("FONTCONFIG_FILE", config.c_str(), 1);
  auto selected = bdf_fonts::resolve("Unifont", "Sans-Serif", 16);
  auto sparse = only_ascii(selected, directory);
  std::ofstream(directory / "fonts.txt") << "Primary fixture derived at test time from " << selected.file
      << "\nFontconfig " << config << "\n";
  palette_text::Font font(sparse, 16), complete(selected.file, 16);
  require(font.layout(std::string_view{}, 600).glyphs.empty(), "Empty labels should have no glyphs");
  auto ascii = font.layout("ABC xyz 123", 600), reference = complete.layout("ABC xyz 123", 600);
  require(ascii.advance == 88 && ascii.advance == reference.advance, "Primary BDF advances changed");
  require(ascii.glyphs.size() == reference.glyphs.size(), "Primary BDF glyph count changed");
  for (size_t n = 0; n < ascii.glyphs.size(); ++n) {
    auto &a = ascii.glyphs[n]; auto &b = reference.glyphs[n];
    require(a.x == b.x && a.bitmap->alpha == b.bitmap->alpha && a.bitmap->top == b.bitmap->top,
        "Primary BDF raster changed");
  }
  palette_text::Font doubled(bdf_fonts::resolve("Unifont", "Sans-Serif", 16, 2).file, 32);
  auto large = doubled.layout("ABC xyz 123", 1200);
  require(large.advance == ascii.advance * 2 && large.glyphs.size() == ascii.glyphs.size(),
      "Double-size BDF advances do not match bitmap pixels");
  for (size_t n = 0; n < ascii.glyphs.size(); ++n) {
    auto &a = *ascii.glyphs[n].bitmap; auto &b = *large.glyphs[n].bitmap;
    require(large.glyphs[n].x == ascii.glyphs[n].x * 2 && b.width == a.width * 2 &&
        b.height == a.height * 2 && b.left == a.left * 2 && b.top == a.top * 2,
        "Double-size BDF placement changed");
    for (int y = 0; y < b.height; ++y) for (int x = 0; x < b.width; ++x)
      require(b.alpha[size_t(y) * b.width + x] == a.alpha[size_t(y / 2) * a.width + x / 2],
          "Double-size BDF lost exact whole-pixel sampling");
  }
  for (const auto &text : {std::string("Žluťoučký kůň / Élodie"), std::string("中文 日本語 한국어"), std::string("مرحبا / שלום")}) {
    auto line = font.layout(text, 600);
    std::cout << "Unicode " << text << " advance=" << line.advance << " ink=" << ink(line) << std::endl;
    require(line.text == text && line.advance > 20 && ink(line) > 25, "Unicode fallback lost text or pixels");
    for (auto &placed : line.glyphs)
      require(placed.bitmap->height <= 32, "Fallback bitmap ignored appearance size");
    require(font.loaded_faces() > 1, "Missing BDF glyph did not use fallback");
  }
  auto hebrew = font.layout("אבג", 600), last_hebrew = font.layout("ג", 600);
  require(!hebrew.glyphs.empty() && hebrew.glyphs.front().bitmap->alpha == last_hebrew.glyphs.front().bitmap->alpha,
      "Bidirectional layout did not place the Hebrew visual last letter first");
  auto invalid = font.layout(std::string("A\xc0\xaf\xed\xa0\x80\xf4\x90\x80\x80\xe2\x82") + "B", 600);
  require(valid_utf8(invalid.text) && invalid.text.front() == 'A' && invalid.text.back() == 'B' &&
      invalid.text.find("\xef\xbf\xbd") != std::string::npos && ink(invalid) > 20,
      "Malformed UTF-8 must produce visible replacement glyphs and preserve valid suffix");
  auto composed = font.layout("éééééééééé", 35);
  require(composed.text.ends_with("…") && composed.advance <= 35, "Measured clipping failed");
  auto prefix = composed.text.substr(0, composed.text.size() - 3);
  require(prefix.empty() || prefix.ends_with("é"), "Clipping split a combining grapheme");
  auto emoji = font.layout("👩‍🔬👩‍🔬👩‍🔬", 24);
  require(emoji.advance <= 24 && (emoji.text == "…" || emoji.text == "👩‍🔬…"), "Clipping split a ZWJ grapheme");
  for (int width = 0; width < 300; ++width) {
    auto line = font.layout("Český / 中文 / é / 👩‍🔬 / العربية", width);
    require(valid_utf8(line.text) && line.advance <= width, "Width clipping exceeded cell or corrupted UTF-8");
  }
  std::string hostile = "A"; for (int n = 0; n < 5000; ++n) hostile += "́";
  auto bounded = font.layout(hostile, 600);
  require(bounded.text.empty(), "Input budget split an unbounded grapheme");
  // Churn distinct real Unicode glyphs to exercise eviction rather than testing
  // a repeated glyph that would never approach the cache bound.
  for (int base = 0x4e00; base < 0x5600; base += 32) {
    std::string chunk;
    for (int cp = base; cp < base + 32; ++cp) {
      chunk += char(0xe0 | (cp >> 12)); chunk += char(0x80 | ((cp >> 6) & 63)); chunk += char(0x80 | (cp & 63));
    }
    font.layout(chunk, 10000);
  }
  require(font.cached_glyphs() <= 1024 && font.cached_bytes() <= 8 * 1024 * 1024 && font.loaded_faces() <= 32,
      "Unicode cache grew without bounds");
  uint32_t random = 0x5eed1234;
  for (int n = 0; n < 256; ++n) {
    std::string bytes;
    for (int i = 0; i < n % 63; ++i) {
      random ^= random << 13; random ^= random >> 17; random ^= random << 5;
      bytes += char(random & 255);
    }
    auto line = font.layout(bytes, 300);
    require(valid_utf8(line.text) && line.advance <= 300, "Malformed byte corpus corrupted bounded layout");
  }
  std::ofstream(directory / "fallback-fonts.txt") << font.source_manifest();
  FT_Library inspection{};
  require(!FT_Init_FreeType(&inspection), "Cannot inspect fallback fonts");
  std::istringstream manifest(font.source_manifest());
  for (std::string face; std::getline(manifest, face);) {
    auto metadata = face.rfind(" index=");
    require(metadata != std::string::npos, "Missing fallback font identity");
    FT_Face loaded{};
    require(!FT_New_Face(inspection, face.substr(0, metadata).c_str(), 0, &loaded), "Cannot inspect selected fallback");
    auto format = FT_Get_Font_Format(loaded);
    bool bitmap = format && std::string_view(format) == "BDF" && !FT_IS_SCALABLE(loaded);
    FT_Done_Face(loaded);
    require(bitmap, "Spatial Unicode fallback selected a non-BDF face");
  }
  FT_Done_FreeType(inspection);
  NativeGraphics graphics;
  std::cout << "GL " << glGetString(GL_RENDERER) << '\n';
  GLuint fbo, color; glGenFramebuffers(1, &fbo); glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glGenTextures(1, &color); glBindTexture(GL_TEXTURE_2D, color);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 640, 640, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color, 0);
  require(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE, "Incomplete framebuffer");
  glViewport(0, 0, 640, 640);
  SpatialPalette palette;
  EditorState state; state.menu = true; state.font_file = sparse; state.font_pixels = 16;
  state.objects.push_back({1, "Žluťoučký kůň / 中文 / 日本語 / 한국어", true, false, {}, {}});
  state.objects.push_back({2, "参照モデル / Élodie / 日本語の長い参照モデル名", false, false, {}, {}});
  state.active_object = 1; state.skin_options.reference = 2;
  state.message = "école / مرحبا / שלום / " + invalid.text;
  Mat4 vp{}; vp[0] = vp[5] = 1.f / .3f; vp[10] = vp[15] = 1;
  auto capture = [&](const std::string &name) {
    glClearColor(0, 0, 0, 0); glClear(GL_COLOR_BUFFER_BIT);
    palette.draw(state, vp);
    std::vector<uint8_t> pixels(640 * 640 * 4);
    glReadPixels(0, 0, 640, 640, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    std::ofstream out(directory / (name + ".ppm"), std::ios::binary); out << "P6\n640 640\n255\n";
    for (int y = 639; y >= 0; --y) for (int x = 0; x < 640; ++x)
      out.write(reinterpret_cast<const char *>(pixels.data() + (y * 640 + x) * 4), 3);
    require(bool(out), "Cannot save palette capture"); return pixels;
  };
  state.menu_page = 14; auto dark = capture("skin-dark-utf8");
  // Names changing independently of a model revision must invalidate text.
  state.objects[1].name = "別の参照 / Příliš žluťoučký kůň";
  auto renamed = capture("skin-renamed-utf8"); require(renamed != dark, "Name changes did not invalidate palette texture");
  state.menu_page = 13; auto weights = capture("weights-utf8");
  state.light = true; state.font_pixels = 32; state.font_file = bdf_fonts::resolve("Unifont", "Sans-Serif", 16, 2).file;
  state.menu_page = 14; auto light = capture("skin-light-double-utf8");
  require(dark != light && weights != dark, "Palette states did not redraw");
  // Existing sRGB/linear and opacity contracts: output background is exact.
  require(dark[0] == 31 && dark[1] == 37 && dark[2] == 45 && dark[3] == 255, "sRGB palette color changed");
  palette.set_linear_output(true); auto linear = capture("skin-linear-utf8");
  require(linear[0] < light[0] && linear[3] == 255, "Linear palette color contract changed");
  state.opacity = 64; palette.set_linear_output(false); auto translucent = capture("skin-opacity-utf8");
  require(translucent[3] == 64, "Palette opacity changed");
  glDeleteTextures(1, &color); glDeleteFramebuffers(1, &fbo);
  std::filesystem::remove_all(home);
  FcFini(); // Release process-global application-font scans for leak checking.
  std::cout << "PASS primary/fallback UTF-8, graphemes, bidi, clipping, invalid input, bounded cache and real GL palette\n";
} catch (const std::exception &error) { std::cerr << error.what() << '\n'; return 1; }
