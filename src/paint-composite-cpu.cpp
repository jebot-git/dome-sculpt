// SPDX-License-Identifier: GPL-3.0-or-later
#include "paint-composite-backend.hpp"
namespace dome::paint_compute {
std::vector<Output> composite(const Input &in,const std::function<bool()> &cancelled){
 validate(in,cancelled);const auto &q=quantization();std::vector<Output> out(in.padding.size());
 for(size_t i=0;i<out.size();++i){if((i&4095)==0)checkpoint(cancelled);out[i]=compose(uint32_t(i),in.parameters,in.layers.data(),in.tiles.data(),in.pixels.data(),in.padding.data(),q);}
 checkpoint(cancelled);return out;
}
}
