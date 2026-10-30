// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/bake.hpp"
#include "dome/mesh-processing.hpp"
#include "dome/tangents.hpp"
#include "dome/source-maps.hpp"
#include <algorithm>
#include <cmath>
#include <deque>
#include <limits>
namespace dome {
namespace {
constexpr float Pi = 3.14159265358979323846f;
const Image *normal_image(const BakeMaterial &material) {
  if (material.normal_map) return material.normal_map.get();
  return material.maps && !material.maps->normal.rgba.empty() ? &material.maps->normal : nullptr;
}
void cancel(const BakeOptions &o) {
  if (o.cancelled && o.cancelled()) throw std::runtime_error("Mesh baking cancelled");
}
Vec3 interpolate(const Mesh &mesh, size_t triangle, const std::array<float,3> &bary, bool normals) {
  const auto &values = normals ? mesh.normals : mesh.positions;
  Vec3 result;
  for (size_t i = 0; i < 3; ++i) result += values.at(mesh.indices.at(triangle*3+i)) * bary[i];
  if (!normals) return result;
  if (length(result)>1e-8f) return normalized(result);
  auto a=mesh.positions[mesh.indices[triangle*3]],b=mesh.positions[mesh.indices[triangle*3+1]],c=mesh.positions[mesh.indices[triangle*3+2]];
  return normalized(cross(b-a,c-a));
}
std::array<float,2> texcoord(const Mesh &mesh, size_t triangle, const std::array<float,3> &bary) {
  std::array<float,2> result{};
  for (size_t i = 0; i < 3; ++i) {
    auto uv = mesh.uv.at(mesh.indices.at(triangle*3+i));
    result[0] += uv[0]*bary[i]; result[1] += uv[1]*bary[i];
  }
  return result;
}
struct Basis { Vec3 tangent, bitangent, normal; };
Basis basis(const Mesh &mesh, size_t triangle, Vec3 normal, const std::array<float,3> &bary, const std::vector<std::array<float,4>> &supplied = {}) {
  if (!supplied.empty()) {
    Vec3 tangent;float hand=0;
    for(int i=0;i<3;++i){auto value=supplied[mesh.indices[triangle*3+i]];tangent+=Vec3{value[0],value[1],value[2]}*bary[i];hand+=value[3]*bary[i];}
    tangent=tangent-normal*dot(normal,tangent);if(length(tangent)<1e-10f)throw std::runtime_error("Invalid supplied tangent frame");
    tangent=normalized(tangent);return {tangent,cross(normal,tangent)*(hand<0?-1.f:1.f),normal};
  }
  auto a=mesh.indices[triangle*3], b=mesh.indices[triangle*3+1], c=mesh.indices[triangle*3+2];
  auto uv0=mesh.uv[a], uv1=mesh.uv[b], uv2=mesh.uv[c];
  double u1=double(uv1[0])-uv0[0], v1=double(uv1[1])-uv0[1], u2=double(uv2[0])-uv0[0], v2=double(uv2[1])-uv0[1];
  double determinant=u1*v2-u2*v1,uvscale=std::hypot(u1,v1)*std::hypot(u2,v2);
  if (!uvscale||std::abs(determinant)<=uvscale*1e-12) throw std::runtime_error("Degenerate UV tangent basis");
  auto e1=mesh.positions[b]-mesh.positions[a], e2=mesh.positions[c]-mesh.positions[a];
  auto direction=[&](double x,double y,double z){double size=std::sqrt(x*x+y*y+z*z);if(size<1e-100)throw std::runtime_error("Degenerate geometric tangent");double sign=determinant<0?-1:1;return Vec3{float(x/size*sign),float(y/size*sign),float(z/size*sign)};};
  auto raw_t=direction(e1.x*v2-e2.x*v1,e1.y*v2-e2.y*v1,e1.z*v2-e2.z*v1),
       raw_b=direction(e2.x*u1-e1.x*u2,e2.y*u1-e1.y*u2,e2.z*u1-e1.z*u2);
  auto tangent=raw_t-normal*dot(normal,raw_t);
  if (length(tangent)<1e-10f) throw std::runtime_error("Low normal has no valid UV tangent");
  tangent=normalized(tangent);
  auto bitangent=cross(normal,tangent)*(dot(cross(normal,tangent),raw_b)<0?-1.f:1.f);
  return {tangent,bitangent,normal};
}
std::array<float,4> sample(const Image &image, std::array<float,2> uv, bool repeat, bool srgb) {
  float x=uv[0]*image.width-.5f, y=uv[1]*image.height-.5f;
  if (!std::isfinite(x)||!std::isfinite(y)||std::abs(x)>1e9f||std::abs(y)>1e9f)
    throw std::runtime_error("Source UV sampling exceeds finite bounds");
  int left=int(std::floor(x)), top=int(std::floor(y)); float fx=x-left, fy=y-top;
  auto index=[&](int i,uint32_t limit){int n=int(limit);return repeat?((i%n)+n)%n:std::clamp(i,0,n-1);};
  std::array<float,4> result{};
  for (int dy=0;dy<2;++dy) for (int dx=0;dx<2;++dx) {
    auto at=(size_t(index(top+dy,image.height))*image.width+index(left+dx,image.width))*4;
    float weight=(dx?fx:1-fx)*(dy?fy:1-fy);
    for (int channel=0;channel<4;++channel) {
      float value=image.rgba.at(at+channel)/255.f;
      if (srgb&&channel<3) value=srgb_to_linear(value);
      result[channel]+=value*weight;
    }
  }
  return result;
}
uint8_t byte(float value) { return uint8_t(std::lround(std::clamp(value,0.f,1.f)*255)); }
struct Channels { std::array<float,4> color; std::array<float,3> emission; float roughness,metallic,ao=1; };
Channels material(const BakeMaterial &m, std::array<float,2> uv) {
  Channels result{m.material.color,m.material.emissive,m.material.roughness,m.material.metallic,1};
  if (!m.maps) return result;
  if(m.maps->source_only){
    const auto &raw=*m.maps->source_maps;
    auto color=sample_source_texture(raw.color,uv[0],uv[1],true),
         orm=sample_source_texture(raw.metal_rough,uv[0],uv[1],false),
         emission=sample_source_texture(raw.emission,uv[0],uv[1],true);
    for(int c=0;c<4;++c)result.color[c]*=color[c];
    for(int c=0;c<3;++c)result.emission[c]*=emission[c];
    result.roughness*=orm[1];result.metallic*=orm[2];
    if(raw.occlusion.image)result.ao=1-raw.occlusion_strength*(1-sample_source_texture(raw.occlusion,uv[0],uv[1],false)[0]);
    return result;
  }
  auto color=sample(m.maps->color,uv,m.repeat_uv,true), orm=sample(m.maps->metal_rough,uv,m.repeat_uv,false),
       emission=sample(m.maps->emission,uv,m.repeat_uv,true);
  for (int i=0;i<4;++i) result.color[i]=color[i]*(m.maps_are_composited?1:result.color[i]);
  for (int i=0;i<3;++i) result.emission[i]=emission[i]*(m.maps_are_composited?1:result.emission[i]);
  result.roughness=orm[1]*(m.maps_are_composited?1:result.roughness);
  result.metallic=orm[2]*(m.maps_are_composited?1:result.metallic); result.ao=orm[0]; return result;
}
float radical_inverse(uint32_t n) {
  n=(n<<16)|(n>>16); n=((n&0x55555555u)<<1)|((n&0xaaaaaaaau)>>1);
  n=((n&0x33333333u)<<2)|((n&0xccccccccu)>>2); n=((n&0x0f0f0f0fu)<<4)|((n&0xf0f0f0f0u)>>4);
  n=((n&0x00ff00ffu)<<8)|((n&0xff00ff00u)>>8); return float(double(n)/4294967296.0);
}
float ambient(const SurfaceQuery &query, Vec3 position, Vec3 normal, const BakeOptions &o, BakeResult &result) {
  if (!o.ao_samples) return 1;
  auto tangent=normalized(cross(std::abs(normal.x)<.8f?Vec3{1,0,0}:Vec3{0,1,0},normal));
  auto bitangent=cross(normal,tangent); uint32_t blocked=0;
  for (uint32_t i=0;i<o.ao_samples;++i) {
    if (!(i%16)) cancel(o);
    float u=(i+.5f)/o.ao_samples, angle=radical_inverse(i)*2*Pi;
    auto direction=tangent*(std::sqrt(u)*std::cos(angle))+bitangent*(std::sqrt(u)*std::sin(angle))+normal*std::sqrt(1-u);
    if (++result.rays>o.max_rays) throw std::length_error("Bake ray budget exceeded");
    blocked+=query.raycast(position+normal*o.ray_bias,direction,o.ao_distance).has_value();
  }
  return 1-float(blocked)/o.ao_samples;
}
double edge(std::array<float,2> a,std::array<float,2> b,std::array<float,2> p) {
  return (double(b[0])-a[0])*(double(p[1])-a[1])-(double(b[1])-a[1])*(double(p[0])-a[0]);
}
bool top_left(std::array<float,2> a,std::array<float,2> b) {
  return b[1]>a[1]||(b[1]==a[1]&&b[0]<a[0]);
}
void validate(const Mesh &low,const BakeSource &source,const BakeOptions &o) {
  if (low.positions.size()>2'000'000||low.indices.size()/3>2'000'000) throw std::length_error("Low bake mesh exceeds surface limits");
  low.validate(); source.mesh.validate();
  for (auto n:low.normals) if (length(n)<1e-8f) throw std::invalid_argument("Low bake mesh contains a zero normal");
  auto validate_tangents=[](const Mesh &mesh,const std::vector<std::array<float,4>> &tangents){
    if(tangents.empty())return;
    if(tangents.size()!=mesh.positions.size())throw std::invalid_argument("Tangent attribute count mismatch");
    for(auto v:tangents)if(!finite(Vec3{v[0],v[1],v[2]})||length(Vec3{v[0],v[1],v[2]})<1e-8f||!std::isfinite(v[3])||std::abs(std::abs(v[3])-1)>1e-5f)throw std::invalid_argument("Invalid tangent attribute");
    for(size_t t=0;t<mesh.indices.size();t+=3)if(tangents[mesh.indices[t]][3]*tangents[mesh.indices[t+1]][3]<0||tangents[mesh.indices[t]][3]*tangents[mesh.indices[t+2]][3]<0)throw std::invalid_argument("Tangent handedness changes inside a triangle");
  };
  validate_tangents(low,o.low_tangents);validate_tangents(source.mesh,source.tangents);
  if(!source.colors.empty()&&source.colors.size()!=source.mesh.positions.size())throw std::invalid_argument("Source color attribute count mismatch");
  for(auto value:source.colors)for(auto c:value)if(!std::isfinite(c)||c<0||c>1)throw std::invalid_argument("Invalid linear source vertex color");
  if (low.uv.size()!=low.positions.size()) throw std::invalid_argument("Baking requires low-mesh UV coordinates");
  for (auto uv:low.uv) if (uv[0]<0||uv[0]>1||uv[1]<0||uv[1]>1) throw std::invalid_argument("Low UVs must fit the unit texture tile");
  if (o.resolution<8||o.resolution>4096||uint64_t(o.resolution)*o.resolution>o.max_texels||o.max_texels>16'777'216||
      o.padding>64||o.ao_samples>256||!o.max_rays||o.max_rays>1'000'000'000||!o.max_raster_samples||o.max_raster_samples>1'000'000'000||
      !std::isfinite(o.front_distance)||!std::isfinite(o.back_distance)||o.front_distance<0||o.back_distance<0||
      o.front_distance+o.back_distance<=0||o.front_distance+o.back_distance>100||
      !std::isfinite(o.ray_bias)||o.ray_bias<=0||o.ray_bias>.1f||
      !std::isfinite(o.ao_distance)||o.ao_distance<0||(o.ao_samples&&o.ao_distance<=o.ray_bias)||o.ao_distance>100)
    throw std::invalid_argument("Invalid bake bounds or resource budget");
  if ((!o.writable.empty()&&o.writable.size()!=size_t(o.resolution)*o.resolution)||std::any_of(o.writable.begin(),o.writable.end(),[](auto v){return v>1;})) throw std::invalid_argument("Invalid bake writable mask");
  if (!o.cage_positions.empty()) {
    if (o.cage_positions.size()!=low.positions.size()) throw std::invalid_argument("Cage vertex identities differ from low mesh");
    for (size_t i=0;i<o.cage_positions.size();++i) {
      auto delta=o.cage_positions[i]-low.positions[i];
      if (!finite(o.cage_positions[i])||length(delta)>o.front_distance+1e-6f||dot(delta,normalized(low.normals[i]))<-1e-6f)
        throw std::invalid_argument("Cage exceeds outward projection bounds");
    }
  }
  if (source.materials.empty()||source.materials.size()>4096||(!source.triangle_materials.empty()&&source.triangle_materials.size()!=source.mesh.indices.size()/3))
    throw std::invalid_argument("Invalid source material assignments");
  for (auto id:source.triangle_materials) if (id>=source.materials.size()) throw std::out_of_range("Unknown source triangle material");
  for (const auto &m:source.materials) {
    m.material.validate();
    if (!std::isfinite(m.normal_strength)||m.normal_strength<0||m.normal_strength>4) throw std::invalid_argument("Invalid normal-map strength");
    if (m.maps) {
      if(m.maps->source_maps)m.maps->source_maps->validate();
      if(m.maps->source_only){
        if(!m.maps->source_maps||m.maps->source_maps->empty())throw std::invalid_argument("Raw bake source needs original texture bindings");
        if(m.maps->color.width||m.maps->metal_rough.width||m.maps->emission.width||m.maps->normal.width)throw std::invalid_argument("Raw bake source cannot also contain derived maps");
      }else{m.maps->color.validate(); m.maps->metal_rough.validate(); m.maps->emission.validate();}
      if(m.maps->pbr_color.width||m.maps->pbr_color.height||!m.maps->pbr_color.rgba.empty()) {
        if(m.maps->source_only||!m.maps_are_composited)throw std::invalid_argument("Separate baked source PBR colors require an explicit resolved composite");
        m.maps->pbr_color.validate();
      }
      if (!m.maps->normal.rgba.empty() || m.maps->normal.width || m.maps->normal.height)
        m.maps->normal.validate();
    }
    if (m.normal_map) m.normal_map->validate();
    if ((m.maps||m.normal_map)&&source.mesh.uv.size()!=source.mesh.positions.size()) throw std::invalid_argument("Textured high mesh requires UV coordinates");
  }
  if (!o.source_triangles.empty() || !o.source_corner_barycentrics.empty()) {
    if (o.source_triangles.size()!=low.indices.size()/3 || o.source_corner_barycentrics.size()!=low.indices.size())
      throw std::invalid_argument("Exact bake provenance count mismatch");
    for (size_t t=0;t<o.source_triangles.size();++t) {
      cancel(o);
      auto source_triangle=o.source_triangles[t];
      if(source_triangle>=source.mesh.indices.size()/3)throw std::out_of_range("Exact bake provenance source triangle is invalid");
      for(int k=0;k<3;++k){
        auto b=o.source_corner_barycentrics[t*3+k];float sum=0;
        for(auto value:b){if(!std::isfinite(value)||value<-.00001f||value>1.00001f)throw std::invalid_argument("Invalid exact bake barycentrics");sum+=value;}
        if(std::abs(sum-1)>1e-5f)throw std::invalid_argument("Exact bake barycentrics are not normalized");
        auto expected=interpolate(source.mesh,source_triangle,b,false),actual=low.positions[low.indices[t*3+k]];
        if(length(expected-actual)>1e-5f+1e-6f*length(actual))throw std::invalid_argument("Exact bake provenance does not match target geometry");
      }
    }
  }
}
void pad(BakeResult &result,const BakeOptions &o) {
  auto n=o.resolution; std::deque<std::pair<uint32_t,uint32_t>> queue;
  for (uint32_t i=0;i<result.coverage.size();++i) if (result.coverage[i]==1) queue.push_back({i,0});
  uint64_t visited=0;
  while (!queue.empty()) {
    auto [at,distance]=queue.front(); queue.pop_front();
    if (!(visited++%256)) cancel(o);
    if (distance>=o.padding) continue;
    int x=int(at%n),y=int(at/n);
    for (int dy=-1;dy<=1;++dy) for (int dx=-1;dx<=1;++dx) {
      if ((!dx&&!dy)||x+dx<0||x+dx>=int(n)||y+dy<0||y+dy>=int(n)) continue;
      auto target=uint32_t((y+dy)*int(n)+x+dx);
      if (result.coverage[target]) continue; // Never cover a miss or another island.
      for (auto *image:{&result.maps.color,&result.maps.metal_rough,&result.maps.emission,&result.normal,&result.ambient_occlusion})
        std::copy_n(image->rgba.data()+size_t(at)*4,4,image->rgba.data()+size_t(target)*4);
      if(o.separate_pbr_color)std::copy_n(result.maps.pbr_color.rgba.data()+size_t(at)*4,4,result.maps.pbr_color.rgba.data()+size_t(target)*4);
      result.coverage[target]=2; ++result.padded; queue.push_back({target,distance+1});
    }
  }
}
} // namespace
BakeResult bake_mesh(const Mesh &low,const BakeSource &source,const BakeOptions &o) {
  if(o.progress)o.progress(0);
  cancel(o); validate(low,source,o); SurfaceQuery query(source.mesh,o.cancelled); cancel(o);
  auto low_frames = o.low_tangents.empty() ? tangent_corners(low, o.cancelled)
                                         : std::vector<Tangent>{};
  std::vector<Tangent> high_frames;
  if (source.tangents.empty() && std::any_of(source.materials.begin(), source.materials.end(),
        [](const auto &material) { return normal_image(material) && material.normal_strength > 0; })) {
    high_frames = tangent_corners(source.mesh, o.cancelled);
    for (size_t t = 0; t < source.mesh.indices.size()/3; ++t) {
      auto id = source.triangle_materials.empty() ? 0 : source.triangle_materials[t];
      if (normal_image(source.materials[id]) && source.materials[id].normal_strength > 0)
        basis(source.mesh, t, normalized(source.mesh.normals[source.mesh.indices[t*3]]), {1,0,0});
    }
  }
  size_t pixels=size_t(o.resolution)*o.resolution;
  BakeResult result; result.coverage.resize(pixels);
  if (!o.writable.empty()) for (size_t i=0;i<pixels;++i) if (!o.writable[i]) result.coverage[i]=4;
  for (auto *image:{&result.maps.color,&result.maps.metal_rough,&result.maps.emission,&result.normal,&result.ambient_occlusion})
    *image={o.resolution,o.resolution,std::vector<uint8_t>(pixels*4)};
  if(o.separate_pbr_color)result.maps.pbr_color={o.resolution,o.resolution,std::vector<uint8_t>(pixels*4)};
  std::vector<uint32_t> owner(pixels,UINT32_MAX);
  for (size_t triangle=0;triangle<low.indices.size()/3;++triangle) {
    if(o.progress && triangle%64==0)o.progress(.05f+.85f*float(triangle)/float(low.indices.size()/3));
    cancel(o); std::array<std::array<float,2>,3> uv;
    for (int i=0;i<3;++i) uv[i]=low.uv[low.indices[triangle*3+i]];
    double area=edge(uv[0],uv[1],uv[2]),uvscale=std::hypot(double(uv[1][0])-uv[0][0],double(uv[1][1])-uv[0][1])*std::hypot(double(uv[2][0])-uv[0][0],double(uv[2][1])-uv[0][1]);
    if (!uvscale||std::abs(area)<=uvscale*1e-12) throw std::runtime_error("Low mesh contains a degenerate UV triangle");
    float minx=std::min({uv[0][0],uv[1][0],uv[2][0]}),maxx=std::max({uv[0][0],uv[1][0],uv[2][0]}),
          miny=std::min({uv[0][1],uv[1][1],uv[2][1]}),maxy=std::max({uv[0][1],uv[1][1],uv[2][1]});
    int left=std::max(0,int(std::ceil(minx*o.resolution-.5f))),right=std::min(int(o.resolution)-1,int(std::floor(maxx*o.resolution-.5f))),
        bottom=std::max(0,int(std::ceil(miny*o.resolution-.5f))),top=std::min(int(o.resolution)-1,int(std::floor(maxy*o.resolution-.5f)));
    for (int y=bottom;y<=top;++y) for (int x=left;x<=right;++x) {
      if (!(result.raster_samples++%64)) cancel(o);
      if (result.raster_samples>o.max_raster_samples) throw std::length_error("Bake UV raster budget exceeded");
      std::array<float,2> pixel{(x+.5f)/o.resolution,(y+.5f)/o.resolution};
      std::array<float,3> bary{float(edge(uv[1],uv[2],pixel)/area),float(edge(uv[2],uv[0],pixel)/area),float(edge(uv[0],uv[1],pixel)/area)};
      bool inside=true;
      for (int i=0;i<3;++i) {
        auto a=uv[(i+1)%3],b=uv[(i+2)%3]; if (area<0) std::swap(a,b);
        if (bary[i]<-1e-7f||(std::abs(bary[i])<=1e-7f&&!top_left(a,b))) inside=false;
      }
      if (!inside) continue;
      auto at=size_t(y)*o.resolution+x;
      if (result.coverage[at]==4) continue;
      if (owner[at]!=UINT32_MAX) { ++result.overlapping; continue; }
      owner[at]=uint32_t(triangle);
      auto position=interpolate(low,triangle,bary,false),normal=interpolate(low,triangle,bary,true);
      TangentFrame frame;
      if (o.low_tangents.empty()) {
        frame = tangent_frame(low, low_frames, triangle, bary);
        normal = normalized(frame.normal);
      } else {
        auto supplied = basis(low,triangle,normal,bary,o.low_tangents);
        frame = {supplied.tangent, supplied.bitangent, supplied.normal};
      }
      Vec3 offset=normal*o.front_distance;
      if (!o.cage_positions.empty()) {
        Vec3 cage; for (int i=0;i<3;++i) cage+=o.cage_positions[low.indices[triangle*3+i]]*bary[i]; offset=cage-position;
      }
      float front=length(offset); auto direction=front>1e-8f?offset/front:normal;
      std::optional<SurfaceHit> hit;
      if(o.source_triangles.empty()){
        if (++result.rays>o.max_rays) throw std::length_error("Bake ray budget exceeded");
        hit=query.raycast(position+offset,direction*-1,front+o.back_distance);
      }else{
        std::array<float,3> mapped{};
        for(int k=0;k<3;++k)for(int c=0;c<3;++c)mapped[c]+=bary[k]*o.source_corner_barycentrics[triangle*3+k][c];
        auto source_triangle=o.source_triangles[triangle];
        hit=SurfaceHit{source_triangle,interpolate(source.mesh,source_triangle,mapped,false),
                       interpolate(source.mesh,source_triangle,mapped,true),mapped,0};
      }
      bool backface=false;
      if (hit) {auto a=source.mesh.positions[source.mesh.indices[hit->triangle*3]],b=source.mesh.positions[source.mesh.indices[hit->triangle*3+1]],c=source.mesh.positions[source.mesh.indices[hit->triangle*3+2]];backface=dot(cross(b-a,c-a),normal)<=0;}
      if (!hit||(!o.allow_backfaces&&backface)) { result.coverage[at]=3; ++result.missed; continue; }
      auto material_id=source.triangle_materials.empty()?0:source.triangle_materials[hit->triangle];
      const auto &m=source.materials[material_id];
      auto high_uv=source.mesh.uv.empty()?std::array<float,2>{}:texcoord(source.mesh,hit->triangle,hit->barycentric);
      auto channels=material(m,high_uv); auto high_normal=normalized(hit->normal);
      auto pbr_color=channels.color;
      const bool resolved_source=m.maps&&!m.maps->pbr_color.rgba.empty();
      if(o.separate_pbr_color&&resolved_source)pbr_color=sample(m.maps->pbr_color,high_uv,false,true);
      if(!source.colors.empty()){
        std::array<float,4> tint{};for(int i=0;i<3;++i)for(int c=0;c<4;++c)tint[c]+=source.colors[source.mesh.indices[hit->triangle*3+i]][c]*hit->barycentric[i];
        for(int c=0;c<4;++c){if(!o.separate_pbr_color)channels.color[c]*=tint[c];else if(!resolved_source)pbr_color[c]*=tint[c];}
      }
      if (normal_image(m)&&m.normal_strength>0) {
        TangentFrame tangent;
        if (source.tangents.empty())
          tangent = tangent_frame(source.mesh, high_frames, hit->triangle, hit->barycentric);
        else {
          auto supplied = basis(source.mesh,hit->triangle,high_normal,hit->barycentric,source.tangents);
          tangent = {supplied.tangent, supplied.bitangent, supplied.normal};
        }
        auto sampled=sample(*normal_image(m),high_uv,m.repeat_uv,false);
        auto detail=Vec3{(sampled[0]*2-1)*m.normal_strength,(sampled[1]*2-1)*m.normal_strength,sampled[2]*2-1};
        detail=length(detail)>1e-8f?normalized(detail):Vec3{0,0,1};
        high_normal=tangent.to_world(detail);
      }
      else if(m.maps && m.maps->source_maps && m.normal_strength>0){
        const auto &raw=*m.maps->source_maps;
        if(source.tangents.empty() || !raw.normal.image || raw.normal_scale==0)
          high_normal=source_texture_normal(source.mesh,raw,hit->triangle,hit->barycentric,m.normal_strength);
        else {
          // Explicit imported frames remain authoritative; affine UV applies to
          // the lookup, while the supplied XYZ/handedness defines its basis.
          auto supplied=basis(source.mesh,hit->triangle,high_normal,hit->barycentric,source.tangents);
          auto encoded=sample_source_texture(raw.normal,high_uv[0],high_uv[1],false);
          double scale=double(raw.normal_scale)*m.normal_strength;
          double x=(encoded[0]*2-1)*scale,y=(encoded[1]*2-1)*scale,z=encoded[2]*2-1;
          double size=std::hypot(x,y,z);
          if(size>1e-20)high_normal=TangentFrame{supplied.tangent,supplied.bitangent,supplied.normal}.to_world(
            {float(x/size),float(y/size),float(z/size)});
        }
      }
      Vec3 mapped=frame.to_tangent(high_normal);
      if (o.normal_y_down) mapped.y=-mapped.y;
      float ao=ambient(query,hit->position,normalized(hit->normal),o,result)*channels.ao;
      for (int c=0;c<3;++c) {
        result.maps.color.rgba[at*4+c]=byte(linear_to_srgb(channels.color[c]));
        result.maps.emission.rgba[at*4+c]=byte(linear_to_srgb(channels.emission[c]));
      }
      result.maps.color.rgba[at*4+3]=byte(channels.color[3]);
      if(o.separate_pbr_color)for(int c=0;c<4;++c)result.maps.pbr_color.rgba[at*4+c]=byte(c<3?linear_to_srgb(pbr_color[c]):pbr_color[c]);
      result.maps.emission.rgba[at*4+3]=255;
      result.maps.metal_rough.rgba[at*4]=byte(ao); result.maps.metal_rough.rgba[at*4+1]=byte(channels.roughness);
      result.maps.metal_rough.rgba[at*4+2]=byte(channels.metallic); result.maps.metal_rough.rgba[at*4+3]=255;
      result.normal.rgba[at*4]=byte(mapped.x*.5f+.5f);result.normal.rgba[at*4+1]=byte(mapped.y*.5f+.5f);
      result.normal.rgba[at*4+2]=byte(mapped.z*.5f+.5f);result.normal.rgba[at*4+3]=255;
      for (int c=0;c<3;++c) result.ambient_occlusion.rgba[at*4+c]=byte(ao);
      result.ambient_occlusion.rgba[at*4+3]=255;
      result.maps.transparent|=channels.color[3]<.9999f;
      result.coverage[at]=1; ++result.projected;
    }
  }
  if (result.overlapping&&o.fail_on_overlap) throw std::runtime_error("Overlapping low UV coverage: "+std::to_string(result.overlapping)+" texels");
  if (result.missed&&o.fail_on_miss) throw std::runtime_error("Projection missed "+std::to_string(result.missed)+" low UV texels");
  if (!result.projected) throw std::runtime_error("No low UV texels projected onto the high source");
  if(o.progress)o.progress(.9f);
  cancel(o); if (o.padding) pad(result,o); cancel(o);
  if(o.progress)o.progress(1);
  return result;
}
} // namespace dome
