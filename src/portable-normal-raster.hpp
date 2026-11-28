// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "normal-bake-encoding.hpp"
#include <functional>
#include <memory>
#include <span>
#include <type_traits>

namespace dome { struct Image; class AffineTransform; }
namespace dome::detail {
class PolygonSurfacePreview;
namespace portable_normal {
struct SourceFrame {
  NormalBakeVertex vertices[3];
  NormalBakeBasis before,current;
  float handedness=1;
  bool native=false;
};
struct Chart {
  SourceFrame source;
  NormalBakeVertex destination[3];
};
struct Sample { float barycentric[3],uv[2]; uint32_t chart=0; };
struct Probe { Sample source; float emitted_uv[2]; };
enum class Verdict : uint8_t { Reference, Pass, Fail };
inline constexpr size_t batch_pixels=8192,batch_charts=128;
inline constexpr uint32_t needs_reference=0;
template<class Input,class Output>
inline void validate_batch(std::span<const Chart> charts,std::span<const Input> samples,std::span<Output> result){
  if(charts.empty()||charts.size()>batch_charts||samples.size()!=result.size()||samples.empty()||samples.size()>batch_pixels)
    throw std::invalid_argument("Invalid portable normal raster batch");
  for(const auto &sample:samples){
    const auto chart=[&]{
      if constexpr(std::is_same_v<Input,Sample>)return sample.chart;
      else return sample.source.chart;
    }();
    if(chart>=charts.size())throw std::out_of_range("Portable normal sample has no chart in its batch");
  }
}

SourceFrame source_frame(const PolygonSurfacePreview &,size_t,const AffineTransform &);

class Raster {
public:
  virtual ~Raster()=default;
  virtual void encode(std::span<const Chart>,std::span<const Sample>,std::span<uint32_t>,
      const std::function<bool()> &cancelled)=0;
  virtual void set_emitted_image(const Image &,const std::function<bool()> &cancelled)=0;
  virtual void certify(std::span<const Chart>,std::span<const Probe>,std::span<Verdict>,
      const std::function<bool()> &cancelled)=0;
};
std::unique_ptr<Raster> upload(const Image &,const std::function<bool()> &cancelled);
}
}
