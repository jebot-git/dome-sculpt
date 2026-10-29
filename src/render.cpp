// SPDX-License-Identifier: GPL-3.0-or-later
// Matrix helpers adapted from OpenBrush Native (Apache-2.0).
#include "dome/render.hpp"
#include <cstddef>
#include "dome/source-maps.hpp"
#include "dome/humanoid-creator.hpp"
#include "render-mtoon.hpp"
#include "render-target.hpp"
#include <chrono>
#include <unordered_set>
namespace dome {
Mat4 model_matrix(CanvasTransform t) {
  auto x = ob::rotate(t.orientation, {t.scale, 0, 0}),
       y = ob::rotate(t.orientation, {0, t.scale, 0}),
       z = ob::rotate(t.orientation, {0, 0, t.scale});
  return {x.x, x.y, x.z, 0, y.x,          y.y,          y.z,          0,
          z.x, z.y, z.z, 0, t.position.x, t.position.y, t.position.z, 1};
}
Mat4 view_matrix(ob::Vec3 p, ob::Quat q) {
  auto inv = ob::conjugate(q);
  auto result = model_matrix({ob::rotate(inv, p) * -1, inv, 1});
  return result;
}
Mat4 multiply(const Mat4 &a, const Mat4 &b) {
  Mat4 c{};
  for (int col = 0; col < 4; ++col)
    for (int row = 0; row < 4; ++row)
      for (int i = 0; i < 4; ++i)
        c[col * 4 + row] += a[i * 4 + row] * b[col * 4 + i];
  return c;
}
Mat4 projection(float l, float r, float b, float t, float n, float f) {
  l = std::tan(l);
  r = std::tan(r);
  b = std::tan(b);
  t = std::tan(t);
  return {2 / (r - l),
          0,
          0,
          0,
          0,
          2 / (t - b),
          0,
          0,
          (r + l) / (r - l),
          (t + b) / (t - b),
          -(f + n) / (f - n),
          -1,
          0,
          0,
          -2 * f * n / (f - n),
          0};
}
namespace {
GLuint shader(GLenum kind, const char *text) {
  GLuint s = glCreateShader(kind);
  glShaderSource(s, 1, &text, nullptr);
  glCompileShader(s);
  GLint good = 0;
  glGetShaderiv(s, GL_COMPILE_STATUS, &good);
  if (!good) {
    char log[4096];
    glGetShaderInfoLog(s, sizeof(log), nullptr, log);
    glDeleteShader(s);
    throw std::runtime_error(log);
  }
  return s;
}
} // namespace
namespace {
std::optional<MToonMaterial> retained_mtoon(const CreatorAppearance *appearance){
  if(!appearance)return {};
  auto parse=[](const std::string&s){if(s.size()>2*1024*1024)throw std::length_error("MToon source metadata exceeds renderer bounds");size_t count=0;return nlohmann::json::parse(s,[&](int depth,nlohmann::json::parse_event_t,const nlohmann::json&){if(depth>32||++count>100000)throw std::length_error("MToon source metadata exceeds renderer bounds");return true;});};
  std::optional<MToonMaterial> result;
  if(!appearance->mtoon_json.empty()){auto j=parse(appearance->mtoon_json);if(j.value("shader",std::string{})=="VRM/MToon")result=MToonMaterial::from_vrm0(j,appearance->asset?appearance->asset->textures.size():0);}
  if(!result&&!appearance->material_json.empty()){auto j=parse(appearance->material_json);if(j.contains("extensions")&&j["extensions"].contains("VRMC_materials_mtoon"))result=MToonMaterial::from_vrm1(j,appearance->asset?appearance->asset->textures.size():0);}
  if(result&&(!appearance->asset||!appearance->decoded_images||appearance->decoded_images->size()!=appearance->asset->images.size()||appearance->material<0||size_t(appearance->material)>=appearance->asset->materials.size()))throw std::runtime_error("MToon retained image/material source is incomplete");
  return result;
}
int mtoon_coordinate(const MToonTexture&t){int coordinate=t.transform_texcoord>=0?t.transform_texcoord:int(t.texcoord);if(coordinate<0||coordinate>1)throw std::runtime_error("MToon preview requires a retained UV0 or UV1 set");return coordinate;}
bool mtoon_shade_alias(const MToonMaterial&m){auto &a=m.textures[0],&b=m.textures[1];return a.index>=0&&a.index==b.index&&mtoon_coordinate(a)==mtoon_coordinate(b)&&(m.version()==MToonVersion::V0||(a.offset==b.offset&&a.scale==b.scale&&a.rotation==b.rotation));}
float mtoon_factor(float typed,float original,float current){if(current==original)return typed;if(original!=0)return typed*current/original;return current==0?typed:current;}
GLenum mtoon_blend(int value){constexpr GLenum factors[]={GL_ZERO,GL_ONE,GL_DST_COLOR,GL_SRC_COLOR,GL_ONE_MINUS_DST_COLOR,GL_SRC_ALPHA,GL_ONE_MINUS_SRC_COLOR,GL_DST_ALPHA,GL_ONE_MINUS_DST_ALPHA,GL_SRC_ALPHA_SATURATE,GL_ONE_MINUS_SRC_ALPHA};if(value<0||value>10)throw std::runtime_error("Unsupported MToon blend factor");return factors[value];}
}

struct SurfaceRenderer::TextureState {
  struct Key {const Image *image=nullptr;bool srgb=false;bool operator==(const Key&)const=default;};
  struct KeyHash {size_t operator()(const Key&k)const{return std::hash<const Image*>{}(k.image)^(size_t(k.srgb)<<1);}};
  using Sampler=std::array<int,4>;
  struct SamplerHash {size_t operator()(const Sampler&s)const{size_t h=0;for(int n:s)h=h*131+size_t(n);return h;}};
  struct ImageResource {std::shared_ptr<const Image> owner;GLuint id=0;bool mips=false;size_t bytes=0;};
  struct Slot {GLuint image=0,sampler=0;std::array<float,6> uv{1,0,0,1,0,0};};
  using Binding=std::array<Slot,16>;
  struct Toon {std::shared_ptr<const CreatorAppearance> appearance;MToonMaterial material;bool derived=false,shade_alias=false,baked_normal=false;};
  std::unordered_map<uint64_t,Toon> toons;
  std::vector<std::array<float,2>> uv1;
  std::unordered_set<uint64_t> weighted;
  std::optional<double> frame_time;
  std::chrono::steady_clock::time_point started=std::chrono::steady_clock::now();
  SurfaceTextureLimits limits;
  LinearSceneTarget target;
  SurfaceOutputEncoding output=SurfaceOutputEncoding::Srgb;
  bool deferred=false;
  uint64_t uploads=0;
  std::unordered_map<Key,ImageResource,KeyHash> images;
  std::unordered_map<Sampler,GLuint,SamplerHash> samplers;
  std::unordered_map<uint64_t,Binding> bindings;
  explicit TextureState(SurfaceTextureLimits l):limits(l){
    if(!l.max_bytes||l.max_bytes>2ull*1024*1024*1024||!l.max_images||l.max_images>2048||!l.max_samplers||l.max_samplers>256)
      throw std::invalid_argument("Invalid GPU source texture resource bounds");
  }
  static size_t storage(const Image&i,bool mips){size_t bytes=0;uint32_t w=i.width,h=i.height;for(;;){bytes+=size_t(w)*h*4;if(!mips||(w==1&&h==1))return bytes;w=std::max(1u,w/2);h=std::max(1u,h/2);}}
  static Sampler sampler(const PaintSourceTexture&s){return {s.wrap_s,s.wrap_t,s.min_filter?s.min_filter:GL_LINEAR,s.mag_filter?s.mag_filter:GL_LINEAR};}
  static bool mipmapped(const Sampler&s){return s[2]>=9984&&s[2]<=9987;}
  SurfaceTextureStats stats()const{size_t bytes=0;for(auto &[key,r]:images)bytes+=r.bytes;return {images.size(),samplers.size(),bytes,uploads};}
  void clear(bool release){if(release){for(auto &[key,r]:images)glDeleteTextures(1,&r.id);for(auto &[key,id]:samplers)glDeleteSamplers(1,&id);}bindings.clear();images.clear();samplers.clear();toons.clear();uv1.clear();weighted.clear();}
  void prepare(const MeshSnapshot&mesh){
    if(!mesh.tangents.empty()&&mesh.tangents.size()!=mesh.mesh.positions.size())throw std::invalid_argument("Canonical tangent count differs from packed geometry");
    for(auto t:mesh.tangents)if(!finite(Vec3{t[0],t[1],t[2]})||length(Vec3{t[0],t[1],t[2]})<1e-8f||!std::isfinite(t[3])||std::abs(std::abs(t[3])-1)>1e-5f)throw std::invalid_argument("Invalid canonical tangent frame");
    if(!mesh.selected_weights.empty()&&mesh.selected_weights.size()!=mesh.mesh.positions.size())throw std::runtime_error("Weight visualization size differs from current geometry");
    for(float w:mesh.selected_weights)if(!std::isfinite(w)||(w!=-1&&(w<0||w>1)))throw std::runtime_error("Invalid weight visualization value");
    std::unordered_set<uint64_t> next_weighted;
    if(!mesh.selected_weights.empty())for(auto &r:mesh.ranges){
      if(r.first>mesh.mesh.indices.size()||r.count>mesh.mesh.indices.size()-r.first)throw std::runtime_error("Weight visualization range is invalid");
      for(size_t i=r.first;i<r.first+r.count;++i){auto index=mesh.mesh.indices[i];if(index>=mesh.selected_weights.size())throw std::runtime_error("Weight visualization vertex is invalid");if(mesh.selected_weights[index]>=0)next_weighted.insert(r.object);}
    }
    struct Desired {std::shared_ptr<const Image> owner;bool mips=false;};
    struct PlanSlot {Key key;Sampler sampler;std::array<float,6>uv;bool present=false;};
    std::unordered_map<Key,Desired,KeyHash> wanted;
    std::unordered_set<Sampler,SamplerHash> wanted_samplers;
    std::unordered_map<uint64_t,std::array<PlanSlot,16>> planned;
    std::unordered_map<uint64_t,Toon> next_toons;std::vector<std::array<float,2>> next_uv1;
    for(auto &range:mesh.ranges){
      auto maps=range.textures;auto &plan=planned[range.object];
      auto slot=[&](int index,std::shared_ptr<const Image> image,const PaintSourceTexture&view,bool srgb){
        if(!image)return;
        view.validate();image->validate();Key key{image.get(),srgb};auto sample=sampler(view);
        auto &desired=wanted[key];desired.owner=std::move(image);desired.mips|=mipmapped(sample);
        wanted_samplers.insert(sample);plan[index]={key,sample,view.uv_transform,true};
      };
      std::optional<MToonMaterial> typed;
      if(auto previous=toons.find(range.object);previous!=toons.end()&&previous->second.appearance==range.appearance)typed=previous->second.material;
      else typed=retained_mtoon(range.appearance.get());
      if(typed){
        Toon toon{range.appearance,*typed,maps&&!maps->source_only,mtoon_shade_alias(*typed)};
        const auto &appearance=*range.appearance;const auto &asset=*appearance.asset;
        bool needs_uv1=false;
        auto original=[&](int destination,int role){const auto &t=typed->textures[role];if(t.index<0)return;int coordinate=mtoon_coordinate(t);needs_uv1|=coordinate==1;
          const auto &texture=asset.textures.at(size_t(t.index));if(texture.image<0||size_t(texture.image)>=appearance.decoded_images->size())throw std::runtime_error("MToon image reference is missing");
          PaintSourceTexture view;view.image=appearance.decoded_images->at(texture.image);view.wrap_s=texture.wrap_s;view.wrap_t=texture.wrap_t;view.min_filter=texture.min_filter;view.mag_filter=texture.mag_filter;
          slot(destination,view.image,view,t.encoding==MToonEncoding::Srgb);
        };
        for(int role=0;role<11;++role)original(role,role);
        if(toon.derived){
          if(!maps->source_maps)throw std::runtime_error("Derived MToon preview lacks its original source views");
          maps->color_coverage.validate();
          original(12,0);
          auto derived=[&](int destination,const Image&image,const PaintSourceTexture&original,bool srgb){auto view=original;view.uv_transform={1,0,0,1,0,0};if(!view.image)view.wrap_s=view.wrap_t=GL_CLAMP_TO_EDGE;slot(destination,std::shared_ptr<const Image>(maps,&image),view,srgb);};
          derived(0,maps->color,maps->source_maps->color,true);derived(3,maps->emission,maps->source_maps->emission,true);
          derived(11,maps->color_coverage,maps->source_maps->color,false);
          slot(14,maps->source_maps->color.image,maps->source_maps->color,true);
          if(!maps->normal.rgba.empty()){
            if(!maps->baked_base||maps->baked_normal.rgba.empty()||maps->baked_coverage.rgba.empty()||mesh.tangents.size()!=mesh.mesh.positions.size())throw std::invalid_argument("Typed baked normals require original canonical data, coverage and worker tangent corners");
            PaintSourceTexture view;view.wrap_s=view.wrap_t=GL_CLAMP_TO_EDGE;
            slot(13,std::shared_ptr<const Image>(maps,&maps->baked_normal),view,false);slot(15,std::shared_ptr<const Image>(maps,&maps->baked_coverage),view,false);toon.baked_normal=true;
          }
        }
        if(needs_uv1){
          const ImportedPrimitive *primitive=nullptr;for(const auto &p:asset.primitives)if(p.source_mesh==appearance.source_mesh&&p.source_primitive==appearance.source_primitive){if(primitive)throw std::runtime_error("MToon source primitive identity is ambiguous");primitive=&p;}
          if(!primitive||primitive->uv1.size()!=primitive->mesh.positions.size()||appearance.source_vertices.empty()||range.first_vertex>mesh.mesh.positions.size()||appearance.source_vertices.size()>mesh.mesh.positions.size()-range.first_vertex)throw std::runtime_error("MToon selected UV1 data/remap is unavailable");
          if(next_uv1.empty())next_uv1.resize(mesh.mesh.positions.size());
          for(size_t n=0;n<appearance.source_vertices.size();++n){auto source=appearance.source_vertices[n];if(source>=primitive->uv1.size())throw std::runtime_error("MToon UV1 authored vertex reference is invalid");next_uv1[range.first_vertex+n]=primitive->uv1[source];}
          if(range.first>mesh.mesh.indices.size()||range.count>mesh.mesh.indices.size()-range.first)throw std::runtime_error("MToon packed index range is invalid");
          for(size_t n=range.first;n<range.first+range.count;++n)if(mesh.mesh.indices[n]<range.first_vertex||mesh.mesh.indices[n]>=range.first_vertex+appearance.source_vertices.size())throw std::runtime_error("MToon packed source vertex interval is invalid");
        }
        next_toons.emplace(range.object,std::move(toon));continue;
      }
      if(!maps)continue;
      if(maps->source_only){
        if(!maps->source_maps||maps->source_maps->empty()||maps->color.width||maps->metal_rough.width||maps->emission.width||maps->normal.width)
          throw std::runtime_error("Invalid raw source preview marker");
        maps->source_maps->validate();auto &s=*maps->source_maps;
        slot(0,s.color.image,s.color,true);slot(1,s.metal_rough.image,s.metal_rough,false);slot(2,s.emission.image,s.emission,true);slot(3,s.normal.image,s.normal,false);slot(4,s.occlusion.image,s.occlusion,false);
      }else{
        PaintSourceTexture view;view.wrap_s=view.wrap_t=GL_CLAMP_TO_EDGE;
        auto color=view,emission=view;if(maps->source_maps){if(maps->source_maps->color.image)color=maps->source_maps->color;if(maps->source_maps->emission.image)emission=maps->source_maps->emission;}
        color.uv_transform=emission.uv_transform={1,0,0,1,0,0};
        slot(0,std::shared_ptr<const Image>(maps,&maps->color),color,true);slot(1,std::shared_ptr<const Image>(maps,&maps->metal_rough),view,false);slot(2,std::shared_ptr<const Image>(maps,&maps->emission),emission,true);
        if(!maps->normal.rgba.empty()){
          if(mesh.tangents.size()!=mesh.mesh.positions.size())throw std::invalid_argument("Normal-mapped snapshots require worker-generated tangent corners");
          slot(3,std::shared_ptr<const Image>(maps,&maps->normal),view,false);
        }else if(maps->source_maps)slot(3,maps->source_maps->normal.image,maps->source_maps->normal,false);
      }
    }
    if(wanted.size()>limits.max_images||wanted_samplers.size()>limits.max_samplers)throw std::length_error("GPU source texture count budget exceeded");
    size_t bytes=0;GLint maximum=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maximum);
    for(auto &[key,desired]:wanted){
      if(auto previous=images.find(key);previous!=images.end())desired.mips|=previous->second.mips;
      auto &i=*desired.owner;if(i.width>uint32_t(maximum)||i.height>uint32_t(maximum))throw std::length_error("Original image exceeds hardware texture dimensions");
      size_t count=storage(i,desired.mips);if(count>limits.max_bytes-bytes)throw std::length_error("GPU original texture byte budget exceeded");bytes+=count;
    }
    for(auto it=images.begin();it!=images.end();)if(!wanted.contains(it->first)){glDeleteTextures(1,&it->second.id);it=images.erase(it);}else ++it;
    for(auto it=samplers.begin();it!=samplers.end();)if(!wanted_samplers.contains(it->first)){glDeleteSamplers(1,&it->second);it=samplers.erase(it);}else ++it;
    glActiveTexture(GL_TEXTURE0);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    for(auto &[key,desired]:wanted){
      auto found=images.find(key);
      if(found==images.end()){
        ImageResource resource;resource.owner=desired.owner;glGenTextures(1,&resource.id);glBindTexture(GL_TEXTURE_2D,resource.id);
        glTexImage2D(GL_TEXTURE_2D,0,key.srgb?GL_SRGB8_ALPHA8:GL_RGBA8,resource.owner->width,resource.owner->height,0,GL_RGBA,GL_UNSIGNED_BYTE,resource.owner->rgba.data());
        if(glGetError()!=GL_NO_ERROR){glDeleteTextures(1,&resource.id);throw std::runtime_error("Original GPU texture allocation failed");}
        try{found=images.emplace(key,std::move(resource)).first;}catch(...){glDeleteTextures(1,&resource.id);throw;}++uploads;
      }
      auto &resource=found->second;
      if(desired.mips&&!resource.mips){glBindTexture(GL_TEXTURE_2D,resource.id);glGenerateMipmap(GL_TEXTURE_2D);if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("Original GPU mip generation failed");resource.mips=true;}
      resource.bytes=storage(*resource.owner,resource.mips);
    }
    for(auto &sample:wanted_samplers)if(!samplers.contains(sample)){
      GLuint id=0;glGenSamplers(1,&id);glSamplerParameteri(id,GL_TEXTURE_WRAP_S,sample[0]);glSamplerParameteri(id,GL_TEXTURE_WRAP_T,sample[1]);glSamplerParameteri(id,GL_TEXTURE_MIN_FILTER,sample[2]);glSamplerParameteri(id,GL_TEXTURE_MAG_FILTER,sample[3]);
      if(glGetError()!=GL_NO_ERROR){glDeleteSamplers(1,&id);throw std::runtime_error("Original GPU sampler allocation failed");}
      try{samplers.emplace(sample,id);}catch(...){glDeleteSamplers(1,&id);throw;}
    }
    std::unordered_map<uint64_t,Binding> ready;
    for(auto &[object,plan]:planned){auto &binding=ready[object];for(int n=0;n<16;++n)if(plan[n].present)binding[n]={images.at(plan[n].key).id,samplers.at(plan[n].sampler),plan[n].uv};}
    bindings=std::move(ready);toons=std::move(next_toons);uv1=std::move(next_uv1);weighted=std::move(next_weighted);
  }
};
SurfaceRenderer::SurfaceRenderer():SurfaceRenderer(SurfaceTextureLimits{}){}
SurfaceTextureStats SurfaceRenderer::texture_stats()const{return textures_->stats();}
void SurfaceRenderer::set_output_encoding(SurfaceOutputEncoding encoding){if(textures_->target.pending())throw std::logic_error("Resolve current view before changing output encoding");textures_->output=encoding;}
void SurfaceRenderer::set_deferred_resolve(bool value){if(textures_->target.pending())throw std::logic_error("Resolve current view before changing deferred output");textures_->deferred=value;}
void SurfaceRenderer::resolve(){textures_->target.resolve(textures_->output);}
void SurfaceRenderer::set_frame_time(double seconds){if(!std::isfinite(seconds)||seconds<0||seconds>std::numeric_limits<float>::max())throw std::invalid_argument("Invalid GPU animation frame time");textures_->frame_time=seconds;}
void SurfaceRenderer::abandon(){program_=vao_=vertices_=indices_=0;cached_.reset();textures_->clear(false);textures_->target.clear(false);count_=0;}

