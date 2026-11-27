// SPDX-License-Identifier: GPL-3.0-or-later
#include "paint-composite-backend.hpp"
#include "paint-grid-filter.hpp"
namespace dome::paint_compute {
Result composite(const Input &in,const std::function<bool()> &cancelled){
 validate(in,cancelled);const auto &q=quantization();
 Result out;
 out.pixels.resize(in.padding.size());
 if(!in.bake.normal.empty())out.normals.resize(4*in.padding.size());
 const auto bake=in.bake.view();
 for(size_t i=0;i<out.pixels.size();++i){
  if((i&4095)==0)checkpoint(cancelled);
  out.pixels[i]=compose(uint32_t(i),in.parameters,in.layers.data(),in.tiles.data(),in.pixels.data(),in.padding.data(),bake,q);
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
