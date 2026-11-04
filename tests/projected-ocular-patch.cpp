// SPDX-License-Identifier: GPL-3.0-or-later
#include "rotary-cap-geometry.hpp"
#include <iostream>
using namespace dome;
namespace {
struct Budget{size_t used=0,maximum=100'000'000;bool cancel=false;void stop()const{if(cancel)throw std::runtime_error("cancelled");}void step(size_t n=1){stop();if(n>maximum-used)throw std::length_error("work");used+=n;}void step_items(size_t n,size_t u){if(u&&n>(maximum-used)/u)throw std::length_error("work");step(n*u);}};
size_t checks=0;void check(bool b,const char*s){++checks;if(!b)throw std::runtime_error(s);}
Mesh disk(bool curved){Mesh m;m.positions.push_back({0,0,curved?-.1f:0.f});for(int i=0;i<12;++i){double a=2*std::numbers::pi*i/12;m.positions.push_back({float(std::cos(a)),float(std::sin(a)),0});}for(uint32_t i=0;i<12;++i)m.indices.insert(m.indices.end(),{0,1+i,1+(i+1)%12});return m;}
Mesh strip(){Mesh m;for(float r:{.7f,1.f})for(int i=0;i<9;++i){double a=-1.2+2.4*i/8;m.positions.push_back({float(r*std::cos(a)),float(r*std::sin(a)),float(.1*r*r)});}for(uint32_t i=0;i<8;++i)m.indices.insert(m.indices.end(),{i,i+1,i+9,i+1,i+10,i+9});return m;}
}
int main()try{
 Budget w;auto flat=disk(false),curved=disk(true);for(const auto&m:{flat,curved}){auto r=detail::analyze_projected_ocular_patch(m,1<<20,w);check(r.disk&&r.convex_boundary,"Flat/curved disk has independent convex geometry");check(r.svd_sweeps==0&&!r.ellipsoid,"Projected rule does not borrow rejected ellipsoid authority");}
 Mesh quad;quad.positions={{-1,-1,0},{1,-1,0},{1,1,0},{-1,1,0}};quad.indices={0,1,2,0,2,3};auto q=detail::analyze_projected_ocular_patch(quad,1<<20,w);check(q.disk&&q.convex_boundary,"Actual four-vertex planar iris supported");
 for(auto&p:quad.positions){auto a=p;p={float(3-2*a.z+.3*a.y),float(1+2*a.x+.2*a.y),float(-4+3*a.y)};}q=detail::analyze_projected_ocular_patch(quad,1<<20,w);check(q.disk&&q.convex_boundary,"Reflection/shear/scale/translation preserves convex planar domain");
 auto concave=detail::analyze_projected_ocular_patch(strip(),1<<20,w);check(concave.disk&&!concave.convex_boundary,"Concave curved tissue strip is distinct");
 auto star=flat;for(int i=0;i<12;++i){int k=(i*5)%12;double a=2*std::numbers::pi*k/12;star.positions[1+i]={float(std::cos(a)),float(std::sin(a)),0};}auto crossed=detail::analyze_projected_ocular_patch(star,1<<20,w);check(!crossed.convex_boundary,"Self-crossed locally turning boundary cannot be a convex patch");
 auto invalid=flat;invalid.indices.insert(invalid.indices.end(),{0,1,2,0,1,2});bool refused=false;try{detail::analyze_projected_ocular_patch(invalid,1<<20,w);}catch(const std::invalid_argument&){refused=true;}check(refused,"Nonmanifold geometry fails instead of becoming tissue");
 refused=false;try{detail::analyze_projected_ocular_patch(flat,1,w);}catch(const std::length_error&){refused=true;}check(refused,"Byte quota before construction");Budget stopped;stopped.cancel=true;refused=false;try{detail::analyze_projected_ocular_patch(flat,1<<20,stopped);}catch(const std::runtime_error&){refused=true;}check(refused,"Actual cancellation remains failure");
 std::cout<<"PASS "<<checks<<" independent flat/curved/quad/concave/crossed/affine/quota/cancel controls\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