SurfaceRenderer::SurfaceRenderer(SurfaceTextureLimits limits):textures_(std::make_unique<TextureState>(limits)) {
  auto vertex=mtoon_renderer_vertex();
  std::string fragment="#version 330 core\n"+mtoon_renderer_common()+R"GLSL(
in vec3 world,n,canonical_t,canonical_b;in vec2 texcoord,texcoord1;in float weight_value;uniform int weight_pass;out vec4 color;uniform vec3 camera;uniform int toon,unlit,alpha_mode;uniform float alpha_cutoff;uniform vec4 base_color;uniform float metallic,roughness;uniform vec3 emission;uniform int map_mode,slots[5],baked_normal;uniform vec3 uv_x[5],uv_y[5];uniform float normal_scale,ao_strength;
)GLSL"+mtoon_renderer_fragment()+R"GLSL(
const float PI=3.141592653589793;
vec3 shade(vec3 N,vec3 V,vec3 L,vec3 power,vec3 base,float roughness,float metallic){
vec3 H=normalize(V+L);float NV=max(dot(N,V),0.0001),NL=max(dot(N,L),0.0),NH=max(dot(N,H),0.0),VH=max(dot(V,H),0.0);
float rough=max(.04,roughness),a=rough*rough,a2=a*a;
float d=a2/(PI*pow(NH*NH*(a2-1.0)+1.0,2.0));float k=pow(rough+1.0,2.0)/8.0;
float g=NV/(NV*(1.0-k)+k)*NL/(NL*(1.0-k)+k);vec3 F0=mix(vec3(.04),base,metallic);vec3 F=F0+(1.0-F0)*pow(1.0-VH,5.0);
return ((1.0-F)*(1.0-metallic)*base/PI+d*g*F/max(4.0*NV*NL,.0001))*power*NL;}
vec3 output_srgb(vec3 c){return mix(1.055*pow(max(c,vec3(0)),vec3(1.0/2.4))-.055,12.92*c,lessThanEqual(c,vec3(.0031308)));}
vec2 source_uv(int slot){return vec2(dot(uv_x[slot],vec3(texcoord,1)),dot(uv_y[slot],vec3(texcoord,1)));}
vec3 unit(vec3 v){float m=max(max(abs(v.x),abs(v.y)),abs(v.z));return m>1e-20?normalize(v/m):vec3(0,0,1);}
vec3 surface_normal(){
vec3 N=unit(n);if(slots[3]==1&&baked_normal==1){vec3 detail=texture(tex3,texcoord).xyz*2.0-1.0;N=unit(canonical_t*detail.x+canonical_b*detail.y+n*detail.z);}
else if(slots[3]==1){vec2 uv=source_uv(3),a=dFdx(uv),b=dFdy(uv);vec3 x=dFdx(world),y=dFdy(world);float determinant=a.x*b.y-a.y*b.x;
if(abs(determinant)>1e-8*max(length(a)*length(b),1e-20)){
vec3 T=(x*b.y-y*a.y)/determinant,B=(y*a.x-x*b.x)/determinant;T=unit(T-N*dot(N,T));B=unit(cross(N,T))*(dot(cross(N,T),B)<0.0?-1.0:1.0);
vec3 encoded=texture(tex3,uv).xyz;vec3 ts=unit(vec3((encoded.xy*2.0-1.0)*normal_scale,encoded.z*2.0-1.0));N=unit(T*ts.x+B*ts.y+N*ts.z);}}
return gl_FrontFacing?N:-N;}
void main(){if(weight_pass==1){if(weight_value<0)discard;float x=clamp(weight_value,0,1)*4;vec3 ramp=x<1?mix(vec3(0,0,1),vec3(0,1,1),x):x<2?mix(vec3(0,1,1),vec3(0,1,0),x-1):x<3?mix(vec3(0,1,0),vec3(1,1,0),x-2):mix(vec3(1,1,0),vec3(1,0,0),x-3);color=vec4(ramp,1);return;}if(weight_value>=0)discard;if(mt_mode==1){bool killed;vec4 result=mt_surface(n,unit(camera-world),killed);if(killed)discard;color=vec4(result.rgb,result.a);return;}vec3 shading_normal=unlit==0?surface_normal():vec3(0,0,1);vec4 base=base_color;float metal=metallic,rough=roughness,ao=1.0;vec3 glow=emission;
if(slots[0]==1)base*=texture(tex0,source_uv(0));
if(slots[1]==1){vec3 orm=texture(tex1,source_uv(1)).rgb;metal*=orm.b;rough*=orm.g;if(map_mode==1)ao=orm.r;}
if(slots[2]==1)glow*=texture(tex2,source_uv(2)).rgb;
if(map_mode==2&&slots[4]==1)ao=mix(1.0,texture(tex4,source_uv(4)).r,ao_strength);if(alpha_mode==1)base.a=1.0;else if(alpha_mode==2){if(base.a<alpha_cutoff)discard;base.a=1.0;}
if(unlit==1){color=vec4(base.rgb+glow,base.a);return;}
vec3 N=shading_normal,V=normalize(camera-world),result;
if(toon==1){float lit=dot(N,normalize(vec3(-.4,.8,.7)));float ramp=mix(.32*ao,1.0,smoothstep(-.05,.05,lit));float edge=smoothstep(.05,.18,max(dot(N,V),0.0));result=base.rgb*ramp*edge+vec3(.025);}
else result=shade(N,V,normalize(vec3(-.4,.8,.7)),vec3(3.8),base.rgb,rough,metal)+shade(N,V,normalize(vec3(.7,.2,-.4)),vec3(.65,.8,1.0),base.rgb,rough,metal)+.045*base.rgb*ao;
result+=glow;result=result/(result+vec3(1));color=vec4(result,base.a);})GLSL";
  GLuint vs = shader(GL_VERTEX_SHADER, vertex.c_str()),
         fs = shader(GL_FRAGMENT_SHADER, fragment.c_str());
  program_ = glCreateProgram();
  glAttachShader(program_, vs);
  glAttachShader(program_, fs);
  glLinkProgram(program_);
  glDeleteShader(vs);
  glDeleteShader(fs);
  GLint good = 0;
  glGetProgramiv(program_, GL_LINK_STATUS, &good);
  if (!good)
    throw std::runtime_error("Sculpt shader linking failed");
  glGenVertexArrays(1, &vao_);
  glGenBuffers(1, &vertices_);
  glGenBuffers(1, &indices_);
}
SurfaceRenderer::~SurfaceRenderer() {
  textures_->target.clear(true);
  textures_->clear(true);
  glDeleteBuffers(1, &indices_);
  glDeleteBuffers(1, &vertices_);
  glDeleteVertexArrays(1, &vao_);
  glDeleteProgram(program_);
}
void SurfaceRenderer::draw(std::shared_ptr<const MeshSnapshot> mesh,
                           CanvasTransform model, const Mat4 &vp,
                           ob::Vec3 camera, int width, int height,
                           bool transparent, bool toon, bool light) {
  if(!mesh)throw std::invalid_argument("Missing sculpt mesh snapshot");
  try{
  textures_->target.begin(width,height,textures_->output);
  glViewport(0, 0, width, height);
  glDisable(GL_FRAMEBUFFER_SRGB);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);
  float background = transparent ? 0.f : srgb_to_linear(light ? 208.f / 255 : 48.f / 255);
  glClearColor(background, background, background, transparent ? 0 : 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  if (mesh != cached_) {
    try{textures_->prepare(*mesh);}catch(...){textures_->clear(true);cached_.reset();count_=0;throw;}
    struct Vertex {
      Vec3 p, n;
      std::array<float, 2> uv,uv1;
      float weight=-1;
      std::array<float,4> tangent;
    };
    std::vector<Vertex> v;
    v.reserve(mesh->mesh.positions.size());
    for (size_t i = 0; i < mesh->mesh.positions.size(); ++i)
      v.push_back(
          {mesh->mesh.positions[i], mesh->mesh.normals[i],
           mesh->mesh.uv.empty() ? std::array<float, 2>{} : mesh->mesh.uv[i],textures_->uv1.empty()?std::array<float,2>{}:textures_->uv1[i],mesh->selected_weights.empty()?-1:mesh->selected_weights[i],mesh->tangents.empty()?std::array<float,4>{1,0,0,1}:mesh->tangents[i]});
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vertices_);
    glBufferData(GL_ARRAY_BUFFER, v.size() * sizeof(Vertex), v.data(),
                 GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void *>(offsetof(Vertex, n)));
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<void *>(offsetof(Vertex, uv)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,uv1)));glEnableVertexAttribArray(3);
    glVertexAttribPointer(4,1,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,weight)));glEnableVertexAttribArray(4);
    glVertexAttribPointer(5,4,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,tangent)));glEnableVertexAttribArray(5);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 mesh->mesh.indices.size() * sizeof(uint32_t),
                 mesh->mesh.indices.data(), GL_STATIC_DRAW);
    count_ = mesh->mesh.indices.size();
    cached_ = std::move(mesh);
  }
  glUseProgram(program_);
  glUniform1i(glGetUniformLocation(program_,"weight_pass"),0);
  auto m = model_matrix(model), mvp = multiply(vp, m);
  glUniformMatrix4fv(glGetUniformLocation(program_, "model"), 1, GL_FALSE,
                     m.data());
  glUniformMatrix4fv(glGetUniformLocation(program_, "mvp"), 1, GL_FALSE,
                     mvp.data());
  glUniform3f(glGetUniformLocation(program_, "camera"), camera.x, camera.y,
              camera.z);
  glUniformMatrix4fv(glGetUniformLocation(program_,"view_projection"),1,GL_FALSE,vp.data());
  Vec3 forward{vp[3],vp[7],vp[11]},up{vp[1],vp[5],vp[9]};
  if(length(forward)>1e-8f){forward=normalized(forward);up=up-forward*dot(up,forward);}else forward={0,0,-1};up=normalized(up);
  glUniform3f(glGetUniformLocation(program_,"mt_camera_up"),up.x,up.y,up.z);glUniform3f(glGetUniformLocation(program_,"mt_camera_forward"),forward.x,forward.y,forward.z);
  glUniform1f(glGetUniformLocation(program_,"mt_aspect"),float(width)/height);
  auto seconds=textures_->frame_time.value_or(std::chrono::duration<double>(std::chrono::steady_clock::now()-textures_->started).count());glUniform1f(glGetUniformLocation(program_,"mt_time"),float(seconds));
  glBindVertexArray(vao_);
  auto blended = [](const Material &m, const TextureMaps *maps) {
    if (m.alpha == Material::AlphaMode::Blend) return true;
    if (m.alpha != Material::AlphaMode::Automatic) return false;
    return m.color[3] < 1 || (maps && maps->transparent);
  };
  auto draw_range = [&](const Material &original, size_t first, size_t count,
                        uint64_t id, const TextureMaps *maps) {
    Material material = original;
    glDepthFunc(GL_LESS);glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    glUniform1i(glGetUniformLocation(program_,"mt_mode"),0);glUniform1i(glGetUniformLocation(program_,"mt_outline"),0);
    if (maps&&!maps->source_only) {
      material.color={1,1,1,1};material.metallic=material.roughness=1;material.emissive={1,1,1};
    }
    std::array<GLint,5> present{};std::array<float,15> ux{},uy{};
    for(int n=0;n<5;++n){
      TextureState::Slot slot;if(maps)slot=textures_->bindings.at(id)[n];
      glActiveTexture(GL_TEXTURE0+n);glBindTexture(GL_TEXTURE_2D,slot.image);glBindSampler(n,slot.sampler);present[n]=slot.image!=0;
      ux[n*3]=slot.uv[0];ux[n*3+1]=slot.uv[1];ux[n*3+2]=slot.uv[4];uy[n*3]=slot.uv[2];uy[n*3+1]=slot.uv[3];uy[n*3+2]=slot.uv[5];
    }
    glUniform1iv(glGetUniformLocation(program_,"slots[0]"),5,present.data());
    glUniform3fv(glGetUniformLocation(program_,"uv_x[0]"),5,ux.data());glUniform3fv(glGetUniformLocation(program_,"uv_y[0]"),5,uy.data());
    glUniform1i(glGetUniformLocation(program_,"map_mode"),maps?(maps->source_only?2:1):0);
    glUniform1i(glGetUniformLocation(program_,"baked_normal"),maps&&!maps->normal.rgba.empty());
    for(int n=0;n<16;++n){auto name="tex"+std::to_string(n);glUniform1i(glGetUniformLocation(program_,name.c_str()),n);}
    glUniform1f(glGetUniformLocation(program_,"normal_scale"),maps&&maps->source_maps?maps->source_maps->normal_scale:1);
    glUniform1f(glGetUniformLocation(program_,"ao_strength"),maps&&maps->source_maps?maps->source_maps->occlusion_strength:1);
    glUniform1i(glGetUniformLocation(program_, "toon"), toon || material.toon);
    glUniform1i(glGetUniformLocation(program_, "unlit"), material.unlit);
    glUniform1i(glGetUniformLocation(program_, "alpha_mode"), int(material.alpha));
    glUniform1f(glGetUniformLocation(program_, "alpha_cutoff"), material.alpha_cutoff);
    glUniform4fv(glGetUniformLocation(program_, "base_color"), 1,
                 material.color.data());
    glUniform3fv(glGetUniformLocation(program_, "emission"), 1,
                 material.emissive.data());
    glUniform1f(glGetUniformLocation(program_, "metallic"), material.metallic);
    glUniform1f(glGetUniformLocation(program_, "roughness"),
                material.roughness);
    if (material.double_sided)
      glDisable(GL_CULL_FACE);
    else {
      glEnable(GL_CULL_FACE);
      glCullFace(GL_BACK);
    }
    if (blended(material, maps)) {
      glEnable(GL_BLEND);
      glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE,
                          GL_ONE_MINUS_SRC_ALPHA);
      glDepthMask(GL_FALSE);
    } else {
      glDisable(GL_BLEND);
      glDepthMask(GL_TRUE);
    }
    glDrawElements(GL_TRIANGLES, GLsizei(count), GL_UNSIGNED_INT,
                   reinterpret_cast<void *>(first * sizeof(uint32_t)));
  };
  auto toon_state=[&](const SurfaceRange&r,const TextureState::Toon&t,bool outline){
    auto state=t.material.render_state(outline);const auto &source=t.appearance->asset->materials.at(size_t(t.appearance->material));
    auto original=source.alpha==ImportedMaterial::AlphaMode::Opaque?Material::AlphaMode::Opaque:source.alpha==ImportedMaterial::AlphaMode::Mask?Material::AlphaMode::Mask:Material::AlphaMode::Blend;
    if(r.material.alpha!=Material::AlphaMode::Automatic&&r.material.alpha!=original){bool blend=r.material.alpha==Material::AlphaMode::Blend;state.blend=blend;state.depth_write=!blend;state.source_rgb=blend?5:1;state.destination_rgb=blend?10:0;state.source_alpha=t.material.version()==MToonVersion::V0?state.source_rgb:1;state.destination_alpha=state.destination_rgb;state.queue=blend?3000:r.material.alpha==Material::AlphaMode::Mask?2450:2000;}
    if(!outline&&r.material.double_sided!=source.double_sided)state.cull=r.material.double_sided?MToonCull::Off:MToonCull::Back;
    return state;
  };
  auto draw_toon=[&](const SurfaceRange&r,const TextureState::Toon&t,bool outline){
    auto u=t.material.gpu_parameters();auto &source=t.appearance->asset->materials.at(size_t(t.appearance->material));std::array<float,4> effective{};
    for(int k=0;k<4;++k)effective[k]=mtoon_factor(u[0][k],source.color[k],r.material.color[k]);
    for(int k=0;k<3;++k)u[2][k]=mtoon_factor(u[2][k],source.emissive[k],r.material.emissive[k]);
    u[0]=effective;
    if(t.derived){u[0]={1,1,1,1};u[2][0]=u[2][1]=u[2][2]=1;}
    auto original=source.alpha==ImportedMaterial::AlphaMode::Opaque?Material::AlphaMode::Opaque:source.alpha==ImportedMaterial::AlphaMode::Mask?Material::AlphaMode::Mask:Material::AlphaMode::Blend;
    if(r.material.alpha!=Material::AlphaMode::Automatic&&r.material.alpha!=original)u[8][1]=r.material.alpha==Material::AlphaMode::Opaque?0:r.material.alpha==Material::AlphaMode::Mask?1:2;
    if(r.material.alpha_cutoff!=source.alpha_cutoff)u[6][3]=r.material.alpha_cutoff;
    std::array<GLint,16> present{},coordinates{},derived{},channels{};std::array<float,32> offsets{},scales{};std::array<float,16> rotations{};std::array<float,6> pbrx{},pbry{};
    auto &binding=textures_->bindings.at(r.object);
    for(int n=0;n<16;++n){auto &slot=binding[n];glActiveTexture(GL_TEXTURE0+n);glBindTexture(GL_TEXTURE_2D,slot.image);glBindSampler(n,slot.sampler);present[n]=slot.image!=0;
      int role=n==12?0:n==13?3:n;scales[n*2]=scales[n*2+1]=1;
      if(role<11){auto &texture=t.material.textures[role];coordinates[n]=mtoon_coordinate(texture);offsets[n*2]=texture.offset[0];offsets[n*2+1]=texture.offset[1];scales[n*2]=texture.scale[0];scales[n*2+1]=texture.scale[1];rotations[n]=texture.rotation;channels[n]=texture.channel;}
      if(n>=14){int k=n-14;pbrx[k*3]=slot.uv[0];pbrx[k*3+1]=slot.uv[1];pbrx[k*3+2]=slot.uv[4];pbry[k*3]=slot.uv[2];pbry[k*3+1]=slot.uv[3];pbry[k*3+2]=slot.uv[5];}
      auto name="tex"+std::to_string(n);glUniform1i(glGetUniformLocation(program_,name.c_str()),n);
    }
    if(t.derived)derived[0]=derived[3]=1;
    glUniform1i(glGetUniformLocation(program_,"mt_mode"),1);glUniform1i(glGetUniformLocation(program_,"mt_outline"),outline);glUniform1i(glGetUniformLocation(program_,"mt_baked_normal"),t.baked_normal);glUniform4fv(glGetUniformLocation(program_,"mt[0]"),12,u[0].data());
    glUniform1iv(glGetUniformLocation(program_,"mt_present[0]"),16,present.data());glUniform1iv(glGetUniformLocation(program_,"mt_coord[0]"),16,coordinates.data());glUniform1iv(glGetUniformLocation(program_,"mt_derived[0]"),16,derived.data());glUniform1iv(glGetUniformLocation(program_,"mt_channel[0]"),16,channels.data());
    glUniform2fv(glGetUniformLocation(program_,"mt_offset[0]"),16,offsets.data());glUniform2fv(glGetUniformLocation(program_,"mt_scale[0]"),16,scales.data());glUniform1fv(glGetUniformLocation(program_,"mt_rotation[0]"),16,rotations.data());glUniform3fv(glGetUniformLocation(program_,"mt_pbr_x[0]"),2,pbrx.data());glUniform3fv(glGetUniformLocation(program_,"mt_pbr_y[0]"),2,pbry.data());
    glUniform4fv(glGetUniformLocation(program_,"mt_effective_color"),1,effective.data());glUniform4fv(glGetUniformLocation(program_,"mt_native_color"),1,r.material.color.data());glUniform1i(glGetUniformLocation(program_,"mt_shade_alias"),t.shade_alias);
    auto state=toon_state(r,t,outline);
    if(state.cull==MToonCull::Off)glDisable(GL_CULL_FACE);else{glEnable(GL_CULL_FACE);glCullFace(state.cull==MToonCull::Front?GL_FRONT:GL_BACK);}
    if(state.blend){if(state.destination_rgb==9||state.destination_alpha==9)throw std::runtime_error("MToon destination SRC_ALPHA_SATURATE is unsupported");glEnable(GL_BLEND);glBlendFuncSeparate(mtoon_blend(state.source_rgb),mtoon_blend(state.destination_rgb),mtoon_blend(state.source_alpha),mtoon_blend(state.destination_alpha));}else glDisable(GL_BLEND);
    glBlendEquationSeparate(GL_FUNC_ADD,state.maximum_alpha?GL_MAX:GL_FUNC_ADD);glDepthMask(state.depth_write?GL_TRUE:GL_FALSE);glDepthFunc(GL_LEQUAL);
    if(state.alpha_to_coverage)glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);else glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    if(outline&&t.material.version()==MToonVersion::V0){glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(1,1);}else glDisable(GL_POLYGON_OFFSET_FILL);
    glDrawElements(GL_TRIANGLES,GLsizei(r.count),GL_UNSIGNED_INT,reinterpret_cast<void*>(r.first*sizeof(uint32_t)));glDisable(GL_POLYGON_OFFSET_FILL);
  };

  if (cached_->ranges.empty())
    draw_range(Material{}, 0, count_, 0, nullptr);
  else {
    std::vector<const SurfaceRange*> ordered;for(auto &r:cached_->ranges)ordered.push_back(&r);
    auto queue=[&](const SurfaceRange&r){auto toon=textures_->toons.find(r.object);if(toon!=textures_->toons.end())return toon_state(r,toon->second,false).queue;return blended(r.material,r.textures.get())?3000:r.material.alpha==Material::AlphaMode::Mask?2450:2000;};
    std::stable_sort(ordered.begin(),ordered.end(),[&](auto a,auto b){return queue(*a)<queue(*b);});
    for(auto r:ordered){auto typed=textures_->toons.find(r->object);if(typed==textures_->toons.end())draw_range(r->material,r->first,r->count,r->object,r->textures.get());else{draw_toon(*r,typed->second,false);if(typed->second.material.parameters.outline!=MToonOutline::None&&typed->second.material.parameters.outline_width>0)draw_toon(*r,typed->second,true);}}
  }
  if(!textures_->weighted.empty()||(cached_->ranges.empty()&&!cached_->selected_weights.empty())){
    glUniform1i(glGetUniformLocation(program_,"weight_pass"),1);glUniform1i(glGetUniformLocation(program_,"mt_outline"),0);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glDepthMask(GL_TRUE);glDepthFunc(GL_LESS);glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    if(cached_->ranges.empty())glDrawElements(GL_TRIANGLES,GLsizei(count_),GL_UNSIGNED_INT,nullptr);
    for(auto &r:cached_->ranges)if(textures_->weighted.contains(r.object))glDrawElements(GL_TRIANGLES,GLsizei(r.count),GL_UNSIGNED_INT,reinterpret_cast<void*>(r.first*sizeof(uint32_t)));
    glUniform1i(glGetUniformLocation(program_,"weight_pass"),0);
  }
  for(int n=0;n<16;++n)glBindSampler(n,0);
  glActiveTexture(GL_TEXTURE0);
  glDepthMask(GL_TRUE);glDepthFunc(GL_LESS);glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);
  glBindVertexArray(0);
  glUseProgram(0);
  if (glGetError() != GL_NO_ERROR){textures_->clear(true);cached_.reset();count_=0;throw std::runtime_error("Sculpt GPU rendering failed");}
  if(!textures_->deferred)resolve();
  }catch(...){textures_->target.abort();throw;}
}

