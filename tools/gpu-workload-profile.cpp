// SPDX-License-Identifier: GPL-3.0-or-later
// Reproducible regular-user CPU cost audit; no GPU execution or UI hooks.
#include "dome/local-surface.hpp"
#include "dome/paint.hpp"
#include "dome/bake.hpp"
#include <chrono>
#include <iostream>
using namespace dome;
using Clock=std::chrono::steady_clock;
namespace {
double milliseconds(Clock::time_point t){return std::chrono::duration<double,std::milli>(Clock::now()-t).count();}
Mesh plane(unsigned divisions=1,float z=0){Mesh m;for(unsigned y=0;y<=divisions;++y)for(unsigned x=0;x<=divisions;++x){float u=float(x)/divisions,v=float(y)/divisions;m.positions.push_back({u*2-1,v*2-1,z});m.normals.push_back({0,0,1});m.uv.push_back({u,v});}for(unsigned y=0;y<divisions;++y)for(unsigned x=0;x<divisions;++x){uint32_t a=y*(divisions+1)+x,b=a+1,c=a+divisions+2,d=c-1;m.indices.insert(m.indices.end(),{a,b,c,a,c,d});}return m;}
Volume sphere(float voxel){Volume::Voxels v;const float band=voxel*4;const int n=int(std::ceil((.25f+band)/voxel));for(int z=-n;z<=n;++z)for(int y=-n;y<=n;++y)for(int x=-n;x<=n;++x){float d=length(Vec3{float(x),float(y),float(z)}*voxel)-.25f;if(d<band)v.emplace(Int3{x,y,z},std::max(-band,d));}return Volume::from_samples(voxel,std::move(v));}
}
int main()try{
 for(float voxel:{.02f,.01f,.0075f}){
  auto field=sphere(voxel);auto t=Clock::now();auto mesh=surface_mesh(field);auto global=milliseconds(t);t=Clock::now();auto cache=LocalSurfaceCache::build(field);auto cold=milliseconds(t);t=Clock::now();auto materialized=cache->materialize();auto flat=milliseconds(t);
  if(mesh.positions.size()!=materialized.positions.size()||mesh.indices!=materialized.indices)throw std::runtime_error("Cost audit extraction changed topology");
  std::cout<<"EXTRACT voxel="<<voxel<<" samples="<<field.voxels().size()<<" vertices="<<mesh.positions.size()<<" cells="<<cache->stats().cells_evaluated<<" tiles="<<cache->tiles().size()<<" global_ms="<<global<<" local_cold_ms="<<cold<<" materialize_ms="<<flat<<std::endl;
  auto same=Clock::now();if(cache->update(field)!=cache)throw std::runtime_error("Same-owner update rebuilt");std::cout<<"EXTRACT_NOOP ms="<<milliseconds(same)<<std::endl;
 }
 const auto quad=plane();
 for(uint32_t r:{512u,1024u,2048u})for(auto channel:{PaintChannel::Color,PaintChannel::Metallic}){
  auto t=Clock::now();auto paint=PaintedMesh::from_mesh(quad,r,PaintSurfaceOptions{});auto raster=milliseconds(t);paint->begin_stroke();PaintSettings settings;settings.channel=channel;
  Brush b;b.radius=.23f;b.pressure=.83f;b.strength=.61f;b.normal={0,0,1};t=Clock::now();
  for(int i=0;i<8;++i){b.from={-.7f,-.6f+i*.15f,0};b.to={.7f,-.6f+i*.15f,0};paint->stamp(b,settings);}
  auto stamps=milliseconds(t);if(!paint->end_stroke())throw std::runtime_error("Cost audit paint stroke was no-op");t=Clock::now();auto image=paint->composite(Material{});auto composite=milliseconds(t);image->color.validate();
  std::cout<<"PAINT resolution="<<r<<" channel="<<int(channel)<<" strokes=8 raster_ms="<<raster<<" stamp_ms="<<stamps<<" composite_ms="<<composite<<" storage="<<paint->storage_bytes()<<std::endl;
 }
 for(unsigned divisions:{16u,64u})for(uint32_t r:{128u,256u})for(uint32_t ao:{0u,16u}){
  BakeSource source;source.mesh=plane(divisions,.01f);BakeOptions o;o.resolution=r;o.ao_samples=ao;o.padding=0;
  auto t=Clock::now();auto result=bake_mesh(quad,source,o);const auto cost=milliseconds(t);
  if(result.projected!=uint64_t(r)*r||result.missed)throw std::runtime_error("Cost audit bake missed real surface");
  std::cout<<"BAKE triangles="<<source.mesh.indices.size()/3<<" resolution="<<r<<" AO="<<ao<<" rays="<<result.rays<<" projected="<<result.projected<<" ms="<<cost<<std::endl;
 }
 return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
