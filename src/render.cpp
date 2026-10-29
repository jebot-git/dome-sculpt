// SPDX-License-Identifier: GPL-3.0-or-later
// Matrix helpers adapted from OpenBrush Native (Apache-2.0).
#include "dome/render.hpp"
#include <cstddef>
#include "dome/source-maps.hpp"
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
struct SurfaceRenderer::TextureState {
  struct Key {const Image *image=nullptr;bool srgb=false;bool operator==(const Key&)const=default;};
  struct KeyHash {size_t operator()(const Key&k)const{return std::hash<const Image*>{}(k.image)^(size_t(k.srgb)<<1);}};
  using Sampler=std::array<int,4>;
  struct SamplerHash {size_t operator()(const Sampler&s)const{size_t h=0;for(int n:s)h=h*131+size_t(n);return h;}};
  struct ImageResource {std::shared_ptr<const Image> owner;GLuint id=0;bool mips=false;size_t bytes=0;};
  struct Slot {GLuint image=0,sampler=0;std::array<float,6> uv{1,0,0,1,0,0};};
  using Binding=std::array<Slot,5>;
  SurfaceTextureLimits limits;
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
  void clear(bool release){if(release){for(auto &[key,r]:images)glDeleteTextures(1,&r.id);for(auto &[key,id]:samplers)glDeleteSamplers(1,&id);}bindings.clear();images.clear();samplers.clear();}
  void prepare(const MeshSnapshot&mesh){
    struct Desired {std::shared_ptr<const Image> owner;bool mips=false;};
    struct PlanSlot {Key key;Sampler sampler;std::array<float,6>uv;bool present=false;};
    std::unordered_map<Key,Desired,KeyHash> wanted;
    std::unordered_set<Sampler,SamplerHash> wanted_samplers;
    std::unordered_map<uint64_t,std::array<PlanSlot,5>> planned;
    for(auto &range:mesh.ranges)if(range.textures){
      auto maps=range.textures;auto &plan=planned[range.object];
      auto slot=[&](int index,std::shared_ptr<const Image> image,const PaintSourceTexture&view,bool srgb){
        if(!image)return;
        view.validate();image->validate();Key key{image.get(),srgb};auto sample=sampler(view);
        auto &desired=wanted[key];desired.owner=std::move(image);desired.mips|=mipmapped(sample);
        wanted_samplers.insert(sample);plan[index]={key,sample,view.uv_transform,true};
      };
      if(maps->source_only){
        if(!maps->source_maps||maps->source_maps->empty()||maps->color.width||maps->metal_rough.width||maps->emission.width)
          throw std::runtime_error("Invalid raw source preview marker");
        maps->source_maps->validate();auto &s=*maps->source_maps;
        slot(0,s.color.image,s.color,true);slot(1,s.metal_rough.image,s.metal_rough,false);slot(2,s.emission.image,s.emission,true);slot(3,s.normal.image,s.normal,false);slot(4,s.occlusion.image,s.occlusion,false);
      }else{
        PaintSourceTexture view;view.wrap_s=view.wrap_t=GL_CLAMP_TO_EDGE;
        slot(0,std::shared_ptr<const Image>(maps,&maps->color),view,true);slot(1,std::shared_ptr<const Image>(maps,&maps->metal_rough),view,false);slot(2,std::shared_ptr<const Image>(maps,&maps->emission),view,true);
        if(maps->source_maps)slot(3,maps->source_maps->normal.image,maps->source_maps->normal,false);
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
    for(auto &[object,plan]:planned){auto &binding=ready[object];for(int n=0;n<5;++n)if(plan[n].present)binding[n]={images.at(plan[n].key).id,samplers.at(plan[n].sampler),plan[n].uv};}
    bindings=std::move(ready);
  }
};
SurfaceRenderer::SurfaceRenderer():SurfaceRenderer(SurfaceTextureLimits{}){}
SurfaceTextureStats SurfaceRenderer::texture_stats()const{return textures_->stats();}
void SurfaceRenderer::abandon(){program_=vao_=vertices_=indices_=0;cached_.reset();textures_->clear(false);count_=0;}

SurfaceRenderer::SurfaceRenderer(SurfaceTextureLimits limits):textures_(std::make_unique<TextureState>(limits)) {
  const char *vertex = R"(#version 330 core
layout(location=0)in vec3 position;layout(location=1)in vec3 normal;layout(location=2)in vec2 uv;
uniform mat4 model,mvp;out vec3 world,n;out vec2 texcoord;
void main(){vec4 p=model*vec4(position,1);world=p.xyz;texcoord=uv;n=normalize(mat3(model)*normal);gl_Position=mvp*vec4(position,1);})";
  const char *fragment = R"(#version 330 core
in vec3 world,n;in vec2 texcoord;out vec4 color;uniform vec3 camera;uniform int toon,unlit,alpha_mode;uniform float alpha_cutoff;uniform vec4 base_color;uniform float metallic,roughness;uniform vec3 emission;uniform int map_mode,slots[5];uniform sampler2D color_map,orm_map,emission_map,normal_map,ao_map;uniform vec3 uv_x[5],uv_y[5];uniform float normal_scale,ao_strength;
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
vec3 N=unit(n);if(slots[3]==1){vec2 uv=source_uv(3),a=dFdx(uv),b=dFdy(uv);vec3 x=dFdx(world),y=dFdy(world);float determinant=a.x*b.y-a.y*b.x;
if(abs(determinant)>1e-8*max(length(a)*length(b),1e-20)){
vec3 T=(x*b.y-y*a.y)/determinant,B=(y*a.x-x*b.x)/determinant;T=unit(T-N*dot(N,T));B=unit(cross(N,T))*(dot(cross(N,T),B)<0.0?-1.0:1.0);
vec3 encoded=texture(normal_map,uv).xyz;vec3 ts=unit(vec3((encoded.xy*2.0-1.0)*normal_scale,encoded.z*2.0-1.0));N=unit(T*ts.x+B*ts.y+N*ts.z);}}
return gl_FrontFacing?N:-N;}
void main(){vec3 shading_normal=unlit==0?surface_normal():vec3(0,0,1);vec4 base=base_color;float metal=metallic,rough=roughness,ao=1.0;vec3 glow=emission;
if(slots[0]==1)base*=texture(color_map,source_uv(0));
if(slots[1]==1){vec3 orm=texture(orm_map,source_uv(1)).rgb;metal*=orm.b;rough*=orm.g;if(map_mode==1)ao=orm.r;}
if(slots[2]==1)glow*=texture(emission_map,source_uv(2)).rgb;
if(map_mode==2&&slots[4]==1)ao=mix(1.0,texture(ao_map,source_uv(4)).r,ao_strength);if(alpha_mode==1)base.a=1.0;else if(alpha_mode==2){if(base.a<alpha_cutoff)discard;base.a=1.0;}
if(unlit==1){color=vec4(output_srgb(base.rgb+glow),base.a);return;}
vec3 N=shading_normal,V=normalize(camera-world),result;
if(toon==1){float lit=dot(N,normalize(vec3(-.4,.8,.7)));float ramp=mix(.32,1.0,smoothstep(-.05,.05,lit));float edge=smoothstep(.05,.18,max(dot(N,V),0.0));result=base.rgb*ramp*edge+vec3(.025);}
else result=shade(N,V,normalize(vec3(-.4,.8,.7)),vec3(3.8),base.rgb,rough,metal)+shade(N,V,normalize(vec3(.7,.2,-.4)),vec3(.65,.8,1.0),base.rgb,rough,metal)+.045*base.rgb*ao;
result+=glow;result=result/(result+vec3(1));color=vec4(pow(result,vec3(1.0/2.2)),base.a);})";
  GLuint vs = shader(GL_VERTEX_SHADER, vertex),
         fs = shader(GL_FRAGMENT_SHADER, fragment);
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
  glViewport(0, 0, width, height);
  glDisable(GL_FRAMEBUFFER_SRGB);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);
  float background = transparent ? 0.f : light ? 208.f / 255 : 48.f / 255;
  glClearColor(background, background, background, transparent ? 0 : 1);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  if (mesh != cached_) {
    struct Vertex {
      Vec3 p, n;
      std::array<float, 2> uv;
    };
    std::vector<Vertex> v;
    v.reserve(mesh->mesh.positions.size());
    for (size_t i = 0; i < mesh->mesh.positions.size(); ++i)
      v.push_back(
          {mesh->mesh.positions[i], mesh->mesh.normals[i],
           mesh->mesh.uv.empty() ? std::array<float, 2>{} : mesh->mesh.uv[i]});
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
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, indices_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 mesh->mesh.indices.size() * sizeof(uint32_t),
                 mesh->mesh.indices.data(), GL_STATIC_DRAW);
    count_ = mesh->mesh.indices.size();
    try{textures_->prepare(*mesh);}catch(...){textures_->clear(true);cached_.reset();count_=0;throw;}
    cached_ = std::move(mesh);
  }
  glUseProgram(program_);
  auto m = model_matrix(model), mvp = multiply(vp, m);
  glUniformMatrix4fv(glGetUniformLocation(program_, "model"), 1, GL_FALSE,
                     m.data());
  glUniformMatrix4fv(glGetUniformLocation(program_, "mvp"), 1, GL_FALSE,
                     mvp.data());
  glUniform3f(glGetUniformLocation(program_, "camera"), camera.x, camera.y,
              camera.z);
  glBindVertexArray(vao_);
  auto blended = [](const Material &m, const TextureMaps *maps) {
    if (m.alpha == Material::AlphaMode::Blend) return true;
    if (m.alpha != Material::AlphaMode::Automatic) return false;
    return m.color[3] < 1 || (maps && maps->transparent);
  };
  auto draw_range = [&](const Material &original, size_t first, size_t count,
                        uint64_t id, const TextureMaps *maps) {
    Material material = original;
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
    const char *names[]={"color_map","orm_map","emission_map","normal_map","ao_map"};for(int n=0;n<5;++n)glUniform1i(glGetUniformLocation(program_,names[n]),n);
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
  if (cached_->ranges.empty())
    draw_range(Material{}, 0, count_, 0, nullptr);
  else {
    for (auto &r : cached_->ranges)
      if (!blended(r.material, r.textures.get()))
        draw_range(r.material, r.first, r.count, r.object, r.textures.get());
    for (auto &r : cached_->ranges)
      if (blended(r.material, r.textures.get()))
        draw_range(r.material, r.first, r.count, r.object, r.textures.get());
  }
  for(int n=0;n<5;++n)glBindSampler(n,0);
  glActiveTexture(GL_TEXTURE0);
  glDepthMask(GL_TRUE);
  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);
  glBindVertexArray(0);
  glUseProgram(0);
  if (glGetError() != GL_NO_ERROR){textures_->clear(true);cached_.reset();count_=0;throw std::runtime_error("Sculpt GPU rendering failed");}
}

CursorRenderer::CursorRenderer() {
  const char *v = R"(#version 330 core
layout(location=0)in vec3 position;layout(location=1)in vec3 color;uniform mat4 vp;out vec3 tint;void main(){tint=color;gl_Position=vp*vec4(position,1);})";
  const char *f = R"(#version 330 core
in vec3 tint;out vec4 color;void main(){color=vec4(tint,1);})";
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
