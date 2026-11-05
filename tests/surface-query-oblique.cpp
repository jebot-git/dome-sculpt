// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/mesh-processing.hpp"
#include <iostream>
#include <stdexcept>
using namespace dome;
void check(bool ok){if(!ok)throw std::runtime_error("Oblique thin-triangle oracle failed");}
int main()try{
  size_t count=0;
  for(float s:{std::ldexp(1.f,-20),1.f,32.f})
    for(float ratio:{std::ldexp(1.f,-10),std::ldexp(1.f,-30),std::ldexp(1.f,-60)})
      for(int axis=0;axis<3;++axis)for(bool mirrored:{false,true})for(bool flipped:{false,true}){
        auto turn=[&](Vec3 p){if(mirrored)p.x=-p.x;return axis==0?p:axis==1?Vec3{p.z,p.x,p.y}:Vec3{p.y,p.z,p.x};};
        const float h=s*ratio,d=s*.125f;
        Mesh m;m.positions={turn({0,0,0}),turn({s,s,0}),turn({s,s,h})};
        m.normals.assign(3,turn(normalized(Vec3{1,-1,0})));m.indices=flipped?std::vector<uint32_t>{0,2,1}:std::vector<uint32_t>{0,1,2};
        SurfaceQuery q(m);auto hit=q.closest(turn({s*.75f+d,s*.75f-d,h*.25f}));
        const std::array<float,3> expected=flipped?std::array<float,3>{.25f,.25f,.5f}:std::array<float,3>{.25f,.5f,.25f};
        for(size_t i=0;i<3;++i)check(std::abs(hit.barycentric[i]-expected[i])<1e-6f);
        const auto point=turn({s*.75f,s*.75f,h*.25f});
        check(hit.position.x==point.x&&hit.position.y==point.y&&hit.position.z==point.z);
        check(std::abs(double(hit.distance)-std::sqrt(2.)*d)<double(s)*1e-7);
        const auto edge=q.closest(turn({s*.5f+d,s*.5f-d,-h}));
        const std::array<float,3> e=flipped?std::array<float,3>{.5f,0,.5f}:std::array<float,3>{.5f,.5f,0};
        for(size_t i=0;i<3;++i)check(std::abs(edge.barycentric[i]-e[i])<1e-6f);
        SurfaceQueryOptions options;options.closest_maximum=s*.17f;
        check(!q.nearest(turn({s*.75f+d,s*.75f-d,h*.25f}),options));
        options.closest_maximum=s*.18f;check(q.nearest(turn({s*.75f+d,s*.75f-d,h*.25f}),options).has_value());
        m.normals.assign(3,{});SurfaceQuery geometric(m);
        const auto expected_normal=turn(normalized(Vec3{1,-1,0}))*(flipped?-1.f:1.f)*(mirrored?-1.f:1.f);
        check(length(geometric.closest(turn({s*.75f+d,s*.75f-d,h*.25f})).normal-expected_normal)<1e-6f);
        const auto ray=geometric.raycast(turn({s*.75f+d,s*.75f-d,h*.25f}),turn({-1,1,0}),d*2);
        check(ray&&length(ray->normal-expected_normal)<1e-6f);
        ++count;
      }
  std::cout<<"PASS "<<count<<" oblique scale/aspect/axis/reflection/winding families\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
