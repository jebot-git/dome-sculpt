// SPDX-License-Identifier: GPL-3.0-or-later
#include "rotary-cap-geometry.hpp"
#include <iostream>
using namespace dome;
namespace {
struct Budget{size_t used=0,maximum=100'000'000;bool cancel=false;void stop()const{if(cancel)throw std::runtime_error("cancelled");}void step(size_t n=1){stop();if(n>maximum-used)throw std::length_error("work");used+=n;}void step_items(size_t n,size_t u){if(u&&n>(maximum-used)/u)throw std::length_error("work");step(n*u);}};
size_t checks=0;void check(bool b,const char*s){++checks;if(!b)throw std::runtime_error(s);}
Mesh cap(bool flat=false){Mesh m;auto add=[&](double x,double y,double z){m.positions.push_back({float(.07+.03*x),float(1.45+.025*y),float(-.02+.05*z)});};add(0,0,flat?0:-1);constexpr uint32_t sides=24;
 for(double r:{.2,.45,.7})for(uint32_t i=0;i<sides;++i){double a=2*std::numbers::pi*i/sides;add(r*std::cos(a),r*std::sin(a),flat?0:-std::sqrt(1-r*r));}
 for(uint32_t i=0;i<sides;++i)m.indices.insert(m.indices.end(),{0,1+(i+1)%sides,1+i});
 for(uint32_t ring=0;ring<2;++ring)for(uint32_t i=0;i<sides;++i){auto a=1+ring*sides+i,b=1+ring*sides+(i+1)%sides,c=a+sides,d=b+sides;m.indices.insert(m.indices.end(),{a,b,c,b,d,c});}
 return m;}
Mesh crescent(){Mesh m;constexpr uint32_t sides=12;for(double r:{.4,.55,.7})for(uint32_t i=0;i<=sides;++i){double a=-1.2+2.4*i/sides;double x=r*std::cos(a),y=r*std::sin(a);m.positions.push_back({float(x),float(y),float(-std::sqrt(1-r*r))});}
 for(uint32_t ring=0;ring<2;++ring)for(uint32_t i=0;i<sides;++i){auto a=ring*(sides+1)+i,b=a+1,c=a+sides+1,d=c+1;m.indices.insert(m.indices.end(),{a,b,c,b,d,c});}return m;}
}
int main()try{
 Budget w;auto m=cap();auto r=detail::analyze_rotary_cap(m,1<<20,w);
 check(r.disk&&r.convex_boundary,"Analytic curved convex disk recognized");check(r.ellipsoid,"Analytic ellipsoid model positive");check(r.svd_sweeps<=30,"Native bounded SVD contract");
 const std::array<double,3>center{.07,1.45,-.02};for(size_t i=0;i<3;++i)check(std::abs(r.center[i]-center[i])<1e-4,"Independent ellipsoid center");
 auto transformed=m;for(auto&p:transformed.positions){auto old=p;p={float(3+old.z*2),float(-1+old.x*2),float(.5+old.y*2)};}
 auto t=detail::analyze_rotary_cap(transformed,1<<20,w);check(t.disk&&t.convex_boundary&&t.ellipsoid,"Similarity preserves model topology");
 const std::array<double,3>expected{3+center[2]*2,-1+center[0]*2,.5+center[1]*2};for(size_t i=0;i<3;++i)check(std::abs(t.center[i]-expected[i])<3e-4,"Independent transformed center");
 auto plane=detail::analyze_rotary_cap(cap(true),1<<20,w);check(plane.disk&&plane.convex_boundary,"Flat disk is valid geometry");check(plane.rank_ratio<1e-12||!plane.ellipsoid,"Flat disk cannot certify unique rotary volume");
 auto strip=detail::analyze_rotary_cap(crescent(),1<<20,w);check(strip.disk&&!strip.convex_boundary,"Concave curved strip remains distinct from a cap");
 bool refused=false;try{detail::analyze_rotary_cap(m,1,w);}catch(const std::length_error&){refused=true;}check(refused,"Prospective byte refusal");Budget stopped;stopped.cancel=true;refused=false;try{detail::analyze_rotary_cap(m,1<<20,stopped);}catch(const std::runtime_error&){refused=true;}check(refused,"Actual cancellation");
 std::cout<<"PASS "<<checks<<" independent cap/ellipsoid/plane/crescent/similarity/budget/cancel controls\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
