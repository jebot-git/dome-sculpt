// SPDX-License-Identifier: GPL-3.0-or-later
#include "palette-text.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>
#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_FONT_FORMATS_H
#include <hb-ft.h>
#include <unicode/ubidi.h>
#include <unicode/ubrk.h>
#include <unicode/ustring.h>
#include <unicode/utf16.h>
#include <unicode/uchar.h>
namespace dome::palette_text {
namespace {
constexpr size_t max_input = 4096, max_faces = 32, max_glyphs = 1024,
                 max_bitmap_bytes = 8 * 1024 * 1024;
std::u16string clean(std::string_view bytes) {
  // Bound hostile labels before decoding. ICU replaces malformed subsequences
  // with U+FFFD (never treats continuation bytes as independent glyphs).
  bool bounded = bytes.size() > max_input;
  size_t end = std::min(bytes.size(), max_input);
  while (end && end < bytes.size() && (uint8_t(bytes[end]) & 0xc0) == 0x80) --end;
  bytes = bytes.substr(0, end);
  std::u16string result(bytes.size() + 1, 0);
  int32_t length = 0; UErrorCode status = U_ZERO_ERROR;
  u_strFromUTF8WithSub(reinterpret_cast<UChar *>(result.data()), result.size(),
      &length, bytes.data(), bytes.size(), 0xfffd, nullptr, &status);
  if (U_FAILURE(status)) throw std::runtime_error("Cannot decode spatial label");
  result.resize(length);
  for (auto &c : result) if (c < 32 || c == 127) c = u' ';
  if (bounded && !result.empty()) {
    // The input budget can cut a long combining/ZWJ sequence. Drop its final
    // grapheme rather than displaying a partial user-visible character.
    status = U_ZERO_ERROR;
    auto breaks = ubrk_open(UBRK_CHARACTER, "root", reinterpret_cast<const UChar *>(result.data()), result.size(), &status);
    if (U_FAILURE(status)) throw std::runtime_error("Cannot segment spatial label");
    auto end = ubrk_preceding(breaks, result.size()); ubrk_close(breaks);
    result.resize(std::max(0, end));
  }
  return result;
}
std::string utf8(const std::u16string &value) {
  std::string bytes(value.size() * 3 + 1, 0); int32_t length = 0;
  UErrorCode status = U_ZERO_ERROR;
  u_strToUTF8(bytes.data(), bytes.size(), &length,
      reinterpret_cast<const UChar *>(value.data()), value.size(), &status);
  if (U_FAILURE(status)) throw std::runtime_error("Cannot encode spatial label");
  bytes.resize(length); return bytes;
}
}
struct Font::Impl {
  FT_Library library{};
  struct Face {
    FT_Face ft{}; hb_font_t *hb{}; std::string file; int index = 0; double scale = 1;
    ~Face() { if (hb) hb_font_destroy(hb); if (ft) FT_Done_Face(ft); }
  };
  std::vector<std::unique_ptr<Face>> faces;
  int pixels;
  using Key = std::pair<size_t, uint32_t>;
  struct Cached { std::shared_ptr<Glyph> bitmap; uint64_t age; };
  std::map<Key, Cached> cache;
  size_t bytes = 0; uint64_t clock = 0;
  std::map<std::u32string, size_t> selections;
  explicit Impl(const std::string &file, int size) : pixels(std::clamp(size, 4, 128)) {
    if (FT_Init_FreeType(&library)) throw std::runtime_error("Cannot initialize menu font");
    try {
      add(file, 0, true);
    } catch (...) { FT_Done_FreeType(library); throw; }
  }
  ~Impl() { faces.clear(); FT_Done_FreeType(library); }
  size_t add(const std::string &file, int index, bool primary = false) {
    for (size_t n = 0; n < faces.size(); ++n)
      if (faces[n]->file == file && faces[n]->index == index) return n;
    if (faces.size() >= max_faces) return 0;
    auto f = std::make_unique<Face>(); f->file = file; f->index = index;
    if (FT_New_Face(library, file.c_str(), index, &f->ft))
      throw std::runtime_error("Cannot load spatial menu font");
    auto format = FT_Get_Font_Format(f->ft);
    if (!format || std::string(format) != "BDF" || FT_IS_SCALABLE(f->ft) || !f->ft->num_fixed_sizes)
      throw std::runtime_error("Spatial menus need native DomeD BDF fonts");
    FT_Select_Charmap(f->ft, FT_ENCODING_UNICODE);
    if (f->ft->num_fixed_sizes) {
      int strike = 0;
      for (int n = 1; n < f->ft->num_fixed_sizes; ++n)
        if (std::abs(f->ft->available_sizes[n].height - pixels) <
            std::abs(f->ft->available_sizes[strike].height - pixels)) strike = n;
      if (FT_Select_Size(f->ft, strike)) throw std::runtime_error("Cannot select spatial font strike");
      if (!primary) {
        int height = std::max(1, int(f->ft->size->metrics.y_ppem));
        // Fontconfig can prefer a larger bitmap-only family. Preserve whole
        // pixels on enlargement, and use integer decimation on reduction so
        // fallback text cannot become twice the selected appearance size.
        f->scale = pixels >= height ? std::max(1l, std::lround(double(pixels) / height))
            : 1. / std::max(1l, std::lround(double(height) / pixels));
      }
    } else if (FT_Set_Pixel_Sizes(f->ft, 0, pixels))
      throw std::runtime_error("Cannot size fallback font");
    f->hb = hb_ft_font_create_referenced(f->ft);
    // Layout consumes 26.6 pixel positions. The native BDF-only HarfBuzz
    // constructor starts in font units, while upstream hb-ft initializes this
    // scale implicitly. Set it explicitly for both runtimes; bitmap sampling
    // and the separate whole-pixel fallback scale stay unchanged.
    const int ppem = f->ft->size->metrics.y_ppem;
    hb_font_set_scale(f->hb, ppem * 64, ppem * 64);
    hb_font_set_ppem(f->hb, ppem, ppem);
    hb_ft_font_set_load_flags(f->hb, FT_LOAD_DEFAULT | FT_LOAD_TARGET_MONO);
    faces.push_back(std::move(f)); return faces.size() - 1;
  }
  size_t face(const std::u32string &cluster) {
    if (auto found = selections.find(cluster); found != selections.end()) return found->second;
    auto covers = [&](size_t n) {
      return std::all_of(cluster.begin(), cluster.end(), [&](auto cp) {
        return u_hasBinaryProperty(cp, UCHAR_DEFAULT_IGNORABLE_CODE_POINT) || FT_Get_Char_Index(faces[n]->ft, cp);
      });
    };
    auto remember = [&](size_t n) {
      if (selections.size() >= 2048) selections.clear();
      selections.emplace(cluster, n); return n;
    };
    for (size_t n = 0; n < faces.size(); ++n) if (covers(n)) return remember(n);
    if (faces.size() >= max_faces) return remember(0);
    auto pattern = FcPatternCreate(); auto chars = FcCharSetCreate();
    for (auto cp : cluster) if (!u_hasBinaryProperty(cp, UCHAR_DEFAULT_IGNORABLE_CODE_POINT)) FcCharSetAddChar(chars, cp);
    // DomeD's chrome loads Unifont for broad bitmap fallback coverage.
    // Keep that stable preference, then let Fontconfig choose monospace faces
    // for scripts outside its coverage (including Unifont Upper).
    FcPatternAddString(pattern, FC_FAMILY, reinterpret_cast<const FcChar8 *>("Unifont"));
    FcPatternAddString(pattern, FC_FAMILY, reinterpret_cast<const FcChar8 *>("monospace"));
    FcPatternAddInteger(pattern, FC_SPACING, FC_MONO);
    FcPatternAddBool(pattern, FC_SCALABLE, FcFalse);
    FcPatternAddString(pattern, FC_FONTFORMAT, reinterpret_cast<const FcChar8 *>("BDF"));
    FcPatternAddDouble(pattern, FC_PIXEL_SIZE, pixels);
    FcPatternAddCharSet(pattern, FC_CHARSET, chars); FcCharSetDestroy(chars);
    FcConfigSubstitute(nullptr, pattern, FcMatchPattern); FcDefaultSubstitute(pattern);
    FcResult result; auto matches = FcFontSort(nullptr, pattern, FcTrue, nullptr, &result);
    FcPatternDestroy(pattern);
    size_t chosen = 0;
    if (matches) {
      for (int n = 0; n < matches->nfont; ++n) {
        FcCharSet *coverage = nullptr; FcChar8 *file = nullptr; int index = 0;
        if (FcPatternGetCharSet(matches->fonts[n], FC_CHARSET, 0, &coverage) != FcResultMatch ||
            !std::all_of(cluster.begin(), cluster.end(), [&](auto cp) { return u_hasBinaryProperty(cp, UCHAR_DEFAULT_IGNORABLE_CODE_POINT) || FcCharSetHasChar(coverage, cp); }) ||
            FcPatternGetString(matches->fonts[n], FC_FILE, 0, &file) != FcResultMatch) continue;
        FcPatternGetInteger(matches->fonts[n], FC_INDEX, 0, &index);
        try { chosen = add(reinterpret_cast<const char *>(file), index); }
        catch (const std::runtime_error &) { continue; }
        if (covers(chosen)) break;
      }
      FcFontSetDestroy(matches);
    }
    return remember(chosen);
  }
  std::shared_ptr<Glyph> glyph(size_t face_index, uint32_t index) {
    Key key{face_index, index};
    if (auto found = cache.find(key); found != cache.end()) {
      found->second.age = ++clock; return found->second.bitmap;
    }
    auto &f = *faces[face_index]; auto out = std::make_shared<Glyph>();
    if (!FT_Load_Glyph(f.ft, index, FT_LOAD_RENDER | FT_LOAD_MONOCHROME | FT_LOAD_TARGET_MONO)) {
      auto &b = f.ft->glyph->bitmap; double scale = f.scale;
      out->width = int(std::ceil(b.width * scale)); out->height = int(std::ceil(b.rows * scale));
      out->left = int(std::lround(f.ft->glyph->bitmap_left * scale)); out->top = int(std::lround(f.ft->glyph->bitmap_top * scale));
      // Reject pathological font dimensions before allocation.
      if (out->width > 512 || out->height > 512) { out->width = out->height = 0; }
      out->alpha.resize(size_t(out->width) * out->height);
      for (int y = 0; y < out->height; ++y) for (int x = 0; x < out->width; ++x) {
        int sy = std::min(int(b.rows) - 1, int(y / scale));
        int sx = std::min(int(b.width) - 1, int(x / scale));
        auto row = b.buffer + (b.pitch >= 0 ? sy : int(b.rows) - 1 - sy) * std::abs(b.pitch);
        out->alpha[size_t(y) * out->width + x] = b.pixel_mode == FT_PIXEL_MODE_MONO
            ? ((row[sx / 8] & (128 >> (sx % 8))) ? 255 : 0) : row[sx];
      }
    }
    while (!cache.empty() && (cache.size() >= max_glyphs || bytes + out->alpha.size() > max_bitmap_bytes)) {
      auto oldest = std::min_element(cache.begin(), cache.end(), [](auto &a, auto &b) { return a.second.age < b.second.age; });
      bytes -= oldest->second.bitmap->alpha.size(); cache.erase(oldest);
    }
    bytes += out->alpha.size(); cache.emplace(key, Cached{out, ++clock}); return out;
  }
  Line shape(const std::u16string &text, bool render = true) {
    Line line; line.text = utf8(text);
    if (text.empty()) return line;
    UErrorCode error = U_ZERO_ERROR; UBiDi *bidi = ubidi_open();
    if (!bidi) throw std::bad_alloc();
    ubidi_setPara(bidi, reinterpret_cast<const UChar *>(text.data()), text.size(), UBIDI_DEFAULT_LTR, nullptr, &error);
    int runs = ubidi_countRuns(bidi, &error);
    if (U_FAILURE(error)) { ubidi_close(bidi); throw std::runtime_error("Cannot lay out spatial label"); }
    for (int run = 0; run < runs; ++run) {
      int32_t start, length; auto direction = ubidi_getVisualRun(bidi, run, &start, &length);
      struct Segment { int32_t start, end; size_t font; hb_script_t script; };
      std::vector<Segment> segments;
      UErrorCode segmentation = U_ZERO_ERROR;
      auto clusters = ubrk_open(UBRK_CHARACTER, "root", reinterpret_cast<const UChar *>(text.data()), text.size(), &segmentation);
      if (U_FAILURE(segmentation)) { ubidi_close(bidi); throw std::runtime_error("Cannot segment spatial label"); }
      for (int32_t pos = start; pos < start + length;) {
        int32_t at = pos;
        int32_t end = std::min(start + length, ubrk_following(clusters, pos));
        std::u32string cluster;
        while (pos < end) { UChar32 cp; U16_NEXT(text.data(), pos, end, cp); cluster.push_back(cp); }
        size_t selected = face(cluster); auto script = hb_unicode_script(hb_unicode_funcs_get_default(), cluster.front());
        if (script == HB_SCRIPT_INHERITED || script == HB_SCRIPT_COMMON)
          script = segments.empty() ? HB_SCRIPT_COMMON : segments.back().script;
        if (!segments.empty() && segments.back().font == selected &&
            (segments.back().script == script || script == HB_SCRIPT_COMMON)) segments.back().end = pos;
        else segments.push_back({at, pos, selected, script});
      }
      ubrk_close(clusters);
      if (direction == UBIDI_RTL) std::reverse(segments.begin(), segments.end());
      for (auto &part : segments) {
        auto &f = *faces[part.font]; auto buffer = hb_buffer_create();
        hb_buffer_set_cluster_level(buffer, HB_BUFFER_CLUSTER_LEVEL_MONOTONE_GRAPHEMES);
        hb_buffer_add_utf16(buffer, reinterpret_cast<const uint16_t *>(text.data()), text.size(), part.start, part.end - part.start);
        hb_buffer_set_direction(buffer, direction == UBIDI_RTL ? HB_DIRECTION_RTL : HB_DIRECTION_LTR);
        hb_buffer_set_script(buffer, part.script); hb_buffer_guess_segment_properties(buffer);
        hb_shape(f.hb, buffer, nullptr, 0);
        unsigned count; auto info = hb_buffer_get_glyph_infos(buffer, &count);
        auto positions = hb_buffer_get_glyph_positions(buffer, nullptr);
        for (unsigned n = 0; n < count; ++n) {
          if (render) line.glyphs.push_back({glyph(part.font, info[n].codepoint),
              line.advance + int(std::lround(positions[n].x_offset * f.scale / 64.)),
              int(std::lround(positions[n].y_offset * f.scale / 64.))});
          line.advance += int(std::lround(positions[n].x_advance * f.scale / 64.));
        }
        hb_buffer_destroy(buffer);
      }
    }
    ubidi_close(bidi); return line;
  }
};
Font::Font(const std::string &bdf, int pixels) : impl_(std::make_unique<Impl>(bdf, pixels)) {}
Font::~Font() = default;
Line Font::layout(std::string_view bytes, int maximum) {
  if (maximum <= 0) return {};
  auto text = clean(bytes); auto full = impl_->shape(text, false);
  if (full.advance <= maximum) return impl_->shape(text);
  UErrorCode error = U_ZERO_ERROR;
  auto breaks = ubrk_open(UBRK_CHARACTER, "root", reinterpret_cast<const UChar *>(text.data()), text.size(), &error);
  if (U_FAILURE(error)) throw std::runtime_error("Cannot segment spatial label");
  std::vector<int32_t> ends{0};
  for (int32_t n = ubrk_first(breaks); (n = ubrk_next(breaks)) != UBRK_DONE;) ends.push_back(n);
  ubrk_close(breaks);
  // Binary search prefixes, then shape the chosen complete graphemes with an
  // ellipsis. Actual advances include proportional BDF and fallback metrics.
  size_t lo = 0, hi = ends.size(); Line best = impl_->shape(u"\u2026", false);
  if (best.advance > maximum) return {};
  while (lo < hi) {
    auto mid = lo + (hi - lo) / 2;
    auto candidate = impl_->shape(text.substr(0, ends[mid]) + u"\u2026", false);
    if (candidate.advance <= maximum) { best = std::move(candidate); lo = mid + 1; }
    else hi = mid;
  }
  auto clipped = clean(best.text);
  return impl_->shape(clipped);
}
size_t Font::cached_glyphs() const { return impl_->cache.size(); }
size_t Font::cached_bytes() const { return impl_->bytes; }
size_t Font::loaded_faces() const { return impl_->faces.size(); }
std::string Font::source_manifest() const {
  std::string result;
  for (auto &face : impl_->faces)
    result += face->file + " index=" + std::to_string(face->index) +
        " pixels=" + std::to_string(face->ft->size->metrics.y_ppem) +
        " scale=" + std::to_string(face->scale) + "\n";
  return result;
}
} // namespace dome::palette_text
