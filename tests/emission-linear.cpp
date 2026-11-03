// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/emission-linear.hpp"
#include <iostream>
#include <limits>
#include <vector>
using namespace dome::detail;
namespace {
int checks = 0;
void check(bool value, const char *message) {
  ++checks;
  if (!value) throw std::runtime_error(message);
}
template<class F> bool rejects(F f) {
  try { f(); } catch (const std::exception &) { return true; }
  return false;
}
bool close(float a, long double b) {
  return std::abs(static_cast<long double>(a)-b) <=
         4e-6L * std::max(1.L, std::abs(b));
}
}
int main() try {
  struct Layer { EmissionPaint paint; float alpha; };
  const std::vector<Layer> layers{
    {{{.2f,.9f,.1f},{}},.65f},
    {{{},{12.f,.0003f,9999.f}},.37f},
    {{{.8f,.1f,.5f},{}},.13f},
    {{{},{0,3.5f,1.f}},.8f}};
  EmissionOverlay overlay;
  for (auto layer : layers) overlay=emission_over(overlay,layer.paint,layer.alpha);
  // Independent back-to-front transmittance sum, not the production over fold.
  for (float strength : {0.f,.125f,1.f,7.25f,10000.f}) {
    for (EmissionRgb source : {EmissionRgb{0,0,0},EmissionRgb{.1f,15,9000}}) {
      auto result=emission_radiance(overlay,source,strength);
      for (size_t channel=0;channel<3;++channel) {
        long double transmission=1, expected=0;
        for (auto i=layers.rbegin();i!=layers.rend();++i) {
          expected+=transmission*i->alpha*(static_cast<long double>(i->paint.legacy[channel])*strength+i->paint.physical[channel]);
          transmission*=1-static_cast<long double>(i->alpha);
        }
        expected+=transmission*source[channel];
        check(close(result[channel],expected),"Physical and legacy layer transmittance differs");
      }
    }
  }
  auto absolute=emission_over({},{{},{5,.25f,100}},1);
  check(emission_radiance(absolute,{100,200,300},0)==EmissionRgb{5,.25f,100},
        "Zero authored strength must not erase absolute paint");
  auto invisible=emission_over(overlay,{{},{10000,10000,10000}},0);
  check(invisible.coverage==overlay.coverage&&invisible.legacy==overlay.legacy&&invisible.physical==overlay.physical,
        "Zero opacity must be exact");
  auto near=emission_over({},{{},{0,0,0}},.999f);
  auto residual=emission_radiance(near,{10000,10000,10000},1);
  const float old_coverage=std::round(near.coverage*255)/255;
  check(residual[0]>9.9f && 10000*(1-old_coverage)==0,
        "UNORM8 coverage counterfactual must expose lost HDR source residual");
  for (EmissionRgb value : {EmissionRgb{},EmissionRgb{.1f,.5f,1},EmissionRgb{1.30411887f,.232f,.245f},EmissionRgb{0,.0001f,10000}}) {
    auto encoded=emission_factor(value);
    check(encoded.strength>=1,"Standard emission strength must remain valid");
    for(size_t channel=0;channel<3;++channel) {
      check(encoded.factor[channel]>=0&&encoded.factor[channel]<=1,"glTF factor exceeds unit range");
      check(close(encoded.factor[channel]*encoded.strength,value[channel]),"Physical factor reconstruction differs");
    }
  }
  for (float invalid : {-1.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
    check(rejects([&]{emission_factor({invalid,0,0});}),"Malformed physical factor admitted");
    check(rejects([&]{emission_over({},{{},{1,1,1}},invalid);}),"Malformed paint opacity admitted");
    check(rejects([&]{emission_radiance({}, {1,1,1},invalid);}),"Malformed authored strength admitted");
  }
  check(rejects([]{emission_factor({10001,0,0});}),"Physical bound must not silently clamp");
  check(rejects([]{emission_radiance({{1,0,0},{},0},{},1);}),"Uncovered legacy coefficient must refuse");
  check(rejects([]{emission_radiance({{}, {2,0,0},0},{},1);}),"Uncovered physical coefficient must refuse");
  check(rejects([]{emission_radiance({}, {std::numeric_limits<float>::infinity(),0,0},1);}),
        "Nonfinite source radiance must refuse explicitly");
  std::cout<<"Linear emission algebra: "<<checks<<" checks\n";
  return 0;
} catch(const std::exception &e) { std::cerr<<"FAIL "<<e.what()<<'\n';return 1; }
