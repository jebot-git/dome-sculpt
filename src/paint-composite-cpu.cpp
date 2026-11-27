// SPDX-License-Identifier: GPL-3.0-or-later
#include "paint-composite-backend.hpp"
#include "paint-grid-filter.hpp"
namespace dome::paint_compute {
std::vector<Output> composite(const Input &in,const std::function<bool()> &cancelled){
 validate(in,cancelled);const auto &q=quantization();std::vector<Output> out(in.padding.size());
 for(size_t i=0;i<out.size();++i){if((i&4095)==0)checkpoint(cancelled);out[i]=compose(uint32_t(i),in.parameters,in.layers.data(),in.tiles.data(),in.pixels.data(),in.padding.data(),q);}
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
