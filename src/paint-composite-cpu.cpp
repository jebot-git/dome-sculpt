// SPDX-License-Identifier: GPL-3.0-or-later
#include "paint-composite-backend.hpp"
#include "paint-grid-filter.hpp"
namespace dome::paint_compute {
Result composite(const Input &in,const std::function<bool()> &cancelled){
 validate(in,cancelled);const auto &q=quantization();
 Result out;
 out.pixels.resize(in.padding.size());
 if(!in.bake.normal.empty())out.normals.resize(4*in.padding.size());
 if(!in.projected_color.empty())out.pbr_colors.resize(4*in.padding.size());
 const auto bake=in.bake.view();
 const PreparedColor prepared{in.projected_color.data(),in.baked_pbr_color.data(),
                              in.color_tiles.empty()?nullptr:in.color_tiles.data(),in.color_pixels.data()};
 for(size_t i=0;i<out.pixels.size();++i){
  if((i&4095)==0)checkpoint(cancelled);
  auto *pbr=out.pbr_colors.empty()?nullptr:out.pbr_colors.data()+4*i;
  out.pixels[i]=compose(uint32_t(i),in.parameters,in.layers.data(),in.tiles.data(),in.pixels.data(),in.padding.data(),bake,prepared,q,pbr);
  if(!out.normals.empty())compose_normal(in.padding[i],bake,q,out.normals.data()+4*i);
 }
 checkpoint(cancelled);return out;
}
}

namespace dome::paint_grid {
struct Filter::Impl { Tables tables; };
Filter::Filter(const Tables &tables):impl_(std::make_unique<Impl>(Impl{tables})){}
Filter::~Filter()=default;
void Filter::run(const Batch &batch,const Layout &layout,std::span<const Pixel> input,
                 std::span<Pixel> output,const std::function<bool()> &cancelled) {
  validate(batch,layout,input.size(),output.size());
  for(size_t i=0;i<output.size();++i) {
    if((i&255)==0)checkpoint(cancelled);
    output[i]=filter(i,batch,layout,impl_->tables,input.data());
  }
  checkpoint(cancelled);
}
}
