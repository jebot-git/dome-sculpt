// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/source-sampling-transport.hpp"
#include "dome/paint.hpp"
#include <iostream>
using namespace dome;
namespace {
size_t checks=0;
void check(bool ok,const char *message){if(!ok)throw std::runtime_error(message);++checks;}
template<class F>bool refuses(F f){try{f();return false;}catch(const std::exception &){return true;}}
}
int main()try {
  Mesh before;before.positions={{0,0,0},{1,0,0},{1,1,0},{0,1,0}};
  before.normals.assign(4,{0,0,1});before.indices={0,1,2,0,2,3};
  before.uv={{-1.25f,-.5f},{1.75f,-.5f},{1.75f,2.5f},{-1.25f,2.5f}};
  std::vector<uint32_t> vertices{0,1,2,0,2,3};Mesh target;
  for(auto v:vertices){target.positions.push_back(before.positions[v]);target.normals.push_back(before.normals[v]);}
  target.indices={0,1,2,3,4,5};target.uv={{.05f,.05f},{.45f,.05f},{.45f,.45f},{.55f,.55f},{.95f,.95f},{.55f,.95f}};
  auto image=std::make_shared<const Image>(Image{2,2,{255,0,0,32,0,255,0,96,0,0,255,160,255,255,255,255}});
  PaintSourceMaps maps;maps.uv1=std::make_shared<const std::vector<std::array<float,2>>>(
    std::vector<std::array<float,2>>{{.1f,.2f},{.9f,.2f},{.9f,.8f},{.1f,.8f}});
  maps.extra_uv_sets[2]=maps.uv1;maps.extra_uv_sets[4]=maps.uv1;
  std::array<PaintSourceTexture*,5> source{&maps.color,&maps.metal_rough,&maps.emission,&maps.normal,&maps.occlusion};
  for(size_t n=0;n<source.size();++n){source[n]->image=image;source[n]->texcoord=std::array<int,5>{0,1,4,2,0}[n];source[n]->uv_transform={.8f,.1f,-.2f,1.1f,.13f,-.27f};}
  for(int wrap:{10497,33648,33071})for(int min:{9728,9729,9984,9985,9986,9987})for(int mag:{9728,9729}) {
    for(auto s:source){s->wrap_s=s->wrap_t=wrap;s->min_filter=min;s->mag_filter=mag;}
    auto output=detail::transport_source_sampling(before,maps,target,vertices);
    const std::array<const PaintSourceTexture*,5> views{&output->color,&output->metal_rough,&output->emission,&output->normal,&output->occlusion};
    check(output->extra_uv_sets.contains(3)&&output->color.texcoord==3&&output->occlusion.texcoord==3,
          "Original UV0 has a distinct free carrier, independently of authored atlas UV0");
    for(size_t n=0;n<views.size();++n) {
      const auto &a=*source[n],&b=*views[n];
      check(a.image==b.image&&a.wrap_s==b.wrap_s&&a.wrap_t==b.wrap_t&&
            a.min_filter==b.min_filter&&a.mag_filter==b.mag_filter&&a.uv_transform==b.uv_transform,
            "Transport preserves exact image identity, all sampler modes and affine transform");
      for(size_t face=0;face<2;++face)for(int x=0;x<=10;++x)for(int y=0;y<=10-x;++y) {
        std::array<float,3> bary{float(x)/10,float(y)/10,1-float(x)/10-float(y)/10};
        check(source_texture_uv(before,maps,a.texcoord,face,bary)==source_texture_uv(target,*output,b.texcoord,face,bary),
              "Every transported role retains independent original interpolated coordinates");
        check(sample_surface_texture(before,maps,a,face,bary,n==0||n==2)==
              sample_surface_texture(target,*output,b,face,bary,n==0||n==2),
              "Point sampling is exact across independent seam vertices and every wrap/filter pair");
      }
    }
    check(before.uv[0][0]==-1.25f&&target.uv[0][0]==.05f&&maps.color.texcoord==0&&
          maps.uv1->size()==4&&maps.extra_uv_sets.size()==2,
          "Neither original source nor new authored atlas is mutated");
  }
  auto invalid=vertices;invalid.pop_back();
  check(refuses([&]{detail::transport_source_sampling(before,maps,target,invalid);}),"Incomplete remap refuses");
  invalid=vertices;invalid[0]=999;
  check(refuses([&]{detail::transport_source_sampling(before,maps,target,invalid);}),"Out of range remap refuses");
  auto changed=target;changed.positions[0].z=.01f;
  check(refuses([&]{detail::transport_source_sampling(before,maps,changed,vertices);}),"Geometry edits refuse");
  changed=target;std::swap(changed.indices[0],changed.indices[1]);
  check(refuses([&]{detail::transport_source_sampling(before,maps,changed,vertices);}),"Corner reversal refuses");
  auto full=maps;for(uint32_t n=2;n<8;++n)full.extra_uv_sets[n]=maps.uv1;
  check(refuses([&]{detail::transport_source_sampling(before,full,target,vertices);}),"Full carrier table refuses without dropping source sets");
  detail::SourceSamplingLimits limits;limits.max_coordinate_bytes=1;
  check(refuses([&]{detail::transport_source_sampling(before,maps,target,vertices,limits);}),"Cumulative coordinate budget refuses before output");
  limits={};limits.max_work=1;
  check(refuses([&]{detail::transport_source_sampling(before,maps,target,vertices,limits);}),"Cumulative work budget refuses");
  limits={};limits.max_work=64;
  check(refuses([&]{detail::transport_source_sampling(before,maps,target,vertices,limits);}),"Validation and remapping share the declared work budget");
  limits={};limits.cancelled=[]{return true;};
  check(refuses([&]{detail::transport_source_sampling(before,maps,target,vertices,limits);}),"Cancellation refuses before output");
  auto one=maps;one.metal_rough={};one.normal={};one.emission={};one.uv1.reset();one.extra_uv_sets.clear();
  auto first=detail::transport_source_sampling(before,one,target,vertices);
  check(first->color.texcoord==1&&first->uv1&&first->extra_uv_sets.empty(),"First free UV1 is a real original UV0 carrier when no source UV1 exists");
  std::cout<<"PASS "<<checks<<" original sampling transport checks (point samples and metadata; no mip/image claim)\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
