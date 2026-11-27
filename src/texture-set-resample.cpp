// SPDX-License-Identifier: GPL-3.0-or-later
#include "texture-set-resample.hpp"
#include "dome/material-bindings.hpp"
#include "dome/source-maps.hpp"
#include "emission-paint-access.hpp"
#include "paint-grid-filter.hpp"
#include "paint-overlay-sample.hpp"
#include "polygon-surface-preview.hpp"
#include "texture-set-field-source.hpp"
#include "texture-set-graph.hpp"
#include "texture-set-paint-access.hpp"
#include "texture-set-surface.hpp"
#include "native-paint-sampling.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <tuple>
namespace dome {
void TextureSetResolutionOptions::validate() const {
  if (resolution < 64 || resolution > 2048 || (resolution & (resolution - 1)) ||
      !std::isfinite(maximum_component_error) || maximum_component_error < 0 ||
      maximum_component_error > 1 || !std::isfinite(maximum_rms_error) ||
      maximum_rms_error < 0 || maximum_rms_error > 1)
    throw std::invalid_argument(
        "Invalid texture-set resolution or reconstruction quality policy");
}
namespace detail {
bool can_resize_retained_texture_grid(const TextureSetView &field) noexcept {
  const auto &paint = field.painted;
  const auto source = field.source ? field.source->source_preview() : nullptr;
  if (!paint || paint->in_stroke() || !source) return false;
  const auto bake = paint->baked();
  if (source->model() == PolygonSurfaceNormalModel::NativeNOnly)
    return resizable_canonical_bake(bake.get()) && (!bake || bake->maps.normal.rgba.empty());
  return bake && bake->normal_field;
}

bool can_resize_native_texture_grid(const SculptObject &object) noexcept {
  if (!object.mesh_active || !object.painted || object.texture_sets || object.polygon)
    return false;
  const auto &binding = object.material_bindings;
  if (binding && binding->native_paint_sampling) {
    const auto &owner = binding->native_paint_sampling;
    const auto &paint = *object.painted;
    return !paint.in_stroke() && owner->valid_owner(owner) && owner->matches_current(paint) &&
           resizable_native_uv_bake(paint.baked().get()) &&
           (resizable_canonical_bake(paint.baked().get()) || bool(binding->native_paint_frames));
  }
  if (!TextureSetPaintAccess::can_resize_raw(*object.painted))
    return false;
  // A retopology patch retains topology and mesh identity independently of
  // its writable raster. The raw-grid transition keeps its projected images.
  return !binding ||
         (!binding->normal_preparation && !binding->color_preparation &&
          !binding->canonical_attributes && !binding->native_paint_sampling &&
          !binding->canonical_paint_sampling && !binding->native_paint_frames &&
          !binding->polygon_surface_transfer && !binding->sampling_reparameterized);
}

TextureSetLimits native_texture_grid_limits() {
  TextureSetLimits limits;
  // Raster initialization at 2048 requires more than the default field work quota.
  limits.maximum_work = 128'000'000;
  return limits;
}

namespace {
uint8_t fallback_byte(float v) {
  return uint8_t(std::lround(std::clamp(v, 0.f, 1.f) * 255));
}
uint8_t byte(double v) {
  return uint8_t(std::lround(std::clamp(v, 0., 1.) * 255));
}
double linear(uint8_t v) {
  static const auto values = [] {
    std::array<double, 256> table{};
    for (size_t i = 0; i < table.size(); ++i) {
      const double x = double(i) / 255;
      table[i] = x <= .04045 ? x / 12.92 : std::pow((x + .055) / 1.055, 2.4);
    }
    return table;
  }();
  return values[v];
}
double encoded(double v) {
  return v <= .0031308 ? v * 12.92 : 1.055 * std::pow(v, 1 / 2.4) - .055;
}
uint64_t word(double v) { return std::bit_cast<uint64_t>(v); }
double number(uint64_t v) { return std::bit_cast<double>(v); }
const paint_grid::Tables &grid_tables() {
  static const auto table=[] {
    paint_grid::Tables result{};
    for(unsigned i=0;i<256;++i) {
      result.unit[i]=word(i/255.);
      result.srgb[i]=word(linear(uint8_t(i)));
    }
    // Share the first binary64 input that produces each byte, including
    // the host's exact sRGB and half-byte rounding boundaries.
    for(unsigned i=1;i<256;++i) for(bool srgb:{false,true}) {
      uint64_t lo=0,hi=word(1.);
      while(lo<hi) {
        const auto mid=lo+(hi-lo)/2;
        const auto v=number(mid);
        if(byte(srgb?encoded(v):v)>=i)hi=mid;
        else lo=mid+1;
      }
      (srgb?result.srgb_threshold:result.unit_threshold)[i-1]=lo;
    }
    return result;
  }();
  return table;
}
class GridFilter {
  paint_grid::Filter filter_{grid_tables()};
  std::vector<paint_grid::Pixel> input_,output_;
public:
  static constexpr size_t peak_bytes=2*(sizeof(paint_grid::Tables)+
      (paint_grid::max_input_pixels+paint_grid::max_output_pixels)*sizeof(paint_grid::Pixel));
  GridFilter() {
    input_.reserve(paint_grid::max_input_pixels);
    output_.reserve(paint_grid::max_output_pixels);
  }
  void apply(uint32_t from,uint32_t to,paint_grid::Rectangle target,
      const paint_grid::Layout &layout,const std::function<bool()> &cancelled,
      auto &&read,auto &&write) {
    const auto source_range=[&](uint32_t first,uint32_t count) {
      const auto a=paint_grid::axis(from,to,first);
      const auto b=paint_grid::axis(from,to,first+count-1);
      return std::pair{a.first,(from>to?b.first+b.count:b.second+1)-a.first};
    };
    const auto [sx,width]=source_range(target.x,target.width);
    for(uint32_t row=0;row<target.height;) {
      paint_grid::checkpoint(cancelled);
      uint32_t rows=std::min(target.height-row,uint32_t(paint_grid::max_output_pixels/target.width));
      auto [sy,height]=source_range(target.y+row,rows);
      while(size_t(width)*height>paint_grid::max_input_pixels) {
        if(rows==1)throw std::length_error("Paint-grid footprint cannot fit a bounded device batch");
        rows/=2;
        std::tie(sy,height)=source_range(target.y+row,rows);
      }
      input_.resize(size_t(width)*height);
      output_.resize(size_t(target.width)*rows);
      for(uint32_t y=0;y<height;++y) {
        paint_grid::checkpoint(cancelled);
        for(uint32_t x=0;x<width;++x)
          input_[size_t(y)*width+x]=read(sx+x,sy+y);
      }
      const paint_grid::Batch batch{from,to,{sx,sy,width,height},
                                    {target.x,target.y+row,target.width,rows}};
      filter_.run(batch,layout,input_,output_,cancelled);
      for(uint32_t y=0;y<rows;++y) {
        paint_grid::checkpoint(cancelled);
        for(uint32_t x=0;x<target.width;++x)
          write(x,row+y,output_[size_t(y)*target.width+x]);
      }
      row+=rows;
    }
  }
};
void stop(const TextureSetLimits &l) {
  if (l.cancelled && l.cancelled())
    throw std::runtime_error("Texture-set resolution cancelled");
}
void charge(size_t &bytes, size_t n, size_t unit, const TextureSetLimits &l) {
  if (bytes > l.maximum_bytes || (unit && n > (l.maximum_bytes - bytes) / unit))
    throw std::length_error(
        "Texture-set resolution exceeds its complete peak-byte budget");
  bytes += n * unit;
}
void work(TextureSetResolutionReport &r, size_t n, const TextureSetLimits &l) {
  if (r.work > l.maximum_work || n > l.maximum_work - r.work)
    throw std::length_error(
        "Texture-set resolution exceeds its measured work budget");
  r.work += n;
}
struct Sample {
  PaintPixel common;
  std::array<double, 4> pbr{};
  PaintEmissionValue emission;
  bool common_present = false, pbr_present = false, emission_present = false;
};
struct StoredPixel {
  const PaintPixel *common = nullptr;
  const std::array<uint8_t, 4> *pbr = nullptr;
  const PaintEmissionTile *emission = nullptr;
  size_t emission_index = 0;
  bool empty() const { return !common && !pbr && !emission; }
  bool operator==(const StoredPixel &other) const {
    const auto equal = [](const auto *a, const auto *b) {
      return a == b || (a && b && *a == *b);
    };
    return equal(common, other.common) && equal(pbr, other.pbr) &&
           ((emission == other.emission && (!emission || emission_index == other.emission_index)) ||
            (emission && other.emission &&
             emission->texel(emission_index) == other.emission->texel(other.emission_index)));
  }
};
StoredPixel stored_pixel(const PaintLayer &layer, uint32_t r, uint32_t x,
                         uint32_t y) {
  const size_t stride = PaintTile::size;
  const auto tile = (y / stride) * (r / stride) + x / stride;
  const auto offset = (y % stride) * stride + x % stride;
  const auto &common = layer.tiles.at(tile);
  StoredPixel stored;
  if (common)
    stored.common = &common->pixels[offset];
  if (!layer.pbr_tiles.empty())
    if (const auto &pbr = layer.pbr_tiles.at(tile))
      stored.pbr = &pbr->pixels[offset];
  if (!layer.emission_tiles.empty()) {
    stored.emission = layer.emission_tiles.at(tile).get();
    stored.emission_index = offset;
  }
  return stored;
}
Sample pixel(const StoredPixel &stored,bool read_pbr=true,bool read_emission=true) {
  Sample s;
  if (stored.common) {
    s.common = *stored.common;
    s.common_present = true;
  }
  const auto &v = s.common.data;
  s.pbr_present = bool(stored.pbr);
  if(read_pbr) {
    const auto *color = stored.pbr ? stored.pbr->data() : v.data();
    for (size_t k = 0; k < 4; ++k)
      s.pbr[k] = k < 3 ? linear(color[k]) : double(color[k]) / 255;
  }
  s.emission_present = bool(stored.emission);
  if (!read_emission)
    return s;
  if (stored.emission) {
    s.emission = stored.emission->texel(stored.emission_index);
  } else {
    s.emission.coverage = float(v[12]) / 255;
    for (size_t k = 0; k < 3; ++k)
      s.emission.legacy[k] = float(linear(v[6 + k]));
  }
  return s;
}
paint_grid::Pixel grid_layer_pixel(const PaintLayer &layer,uint32_t r,uint32_t x,uint32_t y) {
  const auto tile=size_t(y/32)*(r/32)+x/32,offset=size_t(y%32)*32+x%32;
  paint_grid::Pixel p;
  if(const auto &common=layer.tiles[tile]) {
    const auto &bytes=common->pixels[offset].data;
    std::copy_n(bytes.data(),9,p.values);
    std::copy_n(bytes.data()+9,4,p.coverage);
    p.mask=bytes[13];
    p.present=1;
  }
  if(!layer.pbr_tiles.empty()) {
    const auto &plane=layer.pbr_tiles[tile];
    if(plane) {
      std::copy_n(plane->pixels[offset].data(),4,p.values+9);
      p.present|=2;
    }else std::copy_n(p.values,4,p.values+9);
  }
  if(!layer.emission_tiles.empty()) {
    const auto &plane=layer.emission_tiles[tile];
    if(plane) {
      const auto light=plane->texel(offset);
      for(unsigned c=0;c<3;++c) {
        p.emission[c]=word(light.legacy[c]);
        p.emission[3+c]=word(light.physical[c]);
      }
      p.emission[6]=word(light.coverage);
      p.present|=4;
    }else{
      const auto &table=grid_tables();
      for(unsigned c=0;c<3;++c)
        p.emission[c]=word(float(number(table.srgb[p.values[6+c]])));
      p.emission[6]=word(float(p.coverage[3])/255);
    }
  }
  return p;
}
paint_grid::Layout layer_layout(const PaintLayer &layer) {
  paint_grid::Layout layout;
  layout.active=layer.pbr_tiles.empty()?0x1ffu:0x1fffu;
  layout.pbr=layout.active&0x1e00u;
  layout.srgb=0x1c7u|(7u<<9);
  layout.groups=4;
  layout.group[4]=1;
  layout.group[5]=2;
  for(unsigned c=6;c<9;++c)layout.group[c]=3;
  layout.mask=true;
  if(!layer.emission_tiles.empty())layout.floats=paint_grid::FloatPlane::UnassociatedPaint;
  return layout;
}

void omit_default_reduction_channels(const PaintLayer &layer,paint_grid::Layout &layout,
                                     TextureSetResolutionReport &report,const TextureSetLimits &limits) {
  // Test stored bytes, including unassociated values under zero coverage.
  // Absent common tiles contribute the default bytes. Neither independent
  // color nor FLOAT emission can be classified by the common plane.
  std::array<uint8_t,14> differing{};
  for(const auto &tile:layer.tiles) {
    stop(limits);
    if(!tile)continue;
    work(report,tile->pixels.size(),limits);
    for(const auto &pixel:tile->pixels) {
      for(size_t c=0;c<13;++c)differing[c]|=pixel.data[c];
      differing[13]|=pixel.data[13]^255u;
    }
    if(std::ranges::all_of(differing,[](uint8_t value){return value!=0;}))break;
  }
  for(unsigned c=0;c<9;++c)
    if(!differing[c])layout.active&=~(1u<<c);
  for(unsigned g=0;g<4;++g)
    if(!differing[9+g])layout.coverage_mask&=~(1u<<g);
  layout.mask=differing[13]!=0;
}

size_t reduction_tap_work(const paint_grid::Layout &layout) {
  // One input fetch, the byte planes with actual filtering, and the seven
  // raw plus six associated FLOAT sums of an independent emission plane.
  return 1+std::popcount(layout.active)+std::popcount(layout.coverage_mask)+
      size_t(layout.mask)+(layout.floats==paint_grid::FloatPlane::Absent?0:13);
}

bool has_authored_tile(const PaintLayer &layer, size_t tile) {
  return bool(layer.tiles.at(tile)) ||
         (!layer.pbr_tiles.empty() && layer.pbr_tiles.at(tile)) ||
         (!layer.emission_tiles.empty() && layer.emission_tiles.at(tile));
}

bool has_authored_footprint(const PaintLayer &layer, uint32_t from, uint32_t to,
                            uint32_t tx, uint32_t ty) {
  const uint32_t stride = from / PaintTile::size;
  if (to >= from) {
    const auto scale = to / from;
    return has_authored_tile(layer, size_t(ty / scale) * stride + tx / scale);
  }
  const auto scale = from / to;
  for (auto y = ty * scale; y < (ty + 1) * scale; ++y)
    for (auto x = tx * scale; x < (tx + 1) * scale; ++x)
      if (has_authored_tile(layer, size_t(y) * stride + x))
        return true;
  return false;
}

template <class F>
void footprint(uint32_t from, uint32_t to, uint32_t x, uint32_t y, F &&f) {
  if (from > to) {
    const uint32_t scale = from / to;
    const double w = 1. / (double(scale) * scale);
    for (uint32_t sy = y * scale; sy < (y + 1) * scale; ++sy)
      for (uint32_t sx = x * scale; sx < (x + 1) * scale; ++sx)
        f(sx, sy, w);
  } else {
    const double px = (double(x) + .5) * from / to - .5,
                 py = (double(y) + .5) * from / to - .5;
    const int bx = int(std::floor(px)), by = int(std::floor(py));
    const double fx = px - bx, fy = py - by;
    for (int i = 0; i < 4; ++i) {
      const double w = ((i & 1) ? fx : 1 - fx) * ((i & 2) ? fy : 1 - fy);
      if (w)
        f(uint32_t(std::clamp(bx + (i & 1), 0, int(from) - 1)),
          uint32_t(std::clamp(by + ((i >> 1) & 1), 0, int(from) - 1)), w);
    }
  }
}
Sample filtered_samples(const PaintLayer &l,uint32_t from,auto &&for_each_pixel,
                        const paint_grid::Layout *selection=nullptr) {
  const unsigned channels=selection?selection->active:0x1ffu;
  const unsigned groups=selection?selection->coverage_mask:0xfu;
  const bool read_pbr=!selection||selection->pbr;
  const bool read_emission=!selection||selection->floats!=paint_grid::FloatPlane::Absent;
  const bool read_mask=!selection||selection->mask;
  std::array<double, 9> associated{}, raw{};
  std::array<double, 4> coverage{}, pbr{}, pbr_raw{};
  std::array<double, 3> legacy{}, physical{}, legacy_raw{}, physical_raw{};
  double emission_coverage = 0, mask = 0;
  Sample out;
  for_each_pixel([&](uint32_t sx, uint32_t sy, double w) {
    const auto s = pixel(stored_pixel(l, from, sx, sy),read_pbr,read_emission);
    const auto &d = s.common.data;
    out.common_present |= s.common_present;
    out.pbr_present |= s.pbr_present;
    out.emission_present |= s.emission_present;
    for (size_t g = 0; g < 4; ++g) {
      const size_t first = g == 0   ? 0
                           : g == 1 ? 4
                           : g == 2 ? 5
                                    : 6,
                   count = g == 0   ? 4
                           : g == 3 ? 3
                                    : 1;
      const double c = groups&(1u<<g) ? double(d[9 + g]) / 255 : 0;
      if(groups&(1u<<g))coverage[g] += w * c;
      for (size_t k = 0; k < count; ++k) {
        const auto a = first + k;
        if(!(channels&(1u<<a)))continue;
        const double v = (a < 3 || a >= 6) ? linear(d[a]) : double(d[a]) / 255;
        associated[a] += w * c * v;
        raw[a] += w * v;
      }
    }
    if(read_pbr)for (size_t k = 0; k < 4; ++k) {
      pbr[k] += w * double(d[9]) / 255 * s.pbr[k];
      pbr_raw[k] += w * s.pbr[k];
    }
    if(read_emission) {
      const double c = s.emission.coverage;
      emission_coverage += w * c;
      for (size_t k = 0; k < 3; ++k) {
        legacy[k] += w * c * s.emission.legacy[k];
        physical[k] += w * c * s.emission.physical[k];
        legacy_raw[k] += w * s.emission.legacy[k];
        physical_raw[k] += w * s.emission.physical[k];
      }
    }
    if(read_mask)mask += w * double(d[13]) / 255;
  });
  for (size_t g = 0; g < 4; ++g) {
    const size_t first = g == 0   ? 0
                         : g == 1 ? 4
                         : g == 2 ? 5
                                  : 6,
                 count = g == 0   ? 4
                         : g == 3 ? 3
                                  : 1;
    if(groups&(1u<<g))out.common.data[9 + g] = byte(coverage[g]);
    for (size_t k = 0; k < count; ++k) {
      const size_t a = first + k;
      if(!(channels&(1u<<a)))continue;
      const auto v = coverage[g] > 0 ? associated[a] / coverage[g] : raw[a];
      out.common.data[a] = byte((a < 3 || a >= 6) ? encoded(v) : v);
    }
  }
  if(read_mask)out.common.data[13] = byte(mask);
  if(read_pbr)for (size_t k = 0; k < 4; ++k)
    out.pbr[k] = coverage[0] > 0 ? pbr[k] / coverage[0] : pbr_raw[k];
  if(read_emission) {
    out.emission.coverage = float(std::clamp(emission_coverage, 0., 1.));
    for (size_t k = 0; k < 3; ++k) {
      out.emission.legacy[k] = float(std::clamp(
          emission_coverage > 0 ? legacy[k] / emission_coverage : legacy_raw[k],
          0., 1.));
      out.emission.physical[k] =
          float(std::clamp(emission_coverage > 0 ? physical[k] / emission_coverage
                                                 : physical_raw[k],
                           0., double(maximum_native_emission)));
    }
    out.emission.validate();
  }
  return out;
}

Sample filtered(const PaintLayer &l, uint32_t from, uint32_t to, uint32_t x,
                uint32_t y, TextureSetResolutionReport &report,
                const TextureSetLimits &limits,
                const paint_grid::Layout *selection=nullptr,size_t tap_work=32) {
  return filtered_samples(l,from,[&](auto &&sample) {
    footprint(from,to,x,y,[&](uint32_t sx,uint32_t sy,double weight) {
      work(report,tap_work,limits);
      ++report.filtered_samples;
      if((report.filtered_samples&255)==0)stop(limits);
      sample(sx,sy,weight);
    });
  },selection);
}
void quality(const PaintLayer &before, const PaintLayer &after, uint32_t from,
             uint32_t to, const TextureSetResolutionOptions &options,
             const TextureSetLimits &limits, TextureSetResolutionReport &report,
             TextureSetResolutionLayerQuality &q, bool native_cells,
             const paint_grid::Layout *reduction) {
  const auto reconstruct_x = [from,to](uint32_t x) {
    return from > to ? x / (from / to) : x * (to / from);
  };
  double peak = 1;
  for (const auto &t : before.emission_tiles)
    if (t)
      for (size_t i=0;i<PaintEmissionTile::sample_count;++i) {
        work(report, 1, limits);
        for (float v : t->texel(i).physical)
          peak = std::max(peak, double(v));
      }
  double squares = 0;
  size_t count = 0;
  // Coverage and raw/associated values for common, separate PBR and FLOAT.
  constexpr size_t components = 4 + 2 * 9 + 1 + 2 * 4 + 1 + 4 * 3;
  constexpr std::array<size_t,4> first{0,4,5,6}, width{4,1,1,3};
  constexpr size_t stored_comparison_work = 3;
  constexpr size_t full_comparison_work = 32;
  std::array<bool,4> check_group{true,true,true,true};
  size_t known_components=0;
  if(reduction)for(unsigned g=0;g<4;++g) {
    const auto values=((1u<<width[g])-1)<<first[g];
    check_group[g]=(reduction->coverage_mask&(1u<<g))||
                   (reduction->active&values)||
                   (g==3&&reduction->floats!=paint_grid::FloatPlane::Absent);
    if(!check_group[g])known_components+=1+2*width[g];
  }
  const bool check_mask=!reduction||reduction->mask;
  const bool check_pbr=!reduction||reduction->pbr||check_group[0];
  const bool check_emission=!reduction||check_group[3]||
      reduction->floats!=paint_grid::FloatPlane::Absent;
  if(!check_mask)known_components+=1;
  if(!check_pbr)known_components+=2*4;
  if(!check_emission)known_components+=1+4*3;
  const size_t numeric_work=((full_comparison_work-stored_comparison_work)*
      (components-known_components)+components-1)/components;
  auto measure = [&](double a, double b, double &maximum) {
    const double e = std::abs(a - b);
    maximum = std::max(maximum, e);
    squares += e * e;
    ++count;
  };
  for (uint32_t y = 0; y < from; ++y) {
    stop(limits);
    if (native_cells)
      work(report, from, limits);
    for (uint32_t x = 0; x < from; ++x) {
      const auto original = stored_pixel(before, from, x, y);
      Sample b;
      if (native_cells) {
        // A native cell is constant over its footprint. Check every original
        // value against its new cell, including new contributions at unpainted
        // locations after reduction. Equal stored cells have identical
        // decoded values and contributions, and still count toward RMS.
        const auto tx = reconstruct_x(x), ty = reconstruct_x(y);
        const auto restored = stored_pixel(after, to, tx, ty);
        if (original.empty() && restored.empty()) {
          count += components;
          continue;
        }
        work(report, stored_comparison_work, limits);
        if (original == restored) {
          count += components;
          continue;
        }
        work(report, numeric_work, limits);
        b = pixel(restored,check_pbr,check_emission);
      } else {
        b = filtered(after, to, from, x, y, report, limits);
      }
      const auto a = pixel(original,check_pbr,check_emission);
      const auto &ad = a.common.data;
      const auto &bd = b.common.data;
      // The default lanes were proved from the entire source layer. Their
      // means remain zero, or one for an untouched mask, so retain their
      // sample counts without evaluating a numerical difference per cell.
      count+=known_components;
      for (size_t g = 0; g < 4; ++g) {
        if(!check_group[g])continue;
        const double ac = double(ad[9 + g]) / 255, bc = double(bd[9 + g]) / 255;
        measure(ac, bc, q.contribution_error[9 + g]);
        for (size_t k = 0; k < width[g]; ++k) {
          const auto c = first[g] + k;
          const double av = (c < 3 || c >= 6) ? linear(ad[c])
                                              : double(ad[c]) / 255,
                       bv = (c < 3 || c >= 6) ? linear(bd[c])
                                              : double(bd[c]) / 255;
          measure(av, bv, q.value_error[c]);
          measure(av * ac, bv * bc, q.contribution_error[c]);
        }
      }
      if(check_mask)measure(double(ad[13]) / 255, double(bd[13]) / 255,
                           q.contribution_error[13]);
      if(check_pbr)for (size_t k = 0; k < 4; ++k) {
        measure(a.pbr[k], b.pbr[k], q.pbr_value_error[k]);
        measure(a.pbr[k] * double(ad[9]) / 255, b.pbr[k] * double(bd[9]) / 255,
                q.pbr_error[k]);
      }
      if(!check_emission)continue;
      measure(a.emission.coverage, b.emission.coverage,
              q.emission_relative_error[6]);
      for (size_t k = 0; k < 3; ++k) {
        measure(a.emission.legacy[k], b.emission.legacy[k],
                q.emission_value_error[k]);
        measure(double(a.emission.physical[k]) / peak,
                double(b.emission.physical[k]) / peak,
                q.emission_value_error[3 + k]);
        measure(double(a.emission.legacy[k]) * a.emission.coverage,
                double(b.emission.legacy[k]) * b.emission.coverage,
                q.emission_relative_error[k]);
        const double av = double(a.emission.physical[k]) * a.emission.coverage,
                     bv = double(b.emission.physical[k]) * b.emission.coverage;
        q.physical_absolute_error =
            std::max(q.physical_absolute_error, std::abs(av - bv));
        measure(av / peak, bv / peak, q.emission_relative_error[3 + k]);
      }
    }
  }
  q.rms_error = count ? std::sqrt(squares / count) : 0;
  auto exceeds = [&](const auto &v) {
    return std::any_of(v.begin(), v.end(), [&](double e) {
      return e > options.maximum_component_error;
    });
  };
  if (q.rms_error > options.maximum_rms_error || exceeds(q.value_error) ||
      exceeds(q.contribution_error) || exceeds(q.pbr_error) ||
      exceeds(q.pbr_value_error) || exceeds(q.emission_value_error) ||
      exceeds(q.emission_relative_error))
    throw std::runtime_error("Texture-set resolution cannot meet authored "
                             "layer/coverage/mask/FLOAT reconstruction bounds");
}
void material_quality(const PaintedMesh &before, const PaintedMesh &after,
                      const Material &material,
                      const TextureSetResolutionOptions &options,
                      const TextureSetLimits &limits,
                      TextureSetResolutionReport &report) {
  stop(limits);
  // Ordinary composites intentionally refuse physical emission. Retain their
  // exact legacy filtering, but use the renderer's genuine independent linear
  // packet for FLOAT/HDR fields instead of quantizing or dropping that plane.
  const bool physical = before.has_physical_emission() ||
      std::any_of(material.emissive.begin(), material.emissive.end(),
                  [](float v) { return v > 1; });
  const auto old = physical ? EmissionExportAccess::preview(before, material, limits.cancelled)
                            : before.composite(material, limits.cancelled),
             now = physical ? EmissionExportAccess::preview(after, material, limits.cancelled)
                            : after.composite(material, limits.cancelled);
  const auto r = before.resolution();
  work(report,
       (size_t(r) * r + size_t(after.resolution()) * after.resolution()) *
           (before.layers().size() + 1) * 32,
       limits);
  const std::array<const Image *, 5> left{&old->color, &old->metal_rough,
                                          &old->emission, &old->color_coverage,
                                          &old->emission_coverage};
  const std::array<const Image *, 5> right{&now->color, &now->metal_rough,
                                           &now->emission, &now->color_coverage,
                                           &now->emission_coverage};
  double squares = 0;
  size_t count = 0;
  const auto emission_texel = [&](const PaintOverlayPacket &packet,
                                  uint32_t x, uint32_t y) {
    work(report, (packet.layers().size() + 1) * 32, limits);
    const auto values = sample_overlay_texel_values(packet, int(x), int(y), true);
    std::array<double, 4> result{};
    result[3] = values[3][3];
    for (size_t k = 0; k < 3; ++k)
      result[k] = double(material.emissive[k]) * (1 - result[3]) +
                  values[2][k] + double(values[4][k]);
    return result;
  };
  double emission_peak = 1;
  // Quality reconstruction repeatedly visits the same target texels. Evaluate
  // the immutable native packets once per grid and reuse bounded linear values;
  // this spends no layer evaluations or quantization on repeated samples.
  std::vector<std::array<double, 4>> old_emission_grid, new_emission_grid;
  if (physical) {
    if (!old->overlay || !now->overlay || old->overlay->resolution() != r ||
        now->overlay->resolution() != after.resolution())
      throw std::invalid_argument("Material quality lost actual native emission grids");
    old_emission_grid.resize(size_t(r) * r);
    const auto nr = after.resolution();
    new_emission_grid.resize(size_t(nr) * nr);
    for (uint32_t y = 0; y < r; ++y) {
      stop(limits);
      for (uint32_t x = 0; x < r; ++x) {
        auto &v = old_emission_grid[size_t(y) * r + x];
        v = emission_texel(*old->overlay, x, y);
        for (size_t k = 0; k < 3; ++k)
          emission_peak = std::max(emission_peak, v[k]);
      }
    }
    for (uint32_t y = 0; y < nr; ++y) {
      stop(limits);
      for (uint32_t x = 0; x < nr; ++x)
        new_emission_grid[size_t(y) * nr + x] = emission_texel(*now->overlay, x, y);
    }
  }
  const auto sample = [&](const Image &im, uint32_t x, uint32_t y,
                          size_t channel, bool srgb) {
    if (!im.width || im.width != im.height || (im.width & (im.width - 1)) ||
        im.rgba.size() != size_t(im.width) * im.height * 4)
      throw std::invalid_argument(
          "Material reconstruction lost its actual square field image");
    double value = 0;
    footprint(im.width, r, x, y, [&](uint32_t sx, uint32_t sy, double w) {
      work(report, 1, limits);
      const auto b = im.rgba[(size_t(sy) * im.width + sx) * 4 + channel];
      value += w * (srgb ? linear(b) : double(b) / 255);
    });
    return value;
  };
  for (uint32_t y = 0; y < r; ++y) {
    stop(limits);
    for (uint32_t x = 0; x < r; ++x) {
      std::array<double, 4> old_emission{}, new_emission{};
      if (physical) {
        old_emission = old_emission_grid[size_t(y) * r + x];
        footprint(after.resolution(), r, x, y,
                  [&](uint32_t sx, uint32_t sy, double weight) {
          work(report, 4, limits);
          const auto &v = new_emission_grid[size_t(sy) * after.resolution() + sx];
          for (size_t k = 0; k < 4; ++k)
            new_emission[k] += weight * v[k];
        });
      }
      size_t component = 0;
      for (size_t image = 0; image < 5; ++image) {
        const size_t n = image < 3 ? 4 : 1;
        for (size_t k = 0; k < n; ++k) {
          const bool srgb = (image == 0 || image == 2) && k < 3;
          const bool linear_emission = physical && (image == 2 || image == 4);
          const auto channel = image == 4 ? size_t(3) : k;
          const auto scale = channel < 3 ? emission_peak : 1;
          const auto e = linear_emission
              ? std::abs(old_emission[channel] - new_emission[channel]) / scale
              : std::abs(sample(*left[image], x, y, k, srgb) -
                         sample(*right[image], x, y, k, srgb));
          report.material_error[component] =
              std::max(report.material_error[component], e);
          squares += e * e;
          ++component;
          ++count;
        }
      }
    }
  }
  report.material_rms_error = count ? std::sqrt(squares / count) : 0;
  if (report.material_rms_error > options.maximum_rms_error ||
      std::any_of(
          report.material_error.begin(), report.material_error.end(),
          [&](double e) { return e > options.maximum_component_error; }))
    throw std::runtime_error(
        "Texture-set resolution cannot preserve the actual layered "
        "material/mask/alpha reconstruction within its quality bounds");
}

void native_overlay_quality(const PaintedMesh &before, const PaintedMesh &after,
                            const TextureSetResolutionOptions &options,
                            const TextureSetLimits &limits,
                            TextureSetResolutionReport &report, bool pbr = false) {
  const auto old = PaintOverlayFactory::capture(before, limits.cancelled);
  const auto now = PaintOverlayFactory::capture(after, limits.cancelled);
  // Compare the actual composed fields on their common coarser footprints.
  // Layer quality separately checks every authored value on the old grid.
  const auto &coarse = before.resolution() < after.resolution() ? before : after;
  const auto r = coarse.resolution();
  double emission_peak = 1, squares = 0;
  size_t count = 0;
  if (old->baked() && old->baked()->emission_field) {
    const auto &field = *old->baked()->emission_field;
    work(report, field.sample_count(), limits);
    for (size_t i = 0; i < field.sample_count(); ++i) {
      if ((i & 4095) == 0) stop(limits);
      for (float value : field.texel(i).absolute)
        emission_peak = std::max(emission_peak, double(value));
    }
  }
  for (const auto &layer : old->layers())
    for (const auto &tile : layer.emission_tiles)
      if (tile) {
        stop(limits);
        work(report, PaintEmissionTile::sample_count, limits);
        for (size_t i=0;i<PaintEmissionTile::sample_count;++i)
          for (float value : tile->texel(i).physical)
            emission_peak = std::max(emission_peak, double(value));
      }
  auto visible_tiles = [&](const PaintOverlayPacket &packet) {
    const auto stride = packet.resolution() / PaintTile::size;
    std::vector<uint8_t> tiles(size_t(stride) * stride);
    charge(report.peak_bytes, tiles.size(), sizeof(uint8_t), limits);
    for (const auto &layer : packet.layers()) {
      stop(limits);
      work(report, tiles.size(), limits);
      for (size_t tile = 0; tile < tiles.size(); ++tile)
        tiles[tile] |= bool(layer.tiles.at(tile));
    }
    return tiles;
  };
  const auto old_tiles = visible_tiles(*old);
  const auto new_tiles = visible_tiles(*now);
  constexpr std::array<size_t, 5> widths{4, 3, 3, 4, 3};
  constexpr std::array<size_t, 5> offsets{0, 4, 7, 10, 7};
  for (uint32_t y = 0; y < r; ++y) {
    stop(limits);
    work(report, r, limits);
    for (uint32_t x = 0; x < r; ++x) {
      if (!TextureSetPaintAccess::grid_covered(coarse, size_t(y) * r + x))
        continue;
      auto sample_footprint = [&](const PaintOverlayPacket &packet, const auto &tiles) {
        std::array<std::array<double, 4>, 5> values{};
        const auto grid = packet.resolution();
        footprint(grid, r, x, y, [&](uint32_t sx, uint32_t sy, double weight) {
          work(report, 1, limits);
          const auto owner = packet.padding()->at(size_t(sy) * grid + sx);
          if (owner < 0 || (!packet.baked() && !tiles.at(size_t(owner / grid / 32) * (grid / 32) + owner % grid / 32)))
            return;
          const auto sample = sample_overlay_texel_owner(packet, size_t(owner), pbr);
          work(report, sample.work, limits);
          for (size_t plane = 0; plane < widths.size(); ++plane)
            for (size_t c = 0; c < widths[plane]; ++c)
              values[plane][c] += weight * sample.values[plane][c];
        });
        return values;
      };
      const auto original = sample_footprint(*old, old_tiles);
      const auto restored = sample_footprint(*now, new_tiles);
      for (size_t plane = 0; plane < widths.size(); ++plane)
        for (size_t c = 0; c < widths[plane]; ++c) {
          const auto error = std::abs(restored[plane][c] - original[plane][c]) /
                             (plane == 4 ? emission_peak : 1.);
          auto &maximum = report.material_error[offsets[plane] + c];
          maximum = std::max(maximum, error);
          squares += error * error;
          ++count;
        }
    }
  }
  if (count)
    report.material_rms_error = std::max(report.material_rms_error, std::sqrt(squares / count));
  if (report.material_rms_error > options.maximum_rms_error ||
      std::ranges::any_of(report.material_error, [&](double error) {
        return error > options.maximum_component_error;
      }))
    throw std::runtime_error("Native resolution cannot preserve the layered paint overlay within its reconstruction bounds");
}
} // namespace
std::array<PaintBilinearTap,4> paint_bilinear_taps(uint32_t resolution,
    std::array<float,2> uv,std::span<const int32_t> padding,bool require_padding_owner) {
  const double x = std::clamp(double(uv[0]),0.,1.)*resolution-.5;
  const double y = std::clamp(double(uv[1]),0.,1.)*resolution-.5;
  const int ix = int(std::floor(x)), iy = int(std::floor(y));
  const double fx = x-ix, fy = y-iy;
  std::array<PaintBilinearTap,4> taps;
  for(size_t i=0;i<taps.size();++i){
    const auto sx = std::clamp(ix+int(i&1),0,int(resolution)-1);
    const auto sy = std::clamp(iy+int(i>>1),0,int(resolution)-1);
    const auto texel = size_t(sy)*resolution+sx;
    const auto owner = padding[texel];
    const double weight = (i&1?fx:1-fx)*(i&2?fy:1-fy);
    taps[i] = {owner>=0?size_t(owner):texel,owner<0&&require_padding_owner?0:weight};
  }
  return taps;
}

FilteredPaintTexel filter_paint_layer(const PaintLayer &layer,uint32_t resolution,
    const std::array<PaintBilinearTap,4> &taps) {
  const auto source = filtered_samples(layer,resolution,[&](auto &&sample) {
    for(const auto &tap:taps)
      if(tap.weight!=0)sample(uint32_t(tap.texel%resolution),uint32_t(tap.texel/resolution),tap.weight);
  });
  FilteredPaintTexel result{source.common,source.emission};
  result.pbr_present = source.pbr_present;
  if (!layer.pbr_tiles.empty())
    for (size_t k = 0; k < result.pbr.size(); ++k)
      result.pbr[k] = byte(k < 3 ? encoded(source.pbr[k]) : source.pbr[k]);
  return result;
}

std::vector<PaintLayer> resample_texture_set_layers(
    const PaintedMesh &before, const TextureSetResolutionOptions &options,
    const TextureSetLimits &limits, TextureSetResolutionReport &report,
    bool native_cells) {
  options.validate();
  limits.validate();
  stop(limits);
  const auto from = before.resolution(), to = options.resolution;
  // A larger native grid can retain whole authored cells, including masked
  // values and unassociated colour. The separate overlay check still
  // certifies the effect of rebuilding the geometry's writable raster.
  const uint32_t subdivision = native_cells && to > from ? to / from : 1;
  const bool accelerated=subdivision==1&&from!=to&&compute_backend()!="CPU";
  const size_t tiles = size_t(to / 32) * (to / 32);
  // Old field plus new raster constructor/padding, dense worst-case authored
  // output and one writable tile of scratch coexist. No dense per-layer image.
  size_t bytes = TextureSetPaintAccess::working_bytes(before, to);
  charge(bytes, 1, before.storage_bytes(), limits);
  charge(bytes, 1, TextureSetPaintAccess::initial_raster_bytes(from), limits);
  const auto &geometry = before.mesh();
  charge(bytes, geometry.positions.size() + geometry.normals.size(),
         sizeof(Vec3), limits);
  charge(bytes, geometry.uv.size(), sizeof(std::array<float, 2>), limits);
  charge(bytes, geometry.indices.size(), sizeof(uint32_t), limits);
  charge(bytes, 3, TextureSetPaintAccess::snapshot_bytes(before), limits);
  const size_t largest = size_t(std::max(from, to)) * std::max(from, to);
  if (!native_cells && before.layers().size() &&
      largest > (limits.maximum_work - report.work) / before.layers().size() / 160)
    throw std::length_error("Texture-set resolution filtering/quality work "
                            "exceeds its prospective bound");
  charge(bytes, 1,
         sizeof(PaintTile) + sizeof(PaintColorTile) + sizeof(PaintEmissionTile::Pixels),
         limits);
  for (const auto &layer : before.layers()) {
    const bool pbr = !layer.pbr_tiles.empty(),
               emission = !layer.emission_tiles.empty();
    size_t affected = tiles;
    if (native_cells) {
      affected = 0;
      for (uint32_t y = 0; y < to / 32; ++y) {
        stop(limits);
        for (uint32_t x = 0; x < to / 32; ++x)
          affected += has_authored_footprint(layer, from, to, x, y);
      }
      work(report, std::max(tiles, size_t(from / 32) * (from / 32)), limits);
    }
    charge(bytes, affected,
           sizeof(PaintTile) + (pbr ? sizeof(PaintColorTile) : 0) +
               (emission ? PaintEmissionTile::maximum_storage_bytes() : 0),
           limits);
    charge(bytes, tiles,
           (1 + size_t(pbr) + size_t(emission)) *
               sizeof(std::shared_ptr<const PaintTile>),
           limits);
  }
  const auto authored=[](const PaintLayer &layer){
    for(size_t t=0;t<layer.tiles.size();++t)
      if(has_authored_tile(layer,t))return true;
    return false;
  };
  std::unique_ptr<GridFilter> filter;
  if(accelerated&&std::ranges::any_of(before.layers(),authored)) {
    charge(bytes,1,GridFilter::peak_bytes,limits);
    filter=std::make_unique<GridFilter>();
  }
  report.peak_bytes = bytes;
  std::vector<PaintLayer> result;
  result.reserve(before.layers().size());
  report.layers.reserve(before.layers().size());
  for (const auto &layer : before.layers()) {
    stop(limits);
    PaintLayer out;
    out.id = layer.id;
    out.name = layer.name;
    out.visible = layer.visible;
    out.opacity = layer.opacity;
    out.tiles.resize(tiles);
    if (!layer.pbr_tiles.empty())
      out.pbr_tiles.resize(tiles);
    if (!layer.emission_tiles.empty())
      out.emission_tiles.resize(tiles);
    const bool layer_filter=filter&&authored(layer);
    auto layout=layer_layout(layer);
    const bool prune=native_cells&&from>to;
    if(prune)omit_default_reduction_channels(layer,layout,report,limits);
    const size_t tap_work=prune?reduction_tap_work(layout):32;
    for (uint32_t ty = 0; ty < to / 32; ++ty)
      for (uint32_t tx = 0; tx < to / 32; ++tx) {
        stop(limits);
        if (native_cells && !has_authored_footprint(layer, from, to, tx, ty))
          continue;
        const PaintTile *exact_common = nullptr;
        const PaintColorTile *exact_pbr = nullptr;
        const PaintEmissionTile *exact_emission = nullptr;
        if (subdivision > 1) {
          const auto source_tile = size_t(ty / subdivision) * (from / 32) + tx / subdivision;
          exact_common = layer.tiles[source_tile].get();
          if (!layer.pbr_tiles.empty())
            exact_pbr = layer.pbr_tiles[source_tile].get();
          if (!layer.emission_tiles.empty())
            exact_emission = layer.emission_tiles[source_tile].get();
        }
        PaintTile common;
        PaintColorTile pbr;
        PaintEmissionTile::Pixels emission;
        bool has_common = false, has_pbr = false, has_emission = false;
        const auto store=[&](size_t i,PaintPixel c,PaintEmissionValue light) {
          // Any tile owning FLOAT also needs its matching legacy fallback.
          if (!layer.emission_tiles.empty()) {
            c.data[12]=fallback_byte(light.coverage);
            for(size_t k=0;k<3;++k)
              c.data[6+k]=fallback_byte(linear_to_srgb(light.legacy[k]));
          }
          common.pixels[i]=c;
          emission[i]=light;
        };
        if(layer_filter) {
          const auto taps=from>to?size_t(from/to)*(from/to):4;
          work(report,tap_work*1024*taps,limits);
          report.filtered_samples+=1024*taps;
          filter->apply(from,to,{tx*32,ty*32,32,32},layout,limits.cancelled,
            [&](uint32_t x,uint32_t y){return grid_layer_pixel(layer,from,x,y);},
            [&](uint32_t x,uint32_t y,const paint_grid::Pixel &p){
              const size_t i=size_t(y)*32+x;
              PaintPixel c;
              std::copy_n(p.values,9,c.data.data());
              std::copy_n(p.coverage,4,c.data.data()+9);
              c.data[13]=p.mask;
              PaintEmissionValue light;
              if(layout.floats!=paint_grid::FloatPlane::Absent) {
                for(size_t k=0;k<3;++k) {
                  light.legacy[k]=float(std::clamp(number(p.emission[k]),0.,1.));
                  light.physical[k]=float(std::clamp(number(p.emission[k+3]),0.,double(maximum_native_emission)));
                }
                light.coverage=float(std::clamp(number(p.emission[6]),0.,1.));
                light.validate();
              }
              has_common|=bool(p.present&1);
              has_pbr|=bool(p.present&2);
              has_emission|=bool(p.present&4);
              if(layout.pbr)std::copy_n(p.values+9,4,pbr.pixels[i].data());
              store(i,c,light);
            });
        }else if(!accelerated)for (uint32_t y = 0; y < 32; ++y)
          for (uint32_t x = 0; x < 32; ++x) {
            const auto grid_x = tx * 32 + x, grid_y = ty * 32 + y;
            const auto i = size_t(y) * 32 + x;
            if (subdivision > 1) {
              work(report, 1, limits);
              ++report.filtered_samples;
              const auto source_pixel =
                size_t((grid_y / subdivision) % 32) * 32 + (grid_x / subdivision) % 32;
              if (exact_common)
                common.pixels[i] = exact_common->pixels[source_pixel];
              if (exact_pbr)
                pbr.pixels[i] = exact_pbr->pixels[source_pixel];
              if (exact_emission)
                emission[i] = exact_emission->texel(source_pixel);
              continue;
            }
            auto s = filtered(layer, from, to, grid_x, grid_y, report, limits,
                              prune?&layout:nullptr,tap_work);
            has_common |= s.common_present;
            has_pbr |= s.pbr_present;
            has_emission |= s.emission_present;
            store(i,s.common,s.emission);
            if(layout.pbr)for (size_t k = 0; k < 4; ++k)
              pbr.pixels[i][k] = byte(k < 3 ? encoded(s.pbr[k]) : s.pbr[k]);
          }
        const auto t = size_t(ty) * (to / 32) + tx;
        if (exact_common || has_common || has_pbr || has_emission) {
          out.tiles[t] = std::make_shared<const PaintTile>(std::move(common));
          ++report.output_tiles;
        }
        if (exact_pbr || has_pbr)
          out.pbr_tiles[t] =
              std::make_shared<const PaintColorTile>(std::move(pbr));
        if (exact_emission || has_emission)
          out.emission_tiles[t] =
              PaintEmissionTile::pack(emission);
      }
    TextureSetResolutionLayerQuality q;
    q.layer = layer.id;
    quality(layer, out, from, to, options, limits, report, q, native_cells,
            prune?&layout:nullptr);
    report.layers.push_back(std::move(q));
    result.push_back(std::move(out));
  }
  stop(limits);
  return result;
}

std::shared_ptr<const BakedTextures> resample_native_bake_grid(
    const BakedTextures &before,uint32_t from,const TextureSetResolutionOptions &options,
    const TextureSetLimits &limits,TextureSetResolutionReport &report,bool retained_native_uv,
    bool independent_normal_grid) {
  stop(limits);
  before.validate(from, independent_normal_grid);
  if(!independent_normal_grid &&
     !(retained_native_uv?resizable_native_uv_bake(&before):resizable_canonical_bake(&before)))
    throw std::invalid_argument("Paint resolution requires its supported native Bake grid");
  const auto to=options.resolution;
  const size_t source_pixels=size_t(from)*from,target_pixels=size_t(to)*to;
  const bool enlarge=to>=from;
  auto after=std::make_shared<BakedTextures>();
  after->color_interpretation=before.color_interpretation;
  after->normal_interpretation=before.normal_interpretation;
  after->normal_field=before.normal_field;
  after->maps.transparent=before.maps.transparent;
  const std::array old_images{&before.maps.color,&before.maps.pbr_color,&before.maps.metal_rough,
                             &before.maps.normal,&before.maps.emission};
  const std::array new_images{&after->maps.color,&after->maps.pbr_color,&after->maps.metal_rough,
                             &after->maps.normal,&after->maps.emission};
  const auto planes=std::ranges::count_if(old_images,[](const auto *image){return !image->rgba.empty();});
  charge(report.peak_bytes,1,sizeof(BakedTextures),limits);
  charge(report.peak_bytes,target_pixels,1+4*size_t(planes),limits);
  work(report,std::max(source_pixels,target_pixels)*(1+4*size_t(planes)),limits);
  for(size_t role=0;role<old_images.size();++role)
    if(!old_images[role]->rgba.empty())*new_images[role]={to,to,std::vector<uint8_t>(4*target_pixels)};
  after->coverage.resize(target_pixels);
  // Each image carries unassociated material values and one shared linear
  // mask. Use mask-weighted linear values when reducing. Normal RGB stores a
  // signed tangent vector, and the native shader normalizes after blending.
  const auto srgb=[](size_t role,size_t k){return (role<2||role==4)&&k<3;};
  const auto decode=[&](uint8_t v,size_t role,size_t k){return srgb(role,k)?linear(v):v/255.;};
  const auto prior_cell=[&](uint32_t x,uint32_t y){return size_t(y/(to/from))*from+x/(to/from);};
  const bool accelerated=!enlarge&&compute_backend()!="CPU";
  std::vector<BakedEmissionSample> device_emission;
  if(accelerated) {
    charge(report.peak_bytes,1,GridFilter::peak_bytes,limits);
    paint_grid::Layout layout;
    for(size_t role=0;role<old_images.size();++role)if(!old_images[role]->rgba.empty()) {
      layout.active|=15u<<(4*role);
      if(srgb(role,0))layout.srgb|=7u<<(4*role);
    }
    const auto &emission=before.emission_field;
    if(emission) {
      layout.floats=paint_grid::FloatPlane::BakedCoefficients;
      charge(report.peak_bytes,target_pixels,sizeof(BakedEmissionSample),limits);
      work(report,8*source_pixels,limits);
      device_emission.resize(target_pixels);
    }
    GridFilter filter;
    filter.apply(from,to,{0,0,to,to},layout,limits.cancelled,
      [&](uint32_t x,uint32_t y){
        const size_t src=size_t(y)*from+x;
        paint_grid::Pixel p;
        p.coverage[0]=before.coverage[src];
        for(size_t role=0;role<old_images.size();++role)if(!old_images[role]->rgba.empty())
          std::copy_n(old_images[role]->rgba.data()+4*src,4,p.values+4*role);
        if(emission) {
          const auto sample=emission->texel(src);
          for(unsigned c=0;c<3;++c) {
            p.emission[c]=word(sample.legacy[c]);
            p.emission[3+c]=word(sample.absolute[c]);
          }
          p.emission[6]=word(sample.coverage);
        }
        return p;
      },[&](uint32_t x,uint32_t y,const paint_grid::Pixel &p){
        const size_t dst=size_t(y)*to+x;
        after->coverage[dst]=p.coverage[0];
        for(size_t role=0;role<new_images.size();++role)if(!new_images[role]->rgba.empty())
          std::copy_n(p.values+4*role,4,new_images[role]->rgba.data()+4*dst);
        if(emission) {
          auto &sample=device_emission[dst];
          for(unsigned c=0;c<3;++c) {
            sample.legacy[c]=float(number(p.emission[c]));
            sample.absolute[c]=float(number(p.emission[3+c]));
          }
          sample.coverage=float(number(p.emission[6]));
        }
      });
  }else for(uint32_t y=0;y<to;++y){
    stop(limits);
    for(uint32_t x=0;x<to;++x){
      const size_t dst=size_t(y)*to+x;
      if(enlarge){
        const auto src=prior_cell(x,y);
        after->coverage[dst]=before.coverage[src];
        for(size_t role=0;role<old_images.size();++role)if(!old_images[role]->rgba.empty())
          std::copy_n(old_images[role]->rgba.data()+src*4,4,new_images[role]->rgba.data()+dst*4);
        continue;
      }
      double coverage=0;
      std::array<std::array<double,4>,5> visible{},raw{};
      footprint(from,to,x,y,[&](uint32_t sx,uint32_t sy,double area){
        const size_t src=size_t(sy)*from+sx;
        const double weight=before.coverage[src]/255.*area;
        coverage+=weight;
        for(size_t role=0;role<old_images.size();++role)if(!old_images[role]->rgba.empty())
          for(size_t k=0;k<4;++k){
            const double value=decode(old_images[role]->rgba[src*4+k],role,k);
            visible[role][k]+=value*weight;
            raw[role][k]+=value*area;
          }
      });
      after->coverage[dst]=byte(coverage);
      for(size_t role=0;role<new_images.size();++role)if(!new_images[role]->rgba.empty())
        for(size_t k=0;k<4;++k){
          const auto value=coverage>0?visible[role][k]/coverage:raw[role][k];
          new_images[role]->rgba[dst*4+k]=byte(srgb(role,k)?encoded(value):value);
        }
    }
  }
  if(const auto &emission=before.emission_field){
    after->emission_field=BakedEmissionField::generate_compact(to,[&](size_t i){
      if(accelerated)return device_emission[i];
      const auto x=uint32_t(i%to),y=uint32_t(i/to);
      if(enlarge){work(report,1,limits);return emission->texel(prior_cell(x,y));}
      std::array<double,3> legacy{},absolute{};
      double coverage=0;
      footprint(from,to,x,y,[&](uint32_t sx,uint32_t sy,double area){
        work(report,8,limits);
        const auto sample=emission->texel(size_t(sy)*from+sx);
        coverage+=sample.coverage*area;
        for(size_t k=0;k<3;++k){
          legacy[k]+=sample.legacy[k]*area;
          absolute[k]+=sample.absolute[k]*area;
        }
      });
      return BakedEmissionSample{{float(legacy[0]),float(legacy[1]),float(legacy[2])},
          {float(absolute[0]),float(absolute[1]),float(absolute[2])},float(coverage)};
    },{std::min(size_t(128*1024*1024),limits.maximum_bytes-report.peak_bytes),limits.cancelled});
    charge(report.peak_bytes,1,after->emission_field->storage_bytes(),limits);
  }
  after->validate(to, independent_normal_grid);
  if(enlarge)return after;

  // Compare unflattened Bake contributions, including hidden values that
  // the live paint stack can cover. Inspect every original cell so averaging
  // can neither conceal a sharp normal edge nor erase significant HDR detail.
  std::array<double,3> peak{1,1,1};
  if(before.emission_field){
    work(report,source_pixels*4,limits);
    for(size_t i=0;i<source_pixels;++i){
      if((i&4095)==0)stop(limits);
      const auto sample=before.emission_field->texel(i);
      for(size_t k=0;k<3;++k)peak[k]=std::max(peak[k],double(sample.absolute[k]));
    }
  }
  work(report,source_pixels*(1+4*size_t(planes)+(before.emission_field?7:0)),limits);
  double squares=0;
  size_t count=0;
  auto measure=[&](double error){
    if(error>options.maximum_component_error)
      throw std::runtime_error("Paint resolution cannot preserve the baked material, normals and emission within its quality bounds");
    squares+=error*error;
    ++count;
  };
  const auto scale=from/to;
  for(uint32_t y=0;y<from;++y){
    stop(limits);
    for(uint32_t x=0;x<from;++x){
      const size_t src=size_t(y)*from+x,dst=size_t(y/scale)*to+x/scale;
      const double a=before.coverage[src]/255.,b=after->coverage[dst]/255.;
      measure(std::abs(a-b));
      for(size_t role=0;role<old_images.size();++role)if(!old_images[role]->rgba.empty())
        for(size_t k=0;k<4;++k){
          const auto old=decode(old_images[role]->rgba[src*4+k],role,k);
          const auto now=decode(new_images[role]->rgba[dst*4+k],role,k);
          measure(std::abs(role==3&&k<3?a*(2*old-1)-b*(2*now-1):a*old-b*now));
        }
      if(before.emission_field){
        const auto old=before.emission_field->texel(src),now=after->emission_field->texel(dst);
        measure(std::abs(double(old.coverage)-now.coverage));
        for(size_t k=0;k<3;++k){
          measure(std::abs(double(old.legacy[k])-now.legacy[k]));
          measure(std::abs(double(old.absolute[k])-now.absolute[k])/peak[k]);
        }
      }
    }
  }
  if(std::sqrt(squares/count)>options.maximum_rms_error)
    throw std::runtime_error("Paint resolution cannot preserve the baked material, normals and emission within its quality bounds");
  return after;
}
} // namespace detail
TextureSetResolutionReport resample_native_texture_grid(
    Scene &scene, uint64_t object, const TextureSetResolutionOptions &options,
    const TextureSetLimits &limits) {
  options.validate();
  limits.validate();
  detail::stop(limits);
  const auto &before = scene.object(object);
  if (!detail::can_resize_native_texture_grid(before))
    throw std::invalid_argument("Resolution editing needs an actual writable native grid");
  TextureSetResolutionReport report;
  report.original_resolution = before.painted->resolution();
  report.resolution = options.resolution;
  if (report.original_resolution == report.resolution)
    return report;
  const bool empty = detail::TextureSetPaintAccess::can_resize_unpainted(*before.painted);
  const auto retained = before.material_bindings ? before.material_bindings->native_paint_sampling : nullptr;
  if (retained) {
    PaintSamplingLimits policy;
    policy.cancelled=limits.cancelled;
    policy.max_work=limits.maximum_work;
    policy.max_working_bytes=limits.maximum_bytes;
    retained->validate_current(*before.painted,policy);
  }
  const auto resized = empty
      ? detail::TextureSetPaintAccess::resize_unpainted(*before.painted, options.resolution, limits)
      : detail::TextureSetPaintAccess::resample_plain(*before.painted, options, limits, report,false,retained.get());
  if (resized->mesh_owner() != before.painted->mesh_owner() ||
      resized->source_maps() != before.painted->source_maps() ||
      resized->paint_sampling_attachment() != before.painted->paint_sampling_attachment() ||
      !resized->valid_source_sampling_attachment() ||
      !resized->valid_paint_sampling_attachment())
    throw std::invalid_argument("Native resolution lost its immutable mesh or image authority");
  if (!empty) {
    detail::charge(report.peak_bytes, 1, detail::TextureSetPaintAccess::snapshot_bytes(*before.painted), limits);
    detail::charge(report.peak_bytes, 1, detail::TextureSetPaintAccess::snapshot_bytes(*resized), limits);
    detail::native_overlay_quality(*before.painted, *resized, options, limits, report);
  }
  auto next = scene.snapshot();
  next.object(object).painted = resized;
  next.validate();
  detail::stop(limits);
  scene = std::move(next);
  report.changed = true;
  return report;
}
TextureSetResolutionReport
resample_texture_set(Scene &scene, uint64_t object, TextureSetId set,
                     const TextureSetResolutionOptions &options,
                     const TextureSetLimits &limits) {
  options.validate();
  limits.validate();
  detail::validate_texture_set_graph(scene, limits);
  const auto &object_before = scene.object(object);
  if (!object_before.texture_sets)
    throw std::invalid_argument(
        "Resolution editing needs an actual material-set collection");
  object_before.texture_sets->validate(object_before, true, limits);
  const auto field = texture_set_view(object_before, set);
  const bool retained = detail::can_resize_retained_texture_grid(field);
  const bool typed = detail::native_texture_set_mtoon(
      field.painted.get(), field.appearance.get(), field.material_bindings.get());
  if (!retained && (field.source || (!typed && (field.material_bindings || field.painted->source_maps())) ||
      field.appearance ||
      field.painted->paint_sampling_domain() ||
      field.painted->source_sampling_prepared() ||
      field.painted->color_projection() || field.painted->normal_projection() ||
      !detail::resizable_canonical_bake(field.painted->baked().get())))
    throw std::invalid_argument("Source or independently prepared field resolution needs "
                                "its authenticated writable-grid transition");
  TextureSetResolutionReport report;
  report.original_resolution = field.painted->resolution();
  report.resolution = options.resolution;
  if (report.original_resolution == report.resolution) {
    detail::stop(limits);
    return report;
  }
  const bool separate_roles = typed || retained;
  const size_t before_pixels = size_t(field.painted->resolution()) *
                               field.painted->resolution(),
               after_pixels = size_t(options.resolution) * options.resolution;
  // Native previews retain sparse packets and may detach common emission
  // tiles. Reserve both full grids and every layer before any preview capture.
  const size_t layer_count = field.painted->layers().size();
  const bool physical = field.painted->has_physical_emission() ||
      std::any_of(field.material.emissive.begin(), field.material.emissive.end(),
                  [](float v) { return v > 1; });
  const size_t material_bytes = separate_roles ? 0 : (before_pixels + after_pixels) *
                               (128 + (physical ? layer_count * 32 + sizeof(std::array<double, 4>) : 0)) +
      (physical ? 2 * sizeof(std::vector<std::array<double, 4>>) : 0);
  const size_t material_work = separate_roles ? 0 :
      (before_pixels + after_pixels) * (field.painted->layers().size() + 1) *
          32 +
      14 * (before_pixels + std::max(after_pixels, 4 * before_pixels)) +
      // Each native layer/texel is evaluated once into the bounded caches.
      // The original peak uses that same evaluation. Reconstruction reads four
      // cached components per footprint, never reevaluating the layers.
      (physical ? 32 * (layer_count + 1) * (before_pixels + after_pixels) +
                      4 * std::max(after_pixels, 4 * before_pixels)
                : 0);
  if (material_bytes >= limits.maximum_bytes ||
      material_work >= limits.maximum_work)
    throw std::length_error("Material reconstruction exceeds the resolution "
                            "operation's prospective byte/work budget");
  auto transfer_limits = limits;
  transfer_limits.maximum_bytes -= material_bytes;
  transfer_limits.maximum_work -= material_work;
  report.changed = edit_texture_set(
      scene, object, set,
      [&](PaintedMesh &paint, Material &material) {
        const auto resized = detail::TextureSetPaintAccess::resample_plain(
            paint, options, transfer_limits, report, typed, nullptr,
            retained ? field.source.get() : nullptr);
        if (separate_roles) {
          // The original MToon and PBR role images still sample the same UVs.
          // Certify the independently composed paint and coverage fields.
          if (resized->source_maps() != paint.source_maps())
            throw std::invalid_argument("Texture-set resolution changed its original image owner");
          detail::charge(report.peak_bytes, 2, detail::TextureSetPaintAccess::snapshot_bytes(paint), limits);
          detail::charge(report.peak_bytes, 2, detail::TextureSetPaintAccess::snapshot_bytes(*resized), limits);
          detail::native_overlay_quality(paint, *resized, options, limits, report);
          if ((paint.baked() && paint.baked()->color_interpretation == BakedColorInterpretation::ResolvedPbr) ||
              std::ranges::any_of(paint.layers(), [](const auto &layer) { return !layer.pbr_tiles.empty(); }))
            detail::native_overlay_quality(paint, *resized, options, limits, report, true);
        } else {
          report.peak_bytes += material_bytes;
          detail::material_quality(paint, *resized, material, options, limits, report);
        }
        paint = *resized;
        return true;
      },
      limits);
  return report;
}
} // namespace dome
