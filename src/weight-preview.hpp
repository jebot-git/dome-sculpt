// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/weight-authoring.hpp"
#include "weight-brush-device.hpp"

namespace dome::detail {
// A preview is derived from one immutable scene and remains on its worker.
// The final committed gesture is independently evaluated from the same base.
class WeightScenePreview {
public:
  WeightScenePreview(const Scene &,uint64_t object,uint32_t joint,const WeightToolSettings &,
                     const std::function<bool()> &cancelled);
  ~WeightScenePreview();
  Scene paint(std::span<const WeightBrush>,const std::function<bool()> &cancelled);
private:
  struct State;
  std::unique_ptr<State> state_;
};

// This cache belongs to one prepared, immutable surface list and tool setup.
struct WeightFalloffPreview {
  struct Range {
    size_t first,count;
    unsigned sides;
  };
  std::vector<WeightBrush> prefix;
  std::vector<size_t> offsets;
  std::vector<Range> ranges;
  std::vector<float> maxima;
  std::unique_ptr<weight_brush::Query> query;
  bool resident=false;
};
WeightEditResult preview_weights(std::span<const WeightPaintSurface>,size_t bones,std::span<const WeightBrush>,
    WeightEditOptions,WeightFalloffPreview &,const std::function<bool()> &cancelled);
}
