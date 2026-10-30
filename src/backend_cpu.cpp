// SPDX-License-Identifier: GPL-3.0-or-later
#include "brush_kernel.hpp"
#include "csg-kernel.hpp"
#include <vector>
namespace dome {
void apply_brush(std::span<Sample> samples, const Brush &b) {
  for (auto &s : samples)
    s.distance = brush_sample(s, b);
}
void apply_csg(std::span<Sample> samples,std::span<const CsgBox> boxes,float band,
               const std::function<bool()> &cancelled) {
  if(samples.empty() || boxes.empty())return;
  csg_detail::validate(samples,boxes,band,cancelled);
  std::vector<float> result(samples.size());
  for(size_t i=0;i<samples.size();++i) {
    if((i&255)==0)csg_detail::checkpoint(cancelled);
    result[i]=csg_detail::sample(samples[i],boxes.data(),boxes.size(),band);
  }
  csg_detail::checkpoint(cancelled);
  for(size_t i=0;i<samples.size();++i)samples[i].distance=result[i];
}
std::string_view compute_backend() { return "CPU"; }
} // namespace dome
