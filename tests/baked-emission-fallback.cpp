// SPDX-License-Identifier: GPL-3.0-or-later
#include "emission-fallback-metrics.hpp"
#include <iostream>
#include <stdexcept>
namespace {
size_t checks=0;
void check(bool v,const char *s){++checks;if(!v)throw std::runtime_error(s);}
template<class F>void refuses(F f,const char *s){bool caught=false;try{f();}catch(const std::exception&){caught=true;}check(caught,s);}
}
int main()try{
  using namespace dome::detail;
  const float largest=std::numeric_limits<float>::max(),smallest=std::numeric_limits<float>::denorm_min();
  ResolvedFallbackMetrics zero;zero.sample(0,0);check(zero.maximum_absolute_error==0&&zero.maximum_relative_error==0&&zero.erased_nonzero_samples==0,"defined exact-zero denominator rule");
  refuses([&]{zero.sample(0,1);},"invented energy at zero has no finite relative metric");
  ResolvedFallbackMetrics tiny;tiny.sample(smallest,double(largest));
  const double ieee_bound=std::ldexp(2.-std::ldexp(1.,-23),276);
  check(std::isfinite(tiny.maximum_relative_error)&&tiny.maximum_relative_error==ieee_bound,"near-minsubnormal ratio is exact finite binary64 using independent IEEE power oracle");
  check(tiny.maximum_relative_error>double(largest)&&tiny.maximum_absolute_error==double(largest),"dimensionless ratio does not inherit FLOAT radiance bound");
  check(maximum_resolved_fallback_relative_error==ieee_bound,"declared finite ratio bound follows F32 source/reconstruction definition");
  ResolvedFallbackMetrics erased;erased.sample(largest,0);check(erased.maximum_absolute_error==double(largest)&&erased.maximum_relative_error==1&&erased.erased_nonzero_samples==1,"erasure is explicitly measured, never silently hidden or clamped");
  check(resolved_fallback_radiance(255,1,largest)==double(largest),"full F32 scale reconstructs without overflow or half conversion");
  check(resolved_fallback_radiance(0,1,largest)==0,"PNG zero remains exact at maximum scale");
  check(resolved_fallback_radiance(255,.25f,52016)==13004,"normalization scale and component factor applied exactly once");
  const long double encoded=128.L/255;
  const long double independent=std::pow((encoded+.055L)/1.055L,2.4L)*.25L*52016.L;
  check(std::abs(static_cast<long double>(resolved_fallback_radiance(128,.25f,52016))-independent)<1e-11L,"PNG sRGB reconstruction agrees with independent extended-precision oracle");
  ResolvedFallbackMetrics physical;physical.sample(50000,resolved_fallback_radiance(255,.25f,52016));
  check(physical.maximum_absolute_error==36996&&physical.maximum_relative_error==36996./50000,"physical error above old30000 and unitless ratio retain actual values");
  for(double value:{-1.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN(),double(largest)*2})
    refuses([&]{ResolvedFallbackMetrics m;m.sample(1,value);},"invalid/non-F32-scale reconstruction refuses");
  refuses([&]{ResolvedFallbackMetrics m;m.sample(-1,0);},"negative source refuses");
  std::cout<<"Baked emission fallback "<<checks<<" checks PASS\n";return 0;
}catch(const std::exception &e){std::cerr<<"Baked emission fallback FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}
