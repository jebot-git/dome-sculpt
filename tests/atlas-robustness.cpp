// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/mesh.hpp"
#include "dome/paint.hpp"
#include "../src/atlas-detail.hpp"
#include "cgltf.h"
#include <bit>
#include <iostream>
#include <map>
#include <unistd.h>
using namespace dome;
namespace {
void check(bool condition,const char *message) {if(!condition)throw std::runtime_error(message);std::cout<<"PASS "<<message<<'\n';}
auto bits(Vec3 p){return std::array{std::bit_cast<uint32_t>(p.x),std::bit_cast<uint32_t>(p.y),std::bit_cast<uint32_t>(p.z)};}
double determinant(std::array<float,2> a,std::array<float,2> b,std::array<float,2> c) {
  return double(b[1]-c[1])*(a[0]-c[0])+double(c[0]-b[0])*(a[1]-c[1]);
}
bool closed(const std::vector<uint32_t> &indices) {
  std::map<std::pair<uint32_t,uint32_t>,size_t> edges;
  for(size_t c=0;c<indices.size();c+=3)for(size_t k=0;k<3;++k) {
    auto a=indices[c+k],b=indices[c+(k+1)%3];if(a>b)std::swap(a,b);++edges[{a,b}];
  }
  return !edges.empty() && std::all_of(edges.begin(),edges.end(),[](auto e){return e.second==2;});
}
struct Rect {double x0,y0,x1,y1;};
size_t independent_padded_charts(const Mesh &m,AtlasInfo info,uint32_t padding,const detail::AtlasDiagnostics &diagnostics) {
  std::vector<Rect> charts;std::vector<uint8_t> tiny(m.indices.size()/3);double left=info.width;
  for(size_t f=0;f<tiny.size();++f) {
    auto a=m.uv[m.indices[f*3]],b=m.uv[m.indices[f*3+1]],c=m.uv[m.indices[f*3+2]];
    if(diagnostics.repaired_faces[f]) {
      if(!(a[1]==b[1] && a[0]==c[0] && std::abs((double(b[0])-a[0])*info.width-4)<.001 &&
           std::abs((double(c[1])-a[1])*info.height-4)<.001 && a[0]>.5f))throw std::runtime_error("Repair chart lost its minimum integer-texel footprint");
      double x=double(a[0])*info.width,y=double(a[1])*info.height,gutter=padding+1;
      charts.push_back({x-gutter,y-gutter,x+4+gutter,y+4+gutter});left=std::min(left,x-2*gutter);tiny[f]=1;
    }
  }
  for(size_t i=0;i<charts.size();++i) {
    auto a=charts[i];if(a.x0<-.001 || a.y0<-.001 || a.x1>info.width+.001 || a.y1>info.height+.001)
      throw std::runtime_error("Repair padding extends outside its atlas");
    for(size_t j=0;j<i;++j) {
      auto b=charts[j];if(!(a.x1<=b.x0+.001 || b.x1<=a.x0+.001 || a.y1<=b.y0+.001 || b.y1<=a.y0+.001))
        throw std::runtime_error("Independent repaired chart padding overlaps");
    }
  }
  if(!charts.empty())for(size_t f=0;f<tiny.size();++f)if(!tiny[f])for(size_t k=0;k<3;++k)
    if(double(m.uv[m.indices[f*3+k]][0])*info.width>left+.001)
      throw std::runtime_error("Normal atlas charts enter the independent repair band");
  return charts.size();
}
void independent_chart_padding(const Mesh &m,AtlasInfo info,uint32_t padding,const detail::AtlasDiagnostics &diagnostics) {
  const int width=int(info.width),height=int(info.height);std::vector<int32_t> mask(size_t(width)*height,-1),expanded(mask);
  for(size_t face=0;face<m.indices.size()/3;++face) {
    std::array<std::array<double,2>,3> uv;
    for(size_t k=0;k<3;++k){auto v=m.uv[m.indices[face*3+k]];uv[k]={double(v[0])*width,double(v[1])*height};}
    int x0=std::max(0,int(std::floor(std::min({uv[0][0],uv[1][0],uv[2][0]})))),x1=std::min(width-1,int(std::ceil(std::max({uv[0][0],uv[1][0],uv[2][0]}))));
    int y0=std::max(0,int(std::floor(std::min({uv[0][1],uv[1][1],uv[2][1]})))),y1=std::min(height-1,int(std::ceil(std::max({uv[0][1],uv[1][1],uv[2][1]}))));
    double det=(uv[1][0]-uv[0][0])*(uv[2][1]-uv[0][1])-(uv[1][1]-uv[0][1])*(uv[2][0]-uv[0][0]);
    for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x) {
      bool inside=true;for(size_t k=0;k<3;++k){auto a=uv[k],b=uv[(k+1)%3];double edge=(b[0]-a[0])*(y+.5-a[1])-(b[1]-a[1])*(x+.5-a[0]);inside&=det>0?edge>=-1e-8:edge<=1e-8;}
      if(!inside)continue;auto &at=mask[size_t(y)*width+x];int32_t chart=int32_t(diagnostics.face_charts[face]);
      if(at!=-1 && at!=chart)throw std::runtime_error("Independent chart rasterization found overlapping charts");at=chart;
    }
  }
  for(int y=0;y<height;++y)for(int x=0;x<width;++x)if(auto chart=mask[size_t(y)*width+x];chart!=-1)
    for(int dy=-int(padding);dy<=int(padding);++dy)for(int dx=-int(padding);dx<=int(padding);++dx) {
      int a=x+dx,b=y+dy;if(a<0||b<0||a>=width||b>=height)continue;auto &at=expanded[size_t(b)*width+a];
      if(at!=-1 && at!=chart)throw std::runtime_error("Requested per-chart pixel padding was narrowed");at=chart;
    }
}
void exact_geometry(const Mesh &before,const Mesh &after) {
  if(before.indices.size()!=after.indices.size())throw std::runtime_error("Atlas changed face count");
  std::vector<uint32_t> origins(after.positions.size(),UINT32_MAX),welded;welded.reserve(after.indices.size());
  for(size_t c=0;c<before.indices.size();++c) {
    auto a=before.indices[c],b=after.indices[c];
    if(bits(before.positions[a])!=bits(after.positions[b]) || bits(before.normals[a])!=bits(after.normals[b]))
      throw std::runtime_error("Atlas changed exact metre-space geometry, normals or ordered corners");
    if(origins[b]!=UINT32_MAX && origins[b]!=a)throw std::runtime_error("Atlas welded distinct original vertex identities");
    origins[b]=a;welded.push_back(a);
  }
  if(welded!=before.indices || !closed(welded))throw std::runtime_error("UV splitting changed source watertightness");
}
Mesh capacity_fixture() {
  // One connected planar chart: disconnected tiny triangles may legitimately
  // become independently magnified xatlas charts and never need repair.
  const float s=std::ldexp(1.f,-21);Mesh m;m.positions={{0,s,0}};
  for(uint32_t i=0;i<=41;++i)m.positions.push_back({float(i)*s,0,0});
  m.positions.insert(m.positions.end(),{{1,0,0},{1,1,0},{0,1,0}});
  for(uint32_t i=1;i+1<m.positions.size();++i)m.indices.insert(m.indices.end(),{0,i,i+1});
  m.normals.assign(m.positions.size(),{0,0,1});return m;
}
}
int main(int argc,char **)try {
  if(argc==1) {
  Volume field(.04f);field.begin_stroke();Brush brush;brush.radius=.4f;field.stamp(brush);field.end_stroke();
  const auto base=surface_mesh(field);check(closed(base.indices),"Actual0.04m sculpt sphere starts watertight with all tiny closing faces");
  auto path=std::filesystem::temp_directory_path()/("dome-atlas-robustness-"+std::to_string(getpid())+".glb");
  for(int exponent:{0,-10,-30}) {
    auto before=base;for(auto &p:before.positions)p=p*std::ldexp(1.f,exponent);auto after=before;
    detail::AtlasDiagnostics diagnostics;auto info=detail::unwrap_atlas(after,512,4,&diagnostics);after.validate();exact_geometry(before,after);
    check(true,"Atlas preserves exact ordered face geometry, source identities and welded watertightness across metre scales");
    for(size_t c=0;c<after.indices.size();c+=3)
      if(std::abs(determinant(after.uv[after.indices[c]],after.uv[after.indices[c+1]],after.uv[after.indices[c+2]]))<1e-14)
        throw std::runtime_error("A strictly nonpaintable triangle escaped atlas repair");
    check(independent_padded_charts(after,info,4,diagnostics)>0,"Collapsed UV faces receive disjoint4x4texel charts with independent padding outside surviving charts");
    PaintedMesh painted(after,128);check(painted.mesh().indices.size()==before.indices.size(),"Strict PaintedMesh accepts the full repaired sphere without dropping any face");
    if(exponent==0) {
      independent_chart_padding(after,info,4,diagnostics);
      check(diagnostics.u_scale>=.5f && diagnostics.internal_padding>=2*(4+1),"Independent ordinary and repaired chart masks retain all requested pixel padding after affine compression");
      auto repeated=before;unwrap(repeated,512,4);
      bool same=repeated.indices==after.indices && repeated.uv==after.uv && repeated.positions.size()==after.positions.size();
      for(size_t v=0;same && v<after.positions.size();++v)same&=bits(repeated.positions[v])==bits(after.positions[v]);
      check(same,"Identical input deterministically reproduces repaired chart coordinates and vertex/corner ordering");
      export_glb(after,path);cgltf_options options{};cgltf_data *raw=nullptr;
      if(cgltf_parse_file(&options,path.c_str(),&raw)!=cgltf_result_success)throw std::runtime_error("Independent GLB parse failed");
      std::unique_ptr<cgltf_data,decltype(&cgltf_free)> data(raw,cgltf_free);
      check(cgltf_load_buffers(&options,raw,path.c_str())==cgltf_result_success && cgltf_validate(raw)==cgltf_result_success,
            "Independent cgltf accepts the complete repaired sphere GLB");
      auto &p=raw->meshes[0].primitives[0];auto position=cgltf_find_accessor(&p,cgltf_attribute_type_position,0);
      for(size_t c=0;c<after.indices.size();++c) {
        auto index=cgltf_accessor_read_index(p.indices,c);float value[3];
        if(index!=after.indices[c] || !cgltf_accessor_read_float(position,index,value,3) || bits({value[0],value[1],value[2]})!=bits(before.positions[before.indices[c]]))
          throw std::runtime_error("Independent GLB reader found changed geometry or face order");
      }
      check(true,"Independent GLB accessors retain every original ordered metre-space corner exactly");
    }
  }
  // Exact dense source from the native baking regression. Its requested64px
  // atlas needs nine legal repair columns; rounding that to16 falsely refuses.
  Volume sculpt;sculpt.begin_stroke();Brush clay;clay.radius=.18f;sculpt.stamp(clay);sculpt.end_stroke();
  auto dense_source=surface_mesh(sculpt),dense=dense_source;detail::AtlasDiagnostics dense_diagnostics;
  auto dense_info=detail::unwrap_atlas(dense,64,2,&dense_diagnostics);exact_geometry(dense_source,dense);
  independent_chart_padding(dense,dense_info,2,dense_diagnostics);PaintedMesh dense_paint(dense,128);
  check(independent_padded_charts(dense,dense_info,2,dense_diagnostics)>0 && dense_diagnostics.u_scale>=.5f,
        "Actual dense baking sculpt fits non-power-of-two repair capacity with exact geometry, independent padding and strict paint");
  std::filesystem::remove(path);
  }
  auto insufficient=capacity_fixture(),saved=insufficient;bool refused=false;
  try{unwrap(insufficient,64,0);}catch(const std::exception &e){std::string s=e.what();std::cout<<"CAPACITY "<<s<<'\n';refused=s.find("requested resolution 64")!=s.npos && s.find("independent padded small-face charts")!=s.npos;}
  bool unchanged=insufficient.indices==saved.indices && insufficient.uv==saved.uv && insufficient.positions.size()==saved.positions.size();
  for(size_t v=0;unchanged && v<saved.positions.size();++v)unchanged&=bits(insufficient.positions[v])==bits(saved.positions[v]);
  check(refused && unchanged,"Insufficient chart capacity reports resolution and affected count while preserving the input mesh atomically");
  unwrap(insufficient,512,0);PaintedMesh large(insufficient,128);
  check(insufficient.indices.size()==saved.indices.size(),"Increasing requested resolution fits all independent tiny charts under the same strict policy");
  return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