CursorRenderer::CursorRenderer() {
  const char *v = R"(#version 330 core
layout(location=0)in vec3 position;layout(location=1)in vec3 color;uniform mat4 vp;out vec3 tint;void main(){tint=color;gl_Position=vp*vec4(position,1);})";
  const char *f = R"(#version 330 core
in vec3 tint;out vec4 color;uniform int linear_output;vec3 decode(vec3 c){return mix(c/12.92,pow((c+.055)/1.055,vec3(2.4)),greaterThan(c,vec3(.04045)));}void main(){color=vec4(linear_output==1?decode(tint):tint,1);})";
  auto vs = shader(GL_VERTEX_SHADER, v), fs = shader(GL_FRAGMENT_SHADER, f);
  program_ = glCreateProgram();
  glAttachShader(program_, vs);
  glAttachShader(program_, fs);
  glLinkProgram(program_);
  glDeleteShader(vs);
  glDeleteShader(fs);
  GLint good = 0;
  glGetProgramiv(program_, GL_LINK_STATUS, &good);
  if (!good)
    throw std::runtime_error("Cursor shader failed");
  glGenVertexArrays(1, &vao_);
  glGenBuffers(1, &buffer_);
}
CursorRenderer::~CursorRenderer() {
  glDeleteBuffers(1, &buffer_);
  glDeleteVertexArrays(1, &vao_);
  glDeleteProgram(program_);
}
void CursorRenderer::draw(const EditorState &state, const Mat4 &vp) {
  struct Vertex {
    Vec3 p, color;
  };
  std::vector<Vertex> vertices;
  auto line = [&](ob::Vec3 a, ob::Vec3 b, Vec3 color) {
    vertices.push_back({{a.x, a.y, a.z}, color});
    vertices.push_back({{b.x, b.y, b.z}, color});
  };
  for (auto &h : state.hands)
    if (h.tracked.valid) {
      auto p = h.tracked.grip.position;
      auto axis = ob::rotate(h.tracked.grip.orientation, {0, 0, -.08f});
      line(p, p + axis, {.9f, .75f, .2f});
      if (state.controls.parameter != VrParameter::None && h.tracked.aimValid) {
        auto from = h.tracked.aim.position;
        auto hit = panel_ray(state.panel, h.tracked.aim);
        auto to =
            hit ? hit->point
                : from + ob::rotate(h.tracked.aim.orientation, {0, 0, -.6f});
        Vec3 accent{float((state.accent >> 16) & 255) / 255,
                    float((state.accent >> 8) & 255) / 255,
                    float(state.accent & 255) / 255};
        line(from, to, accent);
        if (hit) {
          auto x = ob::rotate(state.panel.orientation, {.008f, 0, 0}),
               y = ob::rotate(state.panel.orientation, {0, .008f, 0});
          Vec3 contrast = state.light ? Vec3{0, 0, 0} : Vec3{1, 1, 1};
          line(to - x, to + x, contrast);
          line(to - y, to + y, contrast);
          line(to - x * .6f, to + x * .6f, accent);
          line(to - y * .6f, to + y * .6f, accent);
        }
      }
      if (!state.menu) {
        float r =
            state.radius * (h.tracked.trigger > .02f
                                ? .25f + .75f * std::sqrt(h.tracked.trigger)
                                : 1.f);
        Vec3 color =
            state.tool == Tool::Carve ? Vec3{1, .2f, .2f} : Vec3{.1f, .9f, .5f};
        for (int axis = 0; axis < 3; ++axis)
          for (int k = 0; k < 32; ++k) {
            float a = k * 6.283185307f / 32, b = (k + 1) * 6.283185307f / 32;
            auto point = [&](float t) {
              float x = std::cos(t) * r, y = std::sin(t) * r;
              return p + (axis == 0   ? ob::Vec3{x, y, 0}
                          : axis == 1 ? ob::Vec3{x, 0, y}
                                      : ob::Vec3{0, x, y});
            };
            line(point(a), point(b), color);
          }
      }
    }
  if (vertices.empty())
    return;
  if (state.controls.parameter == VrParameter::None)
    glEnable(GL_DEPTH_TEST);
  else
    glDisable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glUseProgram(program_);
  glUniform1i(glGetUniformLocation(program_,"linear_output"),linear_output_);
  glUniformMatrix4fv(glGetUniformLocation(program_, "vp"), 1, GL_FALSE,
                     vp.data());
  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, buffer_);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex),
               vertices.data(), GL_STREAM_DRAW);
  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), nullptr);
  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        reinterpret_cast<void *>(offsetof(Vertex, color)));
  glEnableVertexAttribArray(0);
  glEnableVertexAttribArray(1);
  glDrawArrays(GL_LINES, 0, vertices.size());
  glBindVertexArray(0);
  glUseProgram(0);
  glDepthMask(GL_TRUE);
}
} // namespace dome
