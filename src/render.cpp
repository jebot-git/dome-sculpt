#include "native-paint-sampling.hpp"
// SPDX-License-Identifier: GPL-3.0-or-later
// Matrix helpers adapted from OpenBrush Native (Apache-2.0).
#include "dome/render.hpp"
#include "dome/source-placement.hpp"
#include "dome/material-visibility.hpp"
#include <cstddef>
#include <limits>
#include "dome/source-maps.hpp"
#include "dome/material-bindings.hpp"
#include "dome/normal-sampling.hpp"
#include "dome/paint-sampling.hpp"
#include "dome/normal-preview.hpp"
#include "dome/color-paint-sampling.hpp"
#include "dome/humanoid-creator.hpp"
#include "render-mtoon.hpp"
#include "normal-preparation-state.hpp"
#include "render-target.hpp"
#include "render-source-pack.hpp"
#include "render-paint-overlay.hpp"
#include "render-buffer-size.hpp"
#include "prepared-color-state.hpp"
#include "source-vrm1-background.hpp"
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
void check_surface_buffer(const char *operation) {
  const GLenum error = glGetError();
  if (error == GL_NO_ERROR) return;
  while (glGetError() != GL_NO_ERROR) {}
  throw std::runtime_error(std::string(operation)+" (GL error "+std::to_string(error)+")");
}
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
GLuint surface_program(const std::string &vertex,const std::string &fragment) {
  GLuint vs=0,fs=0,program=0;
  try {
    vs=shader(GL_VERTEX_SHADER,vertex.c_str());
    fs=shader(GL_FRAGMENT_SHADER,fragment.c_str());
    program=glCreateProgram();
    glAttachShader(program,vs);glAttachShader(program,fs);glLinkProgram(program);
    GLint good=0;glGetProgramiv(program,GL_LINK_STATUS,&good);
    if(!good) {
      char log[4096]{};glGetProgramInfoLog(program,sizeof(log),nullptr,log);
      throw std::runtime_error(std::string("Sculpt shader linking failed: ")+log);
    }
    glDeleteShader(vs);glDeleteShader(fs);return program;
  } catch(...) {
    if(vs)glDeleteShader(vs);if(fs)glDeleteShader(fs);if(program)glDeleteProgram(program);
    throw;
  }
}
std::string pbr_surface_source(std::string source) {
  // Keep the authored sampling and shading code identical. An actual PBR-only
  // snapshot cannot enter the typed MToon branch; make that fact visible to the
  // driver before it specializes every sampler/state combination.
  constexpr std::string_view declaration="uniform int mt_mode,mt_outline,";
  const auto offset=source.find(declaration);
  if(offset==std::string::npos||source.find(declaration,offset+declaration.size())!=std::string::npos)
    throw std::logic_error("Unexpected typed surface shader declaration");
  source.replace(offset,declaration.size(),"const int mt_mode=0;\nuniform int mt_outline,");
  return source;
}
} // namespace
namespace {
bool mtoon_shade_alias(const MToonMaterial&m){auto &a=m.textures[0],&b=m.textures[1];return a.index>=0&&a.index==b.index&&mtoon_coordinate(a)==mtoon_coordinate(b)&&(m.version()==MToonVersion::V0||(a.offset==b.offset&&a.scale==b.scale&&a.rotation==b.rotation));}
bool resolved_shade_alias(const MToonMaterial &m,const std::array<PaintSourceTexture,size_t(MToonSlot::Count)> &roles) {
  const auto &a=roles[0],&b=roles[1];const auto &x=m.textures[0],&y=m.textures[1];
  return a.image && a.image==b.image && a.wrap_s==b.wrap_s && a.wrap_t==b.wrap_t &&
    a.min_filter==b.min_filter && a.mag_filter==b.mag_filter && mtoon_coordinate(x)==mtoon_coordinate(y) &&
    (m.version()==MToonVersion::V0 || (x.offset==y.offset && x.scale==y.scale && x.rotation==y.rotation));
}
GLenum mtoon_blend(int value){constexpr GLenum factors[]={GL_ZERO,GL_ONE,GL_DST_COLOR,GL_SRC_COLOR,GL_ONE_MINUS_DST_COLOR,GL_SRC_ALPHA,GL_ONE_MINUS_SRC_COLOR,GL_DST_ALPHA,GL_ONE_MINUS_DST_ALPHA,GL_SRC_ALPHA_SATURATE,GL_ONE_MINUS_SRC_ALPHA};if(value<0||value>10)throw std::runtime_error("Unsupported MToon blend factor");return factors[value];}
}

struct SurfaceRenderer::TextureState {
  GLuint pbr_program=0,typed_program=0;
  std::string typed_vertex,typed_fragment;
  ~TextureState(){glDeleteProgram(pbr_program);glDeleteProgram(typed_program);}
  GLuint program() {
    // Mixed snapshots use the complete shader throughout the frame. Never
    // infer this from an individual visible range or switch mid-frame.
    if(toons.empty())return pbr_program;
    if(!typed_program)typed_program=surface_program(typed_vertex,typed_fragment);
    return typed_program;
  }
  struct Key {const Image *image=nullptr;bool srgb=false;bool operator==(const Key&)const=default;};
  struct KeyHash {size_t operator()(const Key&k)const{return std::hash<const Image*>{}(k.image)^(size_t(k.srgb)<<1);}};
  using Sampler=std::array<int,4>;
  struct SamplerHash {size_t operator()(const Sampler&s)const{size_t h=0;for(int n:s)h=h*131+size_t(n);return h;}};
  struct ImageResource {std::shared_ptr<const Image> owner;GLuint id=0;bool mips=false;size_t bytes=0;};
  struct Slot {GLuint image=0,sampler=0;std::array<float,6> uv{1,0,0,1,0,0};int coordinate=0;};
  using Binding=std::array<Slot,16>;
  struct Toon {
    std::shared_ptr<const CreatorAppearance> appearance;
    std::shared_ptr<const SourceMaterialBindings> source;
    MToonMaterial material;SourceNativeMaterial native;
    std::array<PaintSourceTexture,size_t(MToonSlot::Count)> roles;
    std::array<PaintSourceTexture,4> packed_views;
    std::array<bool,4> packed_srgb{true,true,true,false};
    MToonSourcePacks::Inputs packed_inputs;
    std::shared_ptr<const detail::PaintOverlayPacket> overlay;
    bool derived=false,shade_alias=false,baked_normal=false,native_emission=false;
  };
  static bool native_float(const Material &material,const TextureMaps *maps,
      const SourceMaterialBindings *binding) noexcept {
    if(!maps||!maps->overlay||!maps->overlay->native_geometry())return false;
    auto hdr=[](const auto &values){return std::any_of(values.begin(),values.end(),[](float value){return value>1;});};
    return detail::overlay_uses_float_emission(*maps->overlay)||hdr(material.emissive)||
      (binding&&(binding->physical_emission||(!binding->native_paint_sampling&&hdr(binding->native_reference.emission))));
  }
  static bool native_float(const SurfaceRange &range) noexcept {
    return native_float(range.material,range.textures.get(),range.material_bindings.get());
  }
  GLint vertex_attributes=0;
  std::unordered_map<uint64_t,Toon> toons;
  MToonSourcePacks packed_sources;
  detail::PaintOverlayCompiler paint_overlays;
  std::unordered_map<uint64_t,std::array<float,3>> physical_backgrounds;
  std::vector<std::array<float,2>> uv1;
  std::vector<std::array<float,2>> original_uv0,paint_coordinates;
  std::array<std::vector<std::array<float,4>>,3> extra_uv;
  std::unordered_map<uint64_t,std::shared_ptr<const PaintSourceMaps>> source_maps;
  std::unordered_set<uint64_t> weighted;
  std::unordered_set<uint64_t> vertex_alpha;
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
  SurfaceTextureStats stats()const{size_t bytes=0;for(auto &[key,r]:images)bytes+=r.bytes;return {images.size()+packed_sources.size()+paint_overlays.image_count(),samplers.size(),bytes+packed_sources.bytes()+paint_overlays.bytes(),uploads,packed_sources.bytes(),packed_sources.builds()};}
  void clear(bool release){paint_overlays.clear(release);physical_backgrounds.clear();packed_sources.clear(release);if(release){for(auto &[key,r]:images)glDeleteTextures(1,&r.id);for(auto &[key,id]:samplers)glDeleteSamplers(1,&id);}bindings.clear();images.clear();samplers.clear();toons.clear();uv1.clear();original_uv0.clear();paint_coordinates.clear();for(auto &values:extra_uv)values.clear();source_maps.clear();weighted.clear();vertex_alpha.clear();}
  void prepare(const MeshSnapshot&mesh,bool geometry_changed){
    if(geometry_changed && (!mesh.mesh.positions.empty() || !mesh.mesh.indices.empty()))mesh.mesh.validate();
    const auto count=mesh.mesh.positions.size();
    if(!mesh.paint_coordinates.empty()&&mesh.paint_coordinates.size()!=count)throw std::invalid_argument("Paint coordinate vertex count differs from geometry");
    if(!mesh.paint_coordinates.empty()&&vertex_attributes<13)throw std::runtime_error("Independent paint sampling requires thirteen vertex attribute slots");
    if(geometry_changed)for(auto uv:mesh.paint_coordinates)for(float v:uv)if(!std::isfinite(v))throw std::invalid_argument("Packed paint coordinate is nonfinite");
    auto normals_valid=[&](const std::vector<Vec3> &values){
      if(!values.empty() && values.size()!=count)throw std::invalid_argument("Retained normal frame size differs from geometry");
      if(geometry_changed)for(auto v:values){float n=length(v);if(!finite(v)||!std::isfinite(n)||n<1e-8f)throw std::invalid_argument("Retained normal frame direction is invalid");}
    };
    normals_valid(mesh.source_normals);normals_valid(mesh.canonical_normals);
    if(!mesh.source_colors.empty() && mesh.source_colors.size()!=count)throw std::invalid_argument("Source vertex-color count differs from geometry");
    if(geometry_changed) {
      for(auto color:mesh.source_colors)for(float c:color)if(!std::isfinite(c) || c<0 || c>1)throw std::invalid_argument("Source vertex colors must be finite linear RGBA");
      vertex_alpha.clear();
      if(mesh.ranges.empty() && std::any_of(mesh.source_colors.begin(),mesh.source_colors.end(),[](const auto &c){return c[3]<1;}))vertex_alpha.insert(0);
      if(!mesh.source_colors.empty())for(const auto &r:mesh.ranges) {
        if(r.first>mesh.mesh.indices.size() || r.count>mesh.mesh.indices.size()-r.first)throw std::invalid_argument("Source vertex-alpha range is invalid");
        for(size_t c=r.first;c<r.first+r.count;++c)if(mesh.source_colors.at(mesh.mesh.indices[c])[3]<1){vertex_alpha.insert(r.object);break;}
      }
    }
    if(!mesh.source_tangents.empty() && mesh.source_tangents.size()!=count)throw std::invalid_argument("Retained tangent frame size differs from geometry");
    if(geometry_changed)for(auto t:mesh.source_tangents){float n=length(Vec3{t[0],t[1],t[2]});if(!finite(Vec3{t[0],t[1],t[2]})||!std::isfinite(n)||n<1e-8f||(t[3]!=-1&&t[3]!=1))throw std::invalid_argument("Retained tangent frame is invalid");}
    for(const auto &range:mesh.ranges) {
      if(range.material_bindings&&range.material_bindings->physical_emission&&
         (range.textures&&!range.textures->source_only&&(!range.textures->overlay||!range.textures->overlay->native_geometry())))
        throw std::invalid_argument("Derived absolute emission requires its physical overlay adapter");
      if(range.material_bindings && range.material_bindings->normal_frames && !range.source_frame)throw std::invalid_argument("Retained material frames require complete worker frame publication");
      if(range.source_frame && (mesh.source_normals.size()!=count || (!range.source_normal_only && mesh.source_tangents.size()!=count)))throw std::invalid_argument("Source frame arrays are unavailable");
      if(range.canonical_frame && (mesh.canonical_normals.size()!=count || mesh.tangents.size()!=count))throw std::invalid_argument("Canonical frame arrays are unavailable");
      if(range.first>mesh.mesh.indices.size() || range.count>mesh.mesh.indices.size()-range.first || range.count%3)throw std::invalid_argument("Frame range is invalid");
      if(geometry_changed && range.source_frame && !range.source_normal_only)for(size_t c=range.first;c<range.first+range.count;c+=3){auto w=mesh.source_tangents.at(mesh.mesh.indices[c])[3];for(size_t k=1;k<3;++k)if(mesh.source_tangents.at(mesh.mesh.indices[c+k])[3]!=w)throw std::invalid_argument("Source tangent W differs within a triangle");}
    }

    if(!mesh.tangents.empty()&&mesh.tangents.size()!=mesh.mesh.positions.size())throw std::invalid_argument("Canonical tangent count differs from packed geometry");
    if(geometry_changed)for(auto t:mesh.tangents)if(!finite(Vec3{t[0],t[1],t[2]})||length(Vec3{t[0],t[1],t[2]})<1e-8f||!std::isfinite(t[3])||std::abs(std::abs(t[3])-1)>1e-5f)throw std::invalid_argument("Invalid canonical tangent frame");
    if(!mesh.selected_weights.empty()&&mesh.selected_weights.size()!=mesh.mesh.positions.size())throw std::runtime_error("Weight visualization size differs from current geometry");
    for(float w:mesh.selected_weights)if(!std::isfinite(w)||(w!=-1&&(w<0||w>1)))throw std::runtime_error("Invalid weight visualization value");
    if(!mesh.sculpt_masks.empty()&&mesh.sculpt_masks.size()!=mesh.mesh.positions.size())throw std::invalid_argument("Sculpt mask count differs from packed geometry");
    for(float value:mesh.sculpt_masks)if(!std::isfinite(value)||(value!=-1&&(value<0||value>1)))throw std::invalid_argument("Invalid sculpt mask visualization value");
    std::unordered_set<uint64_t> next_weighted;
    if(!mesh.selected_weights.empty())for(auto &r:mesh.ranges){
      if(r.first>mesh.mesh.indices.size()||r.count>mesh.mesh.indices.size()-r.first)throw std::runtime_error("Weight visualization range is invalid");
      for(size_t i=r.first;i<r.first+r.count;++i){auto index=mesh.mesh.indices[i];if(index>=mesh.selected_weights.size())throw std::runtime_error("Weight visualization vertex is invalid");if(mesh.selected_weights[index]>=0)next_weighted.insert(r.object);}
    }
    struct Desired {std::shared_ptr<const Image> owner;bool mips=false;};
    struct PlanSlot {Key key;Sampler sampler;std::array<float,6>uv;bool present=false;int coordinate=0;};
    std::unordered_map<Key,Desired,KeyHash> wanted;
    std::unordered_set<Sampler,SamplerHash> wanted_samplers;
    std::unordered_map<uint64_t,std::array<PlanSlot,16>> planned;
    std::unordered_map<uint64_t,Toon> next_toons;std::vector<std::array<float,2>> next_uv1;
    std::vector<std::array<float,2>> next_original_uv0,next_paint_coordinates;
    std::array<std::vector<std::array<float,4>>,3> next_extra_uv;
    std::unordered_map<uint64_t,std::shared_ptr<const PaintSourceMaps>> next_source_maps;
    std::unordered_map<uint64_t,std::array<float,3>> next_physical_backgrounds;
    std::vector<detail::PaintOverlayCompiler::Input> overlay_inputs;
    for(auto &range:mesh.ranges){
      auto maps=range.textures;auto &plan=planned[range.object];
      if(range.material_bindings&&range.material_bindings->physical_emission)
        next_physical_backgrounds.emplace(range.object,*range.material_bindings->physical_emission);
      if(maps&&maps->overlay&&maps->overlay->native_geometry()){
        const auto &packet=*maps->overlay;const auto &owner=packet.source_maps();
        if(maps->source_only||owner!=maps->source_maps||owner.owner_before(maps->source_maps)||maps->source_maps.owner_before(owner)||
           packet.native_domain()!=(range.paint_sampling_attachment?range.paint_sampling_attachment->domain():nullptr))
          throw std::invalid_argument("Native emission packet lost its actual independent paint owners");
        const auto &basis=*packet.native_geometry();
        if(basis.indices.size()!=range.count)throw std::invalid_argument("Native emission packet corner layout differs from the displayed geometry");
        if(range.paint_sampling_attachment&&!range.paint_sampling_attachment->matches(packet.native_geometry(),packet.native_domain()))
          throw std::invalid_argument("Native emission packet borrowed another current geometry attachment");
        if(geometry_changed)for(size_t c=0;c<range.count;++c){
          const auto logical=range.paint_winding_reversed&&c%3?c+(c%3==1?1:-1):c;
          const auto expected=packet.native_domain()?packet.native_domain()->basis()->uv.at(packet.native_domain()->basis()->indices.at(packet.native_domain()->source_corners()[logical])):basis.uv.at(basis.indices.at(logical));
          const auto vertex=mesh.mesh.indices.at(range.first+c);
          const auto actual=mesh.paint_coordinates.empty()?mesh.mesh.uv.at(vertex):mesh.paint_coordinates.at(vertex);
          if(actual!=expected)throw std::invalid_argument("Native emission stream lost exact ordered paint coordinates");
        }
        if(!packet.empty()&&native_float(range))overlay_inputs.push_back({maps->overlay,true});
      }
      if(maps&&maps->overlay&&!maps->overlay->native_geometry()){
        const auto &owner=maps->overlay->source_maps();
        if(!maps->source_only||!owner||owner!=maps->source_maps||owner.owner_before(maps->source_maps)||maps->source_maps.owner_before(owner))
          throw std::invalid_argument("GPU overlay lost its exact original sampling owner");
        overlay_inputs.push_back({maps->overlay,true});
        const auto &logical=maps->overlay->original_uv0();
        if(!logical)throw std::invalid_argument("Source paint packet omitted its authenticated original UV0");
        if(vertex_attributes<13)throw std::runtime_error("Independent source painting requires thirteen vertex attribute slots");
        if(geometry_changed){
          if(range.first_vertex>count||logical->size()>count-range.first_vertex)throw std::invalid_argument("Original UV0 domain exceeds packed geometry");
          for(size_t c=range.first;c<range.first+range.count;++c){const auto v=mesh.mesh.indices.at(c);if(v<range.first_vertex||v-range.first_vertex>=logical->size())throw std::invalid_argument("Original UV0 index is outside its sealed packed domain");}
          if(next_original_uv0.empty())next_original_uv0=mesh.mesh.uv;
          if(next_paint_coordinates.empty())next_paint_coordinates=mesh.paint_coordinates.empty()?mesh.mesh.uv:mesh.paint_coordinates;
          std::copy(logical->begin(),logical->end(),next_original_uv0.begin()+range.first_vertex);
          // Source4's current mesh UV0 is the actual writable grid, even when
          // another range supplies an independently prepared paint stream.
          std::copy_n(mesh.mesh.uv.begin()+range.first_vertex,logical->size(),next_paint_coordinates.begin()+range.first_vertex);
        }
      }
      if(maps&&maps->source_maps){
        const auto &owner=maps->source_maps;auto before=source_maps.find(range.object);
        if(geometry_changed||before==source_maps.end()||before->second!=owner)owner->validate();
        // The trusted geometry token owns the REST coordinate dependencies.
        // Worker-expanded arrays may have fresh allocation owners while their
        // authenticated REST inputs and packed layout remain unchanged.
        next_source_maps.emplace(range.object,owner);
        if(geometry_changed)for(const auto &[set,values]:owner->extra_uv_sets){
          if(range.first_vertex>count||values->size()>count-range.first_vertex)throw std::invalid_argument("Extra source UV domain exceeds packed geometry");
          for(size_t c=range.first;c<range.first+range.count;++c)if(mesh.mesh.indices.at(c)<range.first_vertex||mesh.mesh.indices.at(c)-range.first_vertex>=values->size())throw std::invalid_argument("Extra source UV index is outside its packed owner");
          auto &packed=next_extra_uv[(set-2)/2];if(packed.empty())packed.resize(count);
          const size_t component=((set-2)%2)*2;for(size_t v=0;v<values->size();++v){packed[range.first_vertex+v][component]=values->at(v)[0];packed[range.first_vertex+v][component+1]=values->at(v)[1];}
        }
      }

      const auto paint_node=detail::material_paint_attachment(range.material_bindings.get());
      if(bool(paint_node)!=bool(range.paint_sampling_attachment))throw std::invalid_argument("Paint GPU range omitted or fabricated its own sampling attachment");
      if(range.paint_sampling_attachment) {
        if(!maps||maps->source_only||!range.material_bindings||range.material_bindings->has_mtoon)
          throw std::invalid_argument("Paint sampling requires its own derived untyped material");
        const auto expected=detail::material_paint_attachment(range.material_bindings.get());
        if(!expected||expected!=range.paint_sampling_attachment||expected.owner_before(range.paint_sampling_attachment)||range.paint_sampling_attachment.owner_before(expected))
          throw std::invalid_argument("Paint GPU range differs from its own immutable material attachment");
        const auto &domain=*range.paint_sampling_attachment->domain();
        if(mesh.paint_coordinates.size()!=count||range.count!=domain.source_corners().size())
          throw std::invalid_argument("Paint sampling GPU stream/range dimensions differ");
        if(geometry_changed)for(size_t c=0;c<range.count;++c) {
          const auto logical=range.paint_winding_reversed&&c%3?c+(c%3==1?1:-1):c;
          const auto source=domain.source_corners()[logical];
          if(mesh.paint_coordinates.at(mesh.mesh.indices.at(range.first+c))!=domain.basis()->uv.at(domain.basis()->indices.at(source)))
            throw std::invalid_argument("Paint GPU stream lost exact ordered corner correspondence");
        }
      }
      if(const auto coordinates=detail::retained_normal_coordinates(range.material_bindings.get(),maps.get())){
        const auto &domain=*coordinates;
        if(mesh.normal_coordinates.size()!=count||range.count!=domain.indices.size())
          throw std::invalid_argument("Retained normal stream/range dimensions differ from its own field");
        if(geometry_changed){
          const bool reflected=range.paint_winding_reversed;
          for(size_t c=0;c<range.count;++c){
            const auto logical=reflected&&c%3?c+(c%3==1?1:-1):c;
            const auto source=domain.indices.at(logical);const auto uv=domain.coordinates.at(source);
            const auto toon=domain.mtoon_coordinates.empty()?uv:domain.mtoon_coordinates.at(source);
            if(mesh.normal_coordinates.at(mesh.mesh.indices.at(range.first+c))!=std::array<float,4>{uv[0],uv[1],toon[0],toon[1]})
              throw std::invalid_argument("Retained normal GPU stream lost exact ordered corner correspondence");
          }
        }
      }
      const auto source_uv1=maps&&maps->source_maps?maps->source_maps->uv1:nullptr;
      if(source_uv1&&geometry_changed){
        if(range.first_vertex>count||source_uv1->size()>count-range.first_vertex)throw std::invalid_argument("Source UV1 domain exceeds packed geometry");
        for(size_t c=range.first;c<range.first+range.count;++c)if(mesh.mesh.indices.at(c)<range.first_vertex||mesh.mesh.indices.at(c)-range.first_vertex>=source_uv1->size())throw std::invalid_argument("Source UV1 index is outside its packed domain");
        if(next_uv1.empty())next_uv1.resize(count);std::copy(source_uv1->begin(),source_uv1->end(),next_uv1.begin()+range.first_vertex);
      }
      if(maps&&maps->overlay&&maps->overlay->original_uv1()&&geometry_changed){
        const auto &logical=*maps->overlay->original_uv1();
        if(range.first_vertex>count||logical.size()>count-range.first_vertex)throw std::invalid_argument("Original logical UV1 exceeds packed geometry");
        if(next_uv1.empty())next_uv1.resize(count);std::copy(logical.begin(),logical.end(),next_uv1.begin()+range.first_vertex);
      }
      if(maps&&maps->source_only) {
        auto present=[](const Image &image){return image.width||image.height||!image.rgba.empty();};
        if(!maps->source_maps || maps->baked_base ||
           present(maps->color)||present(maps->metal_rough)||present(maps->emission)||present(maps->normal)||
           present(maps->color_coverage)||present(maps->emission_coverage)||present(maps->baked_normal)||present(maps->baked_coverage)||present(maps->mtoon_normal)||maps->normal_projection)
          throw std::invalid_argument("Raw source preview marker contains derived material payload");
      }
      auto slot=[&](int index,std::shared_ptr<const Image> image,const PaintSourceTexture&view,bool srgb){
        if(!image)return;
        view.validate(maps&&maps->source_maps?maps->source_maps->coordinate_count():source_uv1?2:1);image->validate();Key key{image.get(),srgb};auto sample=sampler(view);
        auto &desired=wanted[key];desired.owner=std::move(image);desired.mips|=mipmapped(sample);
        wanted_samplers.insert(sample);plan[index]={key,sample,view.uv_transform,true,view.texcoord};
      };
      if(range.normal_preview_proof&&!range.normal_preview_proof->matches(range.material_bindings,maps))throw std::invalid_argument("Normal GPU preview lost its exact independently authenticated owners");
      const bool sealed=range.paint_sampling_attachment&&range.normal_preview_proof&&range.normal_preview_proof->matches(range.material_bindings,maps)&&
        range.normal_preview_proof->metadata_domain()==range.paint_sampling_attachment->domain()&&!range.normal_preview_proof->metadata_domain().owner_before(range.paint_sampling_attachment->domain())&&!range.paint_sampling_attachment->domain().owner_before(range.normal_preview_proof->metadata_domain());
      if(range.normal_preview_proof&&range.paint_sampling_attachment&&!sealed)throw std::invalid_argument("Normal and paint preview metadata use different owning coordinate domains");
      const bool deep_material=geometry_changed&&!sealed;
      const bool prepared_color=detail::prepared_color_state(range.material_bindings.get(),maps.get(),deep_material);
      const bool prepared_normal=detail::prepared_normal_state(range.material_bindings.get(),maps.get(),deep_material);
      std::optional<MToonMaterial> typed;
      if(range.material_bindings) {
        range.material_bindings->validate(deep_material);
        if(range.material_bindings->has_mtoon&&!range.pbr_view)typed=range.material_bindings->material;
      } else if(auto previous=toons.find(range.object);!range.pbr_view&&previous!=toons.end() &&
                previous->second.appearance==range.appearance && !previous->second.source)typed=previous->second.material;
      else if(!range.pbr_view)typed=retained_mtoon(range.appearance.get());
      if(range.source_normal_only && ((typed && typed->parameters.normal_enabled && typed->parameters.normal_scale!=0 && typed->textures[size_t(MToonSlot::Normal)].index>=0) ||
          (!typed && maps && maps->source_maps && maps->source_maps->normal.image && maps->source_maps->normal_scale!=0)))
        throw std::invalid_argument("Normal-mapped source material requires retained tangent authority");
      if(typed){
        Toon toon;toon.appearance=range.appearance;toon.source=range.material_bindings;toon.material=*typed;
        // This packet was authenticated against the actual native paint/domain
        // above. Its color is already in maps; it carries emission only here.
        toon.native_emission=native_float(range);
        toon.derived=maps&&!maps->source_only;
        if(maps&&maps->overlay&&!maps->overlay->empty()&&(!maps->overlay->native_geometry()||toon.native_emission))toon.overlay=maps->overlay;
        if(toon.source) {toon.roles=toon.source->roles;toon.native=toon.source->native_reference;}
        else {
          toon.native=native_source(*range.appearance);const auto &asset=*range.appearance->asset;
          for(size_t role=0;role<size_t(MToonSlot::Count);++role) {
            const auto &t=typed->textures[role];if(t.index<0)continue;
            const auto &texture=asset.textures.at(size_t(t.index));
            if(texture.image<0 || size_t(texture.image)>=range.appearance->decoded_images->size())
              throw std::runtime_error("MToon image reference is missing");
            auto &view=toon.roles[role];view.image=range.appearance->decoded_images->at(texture.image);
            view.wrap_s=texture.wrap_s;view.wrap_t=texture.wrap_t;
            view.min_filter=texture.min_filter;view.mag_filter=texture.mag_filter;
          }
        }
        toon.shade_alias=toon.source?resolved_shade_alias(*typed,toon.roles):mtoon_shade_alias(*typed);
        bool needs_uv1=false;
        auto original=[&](int destination,int role) {
          const auto &t=typed->textures[role];if(t.index<0)return;
          if(role!=int(MToonSlot::Matcap))needs_uv1|=mtoon_coordinate(t)==1;
          const auto &view=toon.roles[role];slot(destination,view.image,view,t.encoding==MToonEncoding::Srgb);
        };
        for(int role=0;role<11;++role)if(!prepared_normal||role!=int(MToonSlot::Normal))original(role,role);
        if(toon.overlay){
          // Original images keep their native texture samplers, including
          // implementation-defined precision at animated wrap boundaries.
          // Only the four actual linear paint fields share the buffer unit.
          for(auto &v:toon.packed_views){v.wrap_s=v.wrap_t=GL_CLAMP_TO_EDGE;v.min_filter=v.mag_filter=GL_LINEAR;}
        }
        if(prepared_normal){PaintSourceTexture view;view.wrap_s=view.wrap_t=GL_CLAMP_TO_EDGE;view.min_filter=view.mag_filter=GL_LINEAR;
          auto image=std::shared_ptr<const Image>(maps,&maps->mtoon_normal);
          if(maps->normal_projection&&maps->normal_projection->version==2){const auto &p=*maps->normal_projection;
            image=maps->baked_base&&!maps->baked_base->maps.normal.rgba.empty()?p.sampling_domain->resolved_mtoon:p.mtoon;}
          slot(2,std::move(image),view,false);
        }
        if(toon.derived){
          maps->color_coverage.validate();maps->emission_coverage.validate();
          PaintSourceMaps empty;const auto &source_maps=maps->source_maps?*maps->source_maps:empty;
          original(12,0);
          auto derived=[&](int destination,const Image&image,const PaintSourceTexture&original,bool srgb){auto view=original;view.uv_transform={1,0,0,1,0,0};if(!view.image)view.wrap_s=view.wrap_t=GL_CLAMP_TO_EDGE;slot(destination,std::shared_ptr<const Image>(maps,&image),view,srgb);};
          derived(0,maps->color,source_maps.color,true);
          if(toon.native_emission)original(3,3);
          else derived(3,maps->emission,source_maps.emission,true);
          derived(11,maps->color_coverage,source_maps.color,false);
          slot(14,source_maps.color.image,source_maps.color,true);
          toon.packed_views={source_maps.color,toon.roles[3],source_maps.emission,source_maps.emission};
          toon.packed_srgb[1]=typed->textures[3].encoding==MToonEncoding::Srgb;
          if(toon.native_emission){
            // Emission uses the real original typed role, never a subtraction
            // from a finite generic emission map. Only color correction needs view0.
            for(size_t view=1;view<4;++view)toon.packed_views[view]={};
          }else{
            auto &coverage=toon.packed_views[3];coverage.image=std::shared_ptr<const Image>(maps,&maps->emission_coverage);
            coverage.texcoord=0;coverage.uv_transform={1,0,0,1,0,0};
            if(!source_maps.emission.image)coverage.wrap_s=coverage.wrap_t=GL_CLAMP_TO_EDGE;
          }
          for(size_t packed=0;packed<4;++packed) {
            const auto &view=toon.packed_views[packed];if(!view.image)continue;view.validate();view.image->validate();
            Key key{view.image.get(),toon.packed_srgb[packed]};auto sample=sampler(view);
            auto &desired=wanted[key];desired.owner=view.image;desired.mips|=mipmapped(sample);wanted_samplers.insert(sample);
          }
          if(!prepared_normal&&!maps->normal.rgba.empty()){
            if(!maps->baked_base||maps->baked_normal.rgba.empty()||maps->baked_coverage.rgba.empty()||mesh.tangents.size()!=mesh.mesh.positions.size())throw std::invalid_argument("Typed baked normals require original canonical data, coverage and worker tangent corners");
            PaintSourceTexture view;view.wrap_s=view.wrap_t=GL_CLAMP_TO_EDGE;
            slot(13,std::shared_ptr<const Image>(maps,&maps->baked_normal),view,false);slot(15,std::shared_ptr<const Image>(maps,&maps->baked_coverage),view,false);toon.baked_normal=true;
          }
        }
        if(needs_uv1&&maps&&maps->overlay){
          const auto &logical=maps->overlay->original_uv1();
          if(!logical)
            throw std::invalid_argument("MToon UV1 lacks its actual original authored coordinate owner");
        }else if(needs_uv1&&geometry_changed){
          if(!range.appearance || !range.appearance->asset)throw std::runtime_error("Resolved MToon UV1 requires retained primitive provenance");
          const auto &appearance=*range.appearance;const auto &asset=*appearance.asset;
          const ImportedPrimitive *primitive=nullptr;for(const auto &p:asset.primitives)if(p.source_mesh==appearance.source_mesh&&p.source_primitive==appearance.source_primitive){if(primitive)throw std::runtime_error("MToon source primitive identity is ambiguous");primitive=&p;}
          if(!primitive||primitive->uv1.size()!=primitive->mesh.positions.size()||appearance.source_vertices.empty()||range.first_vertex>mesh.mesh.positions.size()||appearance.source_vertices.size()>mesh.mesh.positions.size()-range.first_vertex)throw std::runtime_error("MToon selected UV1 data/remap is unavailable");
          if(next_uv1.empty())next_uv1.resize(mesh.mesh.positions.size());
          for(size_t n=0;n<appearance.source_vertices.size();++n){auto source=appearance.source_vertices[n];if(source>=primitive->uv1.size())throw std::runtime_error("MToon UV1 authored vertex reference is invalid");if(source_uv1&&source_uv1->at(n)!=primitive->uv1[source])throw std::invalid_argument("PBR and MToon packed UV1 authority differ");next_uv1[range.first_vertex+n]=primitive->uv1[source];}
          if(range.first>mesh.mesh.indices.size()||range.count>mesh.mesh.indices.size()-range.first)throw std::runtime_error("MToon packed index range is invalid");
          for(size_t n=range.first;n<range.first+range.count;++n)if(mesh.mesh.indices[n]<range.first_vertex||mesh.mesh.indices[n]>=range.first_vertex+appearance.source_vertices.size())throw std::runtime_error("MToon packed source vertex interval is invalid");
        }
        next_toons.emplace(range.object,std::move(toon));continue;
      }
      if(!maps)continue;
      if(maps->source_only){
        if(!maps->source_maps||maps->color.width||maps->metal_rough.width||maps->emission.width||maps->normal.width)
          throw std::runtime_error("Invalid raw source preview marker");
        maps->source_maps->validate();auto &s=*maps->source_maps;
        slot(0,s.color.image,s.color,true);slot(1,s.metal_rough.image,s.metal_rough,false);slot(2,s.emission.image,s.emission,true);slot(3,s.normal.image,s.normal,false);slot(4,s.occlusion.image,s.occlusion,false);
      }else{
        PaintSourceTexture view;view.wrap_s=view.wrap_t=GL_CLAMP_TO_EDGE;
        auto color=view,emission=view;if(maps->source_maps){if(maps->source_maps->color.image)color=maps->source_maps->color;if(maps->source_maps->emission.image)emission=maps->source_maps->emission;}
        color.uv_transform=emission.uv_transform={1,0,0,1,0,0};
        if(prepared_color){color=view;color.min_filter=color.mag_filter=GL_LINEAR;}
        slot(0,std::shared_ptr<const Image>(maps,prepared_color?&maps->pbr_color:&maps->color),color,true);slot(1,std::shared_ptr<const Image>(maps,&maps->metal_rough),view,false);
        if(native_float(range)){
          if(maps->source_maps&&maps->source_maps->emission.image&&!(range.material_bindings&&range.material_bindings->physical_emission)){
            const auto &source=maps->source_maps->emission;
            if(source.texcoord!=0)throw std::invalid_argument("Native float emission requires its actual alternate source-coordinate adapter");
            slot(2,source.image,source,true);
          }
        }else slot(2,std::shared_ptr<const Image>(maps,&maps->emission),emission,true);
        if(!maps->normal.rgba.empty()){
          if(!prepared_normal&&mesh.tangents.size()!=mesh.mesh.positions.size())throw std::invalid_argument("Normal-mapped snapshots require worker-generated tangent corners");
          auto image=std::shared_ptr<const Image>(maps,&maps->normal);
          if(prepared_normal&&maps->normal_projection&&maps->normal_projection->version==2){const auto &p=*maps->normal_projection;
            image=maps->baked_base&&!maps->baked_base->maps.normal.rgba.empty()?p.sampling_domain->resolved_pbr:p.pbr;
            view.min_filter=view.mag_filter=GL_LINEAR;}
          slot(3,std::move(image),view,false);
        }else if(maps->source_maps)slot(3,maps->source_maps->normal.image,maps->source_maps->normal,false);
      }
      if(!typed && range.source_frame && range.canonical_frame && maps && maps->baked_base && !maps->baked_normal.rgba.empty()) {
        PaintSourceTexture view;view.wrap_s=view.wrap_t=33071;view.min_filter=view.mag_filter=GL_LINEAR;
        slot(3,std::shared_ptr<const Image>(maps,&maps->baked_normal),view,false);
        slot(6,std::shared_ptr<const Image>(maps,&maps->baked_coverage),view,false);
        if(maps->source_maps)slot(5,maps->source_maps->normal.image,maps->source_maps->normal,false);
      }
    }
    if(wanted.size()>limits.max_images||wanted_samplers.size()>limits.max_samplers)throw std::length_error("GPU source texture count budget exceeded");
    size_t bytes=0;GLint maximum=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maximum);
    for(auto &[key,desired]:wanted){
      if(auto previous=images.find(key);previous!=images.end())desired.mips|=previous->second.mips;
      auto &i=*desired.owner;if(i.width>uint32_t(maximum)||i.height>uint32_t(maximum))throw std::length_error("Original image exceeds hardware texture dimensions");
      size_t count=storage(i,desired.mips);if(count>limits.max_bytes-bytes)throw std::length_error("GPU original texture byte budget exceeded");bytes+=count;
    }
    std::vector<MToonSourcePacks::Inputs> source_packs;
    for(auto &[object,toon]:next_toons)if(toon.derived||toon.overlay) {
      for(size_t view=0;view<4;++view)if(const auto &image=toon.packed_views[view].image) {
        Key key{image.get(),toon.packed_srgb[view]};
        toon.packed_inputs[view]={image,0,wanted.at(key).mips,key.srgb};
      }
      if(toon.overlay){
        detail::PaintOverlayCompiler::Resource forecast;forecast.owner=toon.overlay->contribution_owner()?toon.overlay->contribution_owner():toon.overlay;forecast.pbr=true;
        if(toon.native_emission){
          toon.packed_inputs[4]=MToonSourcePacks::Input::linear(forecast,2);
          toon.packed_inputs[5]=MToonSourcePacks::Input::linear(forecast,4);
          toon.packed_inputs[6]=MToonSourcePacks::Input::linear(forecast,3);
        }else{
          toon.packed_inputs[0]=MToonSourcePacks::Input::linear(forecast,0);
          toon.packed_inputs[1]=MToonSourcePacks::Input::linear(forecast,2);
          toon.packed_inputs[2]=MToonSourcePacks::Input::linear(forecast,4);
          toon.packed_inputs[3]=MToonSourcePacks::Input::linear(forecast,3);
        }
      }
      source_packs.push_back(toon.packed_inputs);
    }
    std::map<const detail::PaintOverlayPacket*,size_t> overlay_sizes;size_t overlay_bytes=0,overlay_images=0;
    for(const auto &input:overlay_inputs){
      const auto *identity=input.owner->contribution_owner()?input.owner->contribution_owner().get():input.owner.get();
      if(overlay_sizes.contains(identity))continue;
      const size_t size=detail::PaintOverlayCompiler::texture_bytes(input.owner);
      overlay_images+=detail::PaintOverlayCompiler::texture_images(input.owner);
      if(size>limits.max_bytes-bytes||overlay_bytes>limits.max_bytes-bytes-size)throw std::length_error("GPU overlay byte budget exceeded");
      overlay_sizes.emplace(identity,size);overlay_bytes+=size;
    }
    if(overlay_images>limits.max_images-wanted.size())throw std::length_error("GPU overlay image count exceeded");
    packed_sources.preflight(source_packs,bytes+overlay_bytes,wanted.size()+overlay_images,limits);
    for(auto it=images.begin();it!=images.end();)if(!wanted.contains(it->first)){glDeleteTextures(1,&it->second.id);it=images.erase(it);}else ++it;
    for(auto it=samplers.begin();it!=samplers.end();)if(!wanted_samplers.contains(it->first)){glDeleteSamplers(1,&it->second);it=samplers.erase(it);}else ++it;
    if(packed_sources.bytes()>limits.max_bytes-bytes||packed_sources.size()>limits.max_images-wanted.size())
      throw std::length_error("GPU source pack leaves no overlay budget");
    paint_overlays.prepare(overlay_inputs,limits.max_bytes-bytes-packed_sources.bytes(),limits.max_images-wanted.size()-packed_sources.size());
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
    source_packs.clear();
    for(auto &[object,toon]:next_toons)if(toon.derived||toon.overlay) {
      for(auto &view:toon.packed_inputs)if(view.owner)view.texture=images.at(Key{view.owner.get(),view.srgb}).id;
      if(toon.overlay){const auto &resource=paint_overlays.at(toon.overlay,true);
        if(toon.native_emission){
          toon.packed_inputs[4]=MToonSourcePacks::Input::linear(resource,2);
          toon.packed_inputs[5]=MToonSourcePacks::Input::linear(resource,4);
          toon.packed_inputs[6]=MToonSourcePacks::Input::linear(resource,3);
        }else{
          toon.packed_inputs[0]=MToonSourcePacks::Input::linear(resource,0);
          toon.packed_inputs[1]=MToonSourcePacks::Input::linear(resource,2);
          toon.packed_inputs[2]=MToonSourcePacks::Input::linear(resource,4);
          toon.packed_inputs[3]=MToonSourcePacks::Input::linear(resource,3);
        }
      }
      source_packs.push_back(toon.packed_inputs);
    }
    packed_sources.prepare(source_packs,bytes+overlay_bytes,wanted.size()+overlay_images,limits);
    std::unordered_map<uint64_t,Binding> ready;
    for(auto &[object,plan]:planned){auto &binding=ready[object];for(int n=0;n<16;++n)if(plan[n].present)binding[n]={images.at(plan[n].key).id,samplers.at(plan[n].sampler),plan[n].uv,plan[n].coordinate};}
    bindings=std::move(ready);toons=std::move(next_toons);physical_backgrounds=std::move(next_physical_backgrounds);if(geometry_changed){uv1=std::move(next_uv1);extra_uv=std::move(next_extra_uv);original_uv0=std::move(next_original_uv0);paint_coordinates=next_paint_coordinates.empty()?mesh.paint_coordinates:std::move(next_paint_coordinates);}source_maps=std::move(next_source_maps);weighted=std::move(next_weighted);
  }
};
SurfaceRenderer::SurfaceRenderer():SurfaceRenderer(SurfaceTextureLimits{}){}
SurfaceTextureStats SurfaceRenderer::texture_stats()const{return textures_->stats();}
void SurfaceRenderer::set_output_encoding(SurfaceOutputEncoding encoding){if(textures_->target.pending())throw std::logic_error("Resolve current view before changing output encoding");textures_->output=encoding;}
void SurfaceRenderer::set_deferred_resolve(bool value){if(textures_->target.pending())throw std::logic_error("Resolve current view before changing deferred output");textures_->deferred=value;}
void SurfaceRenderer::resolve(){textures_->target.resolve(textures_->output);}
void SurfaceRenderer::set_frame_time(double seconds){if(!std::isfinite(seconds)||seconds<0||seconds>std::numeric_limits<float>::max())throw std::invalid_argument("Invalid GPU animation frame time");textures_->frame_time=seconds;}
void SurfaceRenderer::abandon(){program_=vao_=vertices_=indices_=weights_=masks_=0;textures_->pbr_program=textures_->typed_program=0;buffers_.vertex_bytes=buffers_.index_bytes=buffers_.weight_bytes=buffers_.mask_bytes=0;cached_.reset();textures_->clear(false);textures_->target.clear(false);count_=0;}

SurfaceRenderer::SurfaceRenderer(SurfaceTextureLimits limits):textures_(std::make_unique<TextureState>(limits)) {
  GLint attributes=0;glGetIntegerv(GL_MAX_VERTEX_ATTRIBS,&attributes);
  if(attributes<16)throw std::runtime_error("Source UV rendering requires sixteen vertex attribute slots");
  GLint varying=0;glGetIntegerv(GL_MAX_VARYING_COMPONENTS,&varying);if(varying<56)throw std::runtime_error("Source UV rendering exceeds available varying components");
  textures_->vertex_attributes=attributes;
  auto vertex=mtoon_renderer_vertex(true);
  std::string fragment="#version 330 core\n"+mtoon_renderer_common()+R"GLSL(
in vec3 world,n,canonical_t,canonical_b,cut_position,canonical_n,source_n,source_t,source_b;in vec4 source_color,normal_coordinates,source_uv23,source_uv45,source_uv67;in float source_w;in vec2 texcoord,texcoord1,paintcoord;in float weight_value,mask_value;uniform int weight_pass;uniform float ghost_opacity;uniform vec3 selection_color;uniform float selection_amount;uniform int clip_enabled;uniform vec4 clip_plane;out vec4 color;uniform vec3 camera;uniform int toon,unlit,alpha_mode;uniform float alpha_cutoff;uniform vec4 base_color;uniform float metallic,roughness;uniform vec3 emission;uniform int source_uv1_is_uv0;uniform int overlay_mode;uniform float relative_emission_strength;uniform int map_mode,slots[5],source_coordinates[5],baked_normal;uniform vec3 uv_x[5],uv_y[5];uniform float normal_scale,ao_strength;uniform int source_baked_frame,original_normal_present;uniform vec3 original_normal_x,original_normal_y;uniform int original_normal_coordinate;
)GLSL"+mtoon_renderer_fragment()+R"GLSL(
const float PI=3.141592653589793;
vec3 shade(vec3 N,vec3 V,vec3 L,vec3 power,vec3 base,float roughness,float metallic){
vec3 H=normalize(V+L);float NV=max(dot(N,V),0.0001),NL=max(dot(N,L),0.0),NH=max(dot(N,H),0.0),VH=max(dot(V,H),0.0);
float rough=max(.04,roughness),a=rough*rough,a2=a*a;
float d=a2/(PI*pow(NH*NH*(a2-1.0)+1.0,2.0));float k=pow(rough+1.0,2.0)/8.0;
float g=NV/(NV*(1.0-k)+k)*NL/(NL*(1.0-k)+k);vec3 F0=mix(vec3(.04),base,metallic);vec3 F=F0+(1.0-F0)*pow(1.0-VH,5.0);
return ((1.0-F)*(1.0-metallic)*base/PI+d*g*F/max(4.0*NV*NL,.0001))*power*NL;}
vec3 output_srgb(vec3 c){return mix(1.055*pow(max(c,vec3(0)),vec3(1.0/2.4))-.055,12.92*c,lessThanEqual(c,vec3(.0031308)));}
vec2 source_coordinate(int set){if(set==1)return source_uv1_is_uv0==1?texcoord:texcoord1;if(set==2)return source_uv23.xy;if(set==3)return source_uv23.zw;if(set==4)return source_uv45.xy;if(set==5)return source_uv45.zw;if(set==6)return source_uv67.xy;if(set==7)return source_uv67.zw;return texcoord;}
vec2 source_uv(int slot){if(slot==3&&retained_normal_domain==1)return normal_coordinates.xy;vec2 q=map_mode==1?paintcoord:source_coordinate(source_coordinates[slot]);return vec2(dot(uv_x[slot],vec3(q,1)),dot(uv_y[slot],vec3(q,1)));}
vec3 unit(vec3 v){float m=max(max(abs(v.x),abs(v.y)),abs(v.z));return m>1e-20?normalize(v/m):vec3(0,0,1);}
vec3 source_baked_detail(out float coverage){
ivec2 size=textureSize(tex3,0);vec2 p=paintcoord*vec2(size)-.5;ivec2 base=ivec2(floor(p));vec2 f=fract(p);vec3 sum=vec3(0);coverage=0;
for(int y=0;y<2;++y)for(int x=0;x<2;++x){ivec2 q=clamp(base+ivec2(x,y),ivec2(0),size-1);float w=(x==1?f.x:1-f.x)*(y==1?f.y:1-f.y)*texelFetch(tex6,q,0).r;coverage+=w;sum+=(texelFetch(tex3,q,0).rgb*2-1)*w;}
return coverage>1e-8?sum/coverage:vec3(0,0,1);}
vec3 surface_normal(){
vec3 N=unit(source_frame==1?source_n:n);
if(source_baked_frame==1){vec3 original=N;
if(original_normal_present==1){vec2 input_uv=source_coordinate(original_normal_coordinate);vec2 q=vec2(dot(original_normal_x,vec3(input_uv,1)),dot(original_normal_y,vec3(input_uv,1)));vec3 encoded=texture(tex5,q).rgb;
vec3 ts=unit(vec3((encoded.xy*2-1)*normal_scale,encoded.z*2-1));original=unit(unit(source_t)*ts.x+unit(source_b)*ts.y+N*ts.z);}
float coverage;vec3 detail=source_baked_detail(coverage);vec3 canonical=unit(canonical_t*detail.x+canonical_b*detail.y+canonical_n*detail.z);
N=unit(mix(original,canonical,coverage));return gl_FrontFacing?N:-N;}
if(slots[3]==1&&baked_normal==1){vec3 detail=texture(tex3,paintcoord).xyz*2.0-1.0;
vec3 T=canonical_t,B=canonical_b,CN=canonical_n;
N=unit(T*detail.x+B*detail.y+CN*detail.z);}
else if(slots[3]==1&&source_frame==1){vec3 T=unit(source_t),B=unit(source_b);
vec3 encoded=texture(tex3,source_uv(3)).xyz;vec3 ts=unit(vec3((encoded.xy*2.0-1.0)*normal_scale,encoded.z*2.0-1.0));N=unit(T*ts.x+B*ts.y+N*ts.z);}
else if(slots[3]==1){vec2 uv=source_uv(3),a=dFdx(uv),b=dFdy(uv);vec3 x=dFdx(world),y=dFdy(world);float determinant=a.x*b.y-a.y*b.x;
if(abs(determinant)>1e-8*max(length(a)*length(b),1e-20)){
vec3 T=(x*b.y-y*a.y)/determinant,B=(y*a.x-x*b.x)/determinant;T=unit(T-N*dot(N,T));B=unit(cross(N,T))*(dot(cross(N,T),B)<0.0?-1.0:1.0);
vec3 encoded=texture(tex3,uv).xyz;vec3 ts=unit(vec3((encoded.xy*2.0-1.0)*normal_scale,encoded.z*2.0-1.0));N=unit(T*ts.x+B*ts.y+N*ts.z);}}
return gl_FrontFacing?N:-N;}
// Masking darkens final linear RGB, retaining the material's lighting, alpha,
// cutout, depth and blend rules. Zero and the ordinary-object sentinel are exact.
vec3 sculpt_mask_color(vec3 rgb){return mask_value>0?rgb*(1.0-.72*clamp(mask_value,0.0,1.0)):rgb;}
vec4 ghost_color(vec4 c){c.rgb=mix(c.rgb,selection_color,selection_amount);return ghost_opacity<1.0?vec4(mix(c.rgb,vec3(.35,.5,.7),.5),c.a*ghost_opacity):c;}
void main(){if(clip_enabled==1&&dot(clip_plane.xyz,cut_position)<clip_plane.w)discard;if(weight_pass==1){if(weight_value<0)discard;float opacity;
if(mt_mode==1){bool killed;vec4 source=mt_surface(n,unit(camera-world),killed);if(killed)discard;opacity=source.a;}
else{opacity=base_color.a;if(slots[0]==1)opacity*=texture(tex0,source_uv(0)).a;
if(overlay_mode==1){vec4 contribution=texture(tex7,paintcoord),coverage=texture(tex10,paintcoord);opacity=opacity*(1-coverage.r)+contribution.a;}
opacity*=source_color.a;
if(alpha_mode==1)opacity=1;else if(alpha_mode==2){if(opacity<alpha_cutoff)discard;opacity=1;}}
if(opacity<=0)discard;float x=clamp(weight_value,0,1)*4;vec3 ramp=x<1?mix(vec3(0,0,1),vec3(0,1,1),x):x<2?mix(vec3(0,1,1),vec3(0,1,0),x-1):x<3?mix(vec3(0,1,0),vec3(1,1,0),x-2):mix(vec3(1,1,0),vec3(1,0,0),x-3);color=ghost_color(vec4(ramp,opacity));return;}if(weight_value>=0&&ghost_opacity>=1.0)discard;if(mt_mode==1){bool killed;vec4 result=mt_surface(n,unit(camera-world),killed);if(killed)discard;color=ghost_color(vec4(sculpt_mask_color(result.rgb),result.a));return;}vec3 shading_normal=unlit==0?surface_normal():vec3(0,0,1);vec4 base=base_color;float metal=metallic,rough=roughness,ao=1.0;vec3 glow=emission;
if(slots[0]==1)base*=texture(tex0,source_uv(0));
if(slots[1]==1){vec3 orm=texture(tex1,source_uv(1)).rgb;metal*=orm.b;rough*=orm.g;if(map_mode==1)ao=orm.r;}
if(slots[2]==1)glow*=texture(tex2,source_uv(2)).rgb;
if(map_mode==2&&slots[4]==1)ao=mix(1.0,texture(tex4,source_uv(4)).r,ao_strength);
if(overlay_mode==1){vec4 contribution=texture(tex7,paintcoord),orm=texture(tex8,paintcoord),coverage=texture(tex10,paintcoord);
 base=base*(1-coverage.r)+contribution;metal=metal*(1-coverage.g)+orm.b;rough=rough*(1-coverage.b)+orm.g;ao=ao*(1-orm.a)+orm.r;
 glow=glow*(1-coverage.a)+texture(tex9,paintcoord).rgb*relative_emission_strength+texture(tex13,paintcoord).rgb;}
if(overlay_mode==2){float coverage=texture(tex10,paintcoord).a;glow=glow*(1-coverage)+texture(tex9,paintcoord).rgb+texture(tex13,paintcoord).rgb;}
base*=source_color;
if(alpha_mode==1)base.a=1.0;else if(alpha_mode==2){if(base.a<alpha_cutoff)discard;base.a=1.0;}
if(unlit==1){color=ghost_color(vec4(sculpt_mask_color(base.rgb),base.a));return;}
vec3 N=shading_normal,V=normalize(camera-world),result;
if(toon==1){float lit=dot(N,normalize(vec3(-.4,.8,.7)));float ramp=mix(.32*ao,1.0,smoothstep(-.05,.05,lit));float edge=smoothstep(.05,.18,max(dot(N,V),0.0));result=base.rgb*ramp*edge+vec3(.025);}
else result=shade(N,V,normalize(vec3(-.4,.8,.7)),vec3(3.8),base.rgb,rough,metal)+shade(N,V,normalize(vec3(.7,.2,-.4)),vec3(.65,.8,1.0),base.rgb,rough,metal)+.045*base.rgb*ao;
result+=glow;result=result/(result+vec3(1));color=ghost_color(vec4(sculpt_mask_color(result),base.a));})GLSL";
  textures_->typed_vertex=std::move(vertex);textures_->typed_fragment=std::move(fragment);
  textures_->pbr_program=surface_program(pbr_surface_source(textures_->typed_vertex),pbr_surface_source(textures_->typed_fragment));
  program_=textures_->pbr_program;
  glGenVertexArrays(1, &vao_);
  glGenBuffers(1, &vertices_);
  glGenBuffers(1, &indices_);
  glGenBuffers(1, &weights_);
  glGenBuffers(1, &masks_);
}
SurfaceRenderer::~SurfaceRenderer() {
  textures_->target.clear(true);
  textures_->clear(true);
  glDeleteBuffers(1, &indices_);
  glDeleteBuffers(1, &weights_);
  glDeleteBuffers(1, &masks_);
  glDeleteBuffers(1, &vertices_);
  glDeleteVertexArrays(1, &vao_);
}
void SurfaceRenderer::draw(std::shared_ptr<const MeshSnapshot> mesh,
                           CanvasTransform model, const Mat4 &vp,
                           ob::Vec3 camera, int width, int height,
                           bool transparent, bool toon, bool light, std::span<const ObjectInfo> display, uint64_t replaced_object) {
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
    const auto &identity=mesh->geometry_identity;
    bool reuse=identity && cached_ && cached_->geometry_identity &&
      identity==cached_->geometry_identity &&
      !identity.owner_before(cached_->geometry_identity) &&
      !cached_->geometry_identity.owner_before(identity) &&
      mesh->generation==cached_->generation;
    const auto count=mesh->mesh.positions.size();
    if(mesh->mesh.normals.size()!=count || (!mesh->mesh.uv.empty()&&mesh->mesh.uv.size()!=count))
      throw std::invalid_argument("Packed geometry attribute count mismatch");
    if(mesh->mesh.indices.size()>size_t(std::numeric_limits<GLsizei>::max()))
      throw std::length_error("Packed geometry exceeds GPU draw count limit");
    if(reuse && (count!=cached_->mesh.positions.size() ||
                 mesh->mesh.indices.size()!=cached_->mesh.indices.size() ||
                 mesh->tangents.size()!=cached_->tangents.size() ||
                 mesh->mesh.uv.size()!=cached_->mesh.uv.size() ||
                 mesh->source_normals.size()!=cached_->source_normals.size() ||
                 mesh->canonical_normals.size()!=cached_->canonical_normals.size() ||
                 mesh->source_tangents.size()!=cached_->source_tangents.size() ||
                 mesh->source_colors.size()!=cached_->source_colors.size() ||
                 mesh->normal_coordinates.size()!=cached_->normal_coordinates.size() ||
                 mesh->paint_coordinates.size()!=cached_->paint_coordinates.size()))
      throw std::invalid_argument("Shared geometry identity has inconsistent packed counts");
    try{textures_->prepare(*mesh,!reuse);}catch(...){textures_->clear(true);cached_.reset();count_=0;throw;}
    glBindVertexArray(vao_);
    if(!reuse) {
      struct Vertex {
        Vec3 p,n;
        std::array<float,2> uv,uv1;
        std::array<float,4> tangent;
      };
      struct FrameVertex {Vec3 source_n,canonical_n;std::array<float,4> source_t;};
      const bool has_frames=!mesh->source_normals.empty() || !mesh->canonical_normals.empty() || !mesh->source_tangents.empty();
      const size_t vertex_bytes=render_detail::buffer_bytes(count,sizeof(Vertex));
      const size_t frame_bytes=has_frames?render_detail::buffer_bytes(count,sizeof(FrameVertex)):0;
      const size_t color_bytes=render_detail::buffer_bytes(mesh->source_colors.size(),sizeof(std::array<float,4>));
      const size_t color_offset=render_detail::buffer_sum(vertex_bytes,frame_bytes);
      const size_t normal_offset=render_detail::buffer_sum(color_offset,color_bytes);
      if(!mesh->normal_coordinates.empty()&&mesh->normal_coordinates.size()!=count)throw std::invalid_argument("Retained normal coordinate vertex count differs");
      const size_t normal_bytes=render_detail::buffer_bytes(mesh->normal_coordinates.size(),sizeof(std::array<float,4>));
      const size_t paint_offset=render_detail::buffer_sum(normal_offset,normal_bytes);
      const size_t paint_bytes=render_detail::buffer_bytes(textures_->paint_coordinates.size(),sizeof(std::array<float,2>));
      std::array<size_t,3> extra_offsets{},extra_bytes{};
      size_t geometry_bytes=render_detail::buffer_sum(paint_offset,paint_bytes);
      for(size_t block=0;block<3;++block){const auto &values=textures_->extra_uv[block];if(!values.empty()&&values.size()!=count)throw std::invalid_argument("Extra source UV packed count differs");extra_offsets[block]=geometry_bytes;extra_bytes[block]=render_detail::buffer_bytes(values.size(),sizeof(std::array<float,4>));geometry_bytes=render_detail::buffer_sum(geometry_bytes,extra_bytes[block]);}
      const size_t index_bytes=render_detail::buffer_bytes(mesh->mesh.indices.size(),sizeof(uint32_t));
      std::vector<Vertex> vertices;
      vertices.reserve(count);
      for(size_t i=0;i<count;++i)
        vertices.push_back({mesh->mesh.positions[i],mesh->mesh.normals[i],
          textures_->original_uv0.empty()?(mesh->mesh.uv.empty()?std::array<float,2>{}:mesh->mesh.uv[i]):textures_->original_uv0[i],
          textures_->uv1.empty()?std::array<float,2>{}:textures_->uv1[i],
          mesh->tangents.empty()?std::array<float,4>{1,0,0,1}:mesh->tangents[i]});
      glBindBuffer(GL_ARRAY_BUFFER,vertices_);
      if(has_frames || color_bytes || normal_bytes || paint_bytes || std::any_of(extra_bytes.begin(),extra_bytes.end(),[](auto n){return n!=0;})) {
        // Optional direction block shares the static geometry lifetime without
        // increasing ordinary sculpt vertices or concatenating host buffers.
        std::vector<FrameVertex> frames;if(has_frames)frames.reserve(count);
        if(has_frames)for(size_t i=0;i<count;++i)frames.push_back({
          mesh->source_normals.empty()?mesh->mesh.normals[i]:mesh->source_normals[i],
          mesh->canonical_normals.empty()?mesh->mesh.normals[i]:mesh->canonical_normals[i],
          mesh->source_tangents.empty()?std::array<float,4>{1,0,0,1}:mesh->source_tangents[i]});
        glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(geometry_bytes),nullptr,GL_STATIC_DRAW);
        check_surface_buffer("Surface geometry buffer allocation failed");
        if(vertex_bytes)glBufferSubData(GL_ARRAY_BUFFER,0,GLsizeiptr(vertex_bytes),vertices.data());
        if(frame_bytes)glBufferSubData(GL_ARRAY_BUFFER,GLintptr(vertex_bytes),GLsizeiptr(frame_bytes),frames.data());
        if(color_bytes)glBufferSubData(GL_ARRAY_BUFFER,GLintptr(color_offset),GLsizeiptr(color_bytes),mesh->source_colors.data());
        if(normal_bytes)glBufferSubData(GL_ARRAY_BUFFER,GLintptr(normal_offset),GLsizeiptr(normal_bytes),mesh->normal_coordinates.data());
        for(size_t block=0;block<3;++block)if(extra_bytes[block])glBufferSubData(GL_ARRAY_BUFFER,GLintptr(extra_offsets[block]),GLsizeiptr(extra_bytes[block]),textures_->extra_uv[block].data());
        if(paint_bytes)glBufferSubData(GL_ARRAY_BUFFER,GLintptr(paint_offset),GLsizeiptr(paint_bytes),textures_->paint_coordinates.data());
      } else glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(vertex_bytes),vertices.data(),GL_STATIC_DRAW);
      check_surface_buffer("Surface vertex buffer upload failed");
      glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),nullptr);glEnableVertexAttribArray(0);
      glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,n)));glEnableVertexAttribArray(1);
      glVertexAttribPointer(2,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,uv)));glEnableVertexAttribArray(2);
      glVertexAttribPointer(3,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,uv1)));glEnableVertexAttribArray(3);
      glVertexAttribPointer(5,4,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,tangent)));glEnableVertexAttribArray(5);
      if(has_frames) {
        glVertexAttribPointer(7,3,GL_FLOAT,GL_FALSE,sizeof(FrameVertex),reinterpret_cast<void*>(vertex_bytes+offsetof(FrameVertex,source_n)));glEnableVertexAttribArray(7);
        glVertexAttribPointer(8,4,GL_FLOAT,GL_FALSE,sizeof(FrameVertex),reinterpret_cast<void*>(vertex_bytes+offsetof(FrameVertex,source_t)));glEnableVertexAttribArray(8);
        glVertexAttribPointer(9,3,GL_FLOAT,GL_FALSE,sizeof(FrameVertex),reinterpret_cast<void*>(vertex_bytes+offsetof(FrameVertex,canonical_n)));glEnableVertexAttribArray(9);
      } else for(GLuint attribute:{7,8,9})glDisableVertexAttribArray(attribute);
      if(color_bytes){glVertexAttribPointer(10,4,GL_FLOAT,GL_FALSE,sizeof(std::array<float,4>),reinterpret_cast<void*>(color_offset));glEnableVertexAttribArray(10);}
      else glDisableVertexAttribArray(10);
      if(normal_bytes){glVertexAttribPointer(11,4,GL_FLOAT,GL_FALSE,sizeof(std::array<float,4>),reinterpret_cast<void*>(normal_offset));glEnableVertexAttribArray(11);}
      else glDisableVertexAttribArray(11);
      for(size_t block=0;block<3;++block){if(extra_bytes[block]){glVertexAttribPointer(GLuint(13+block),4,GL_FLOAT,GL_FALSE,sizeof(std::array<float,4>),reinterpret_cast<void*>(extra_offsets[block]));glEnableVertexAttribArray(GLuint(13+block));}else glDisableVertexAttribArray(GLuint(13+block));}
      if(paint_bytes){glVertexAttribPointer(12,2,GL_FLOAT,GL_FALSE,sizeof(std::array<float,2>),reinterpret_cast<void*>(paint_offset));glEnableVertexAttribArray(12);}
      else if(textures_->vertex_attributes>=13)glDisableVertexAttribArray(12);
      glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,indices_);
      glBufferData(GL_ELEMENT_ARRAY_BUFFER,GLsizeiptr(index_bytes),mesh->mesh.indices.data(),GL_STATIC_DRAW);
      check_surface_buffer("Surface index buffer upload failed");
      buffers_.vertex_bytes=geometry_bytes;
      buffers_.index_bytes=index_bytes;
      ++buffers_.geometry_uploads;
    }
    auto scalar=[&](GLuint buffer,GLuint attribute,const std::vector<float>&values,size_t&bytes,uint64_t&uploads) {
      if(values.empty()) {
        glDisableVertexAttribArray(attribute);
        if(bytes){glBindBuffer(GL_ARRAY_BUFFER,buffer);glBufferData(GL_ARRAY_BUFFER,0,nullptr,GL_DYNAMIC_DRAW);check_surface_buffer("Surface scalar buffer release failed");}
        bytes=0;return;
      }
      glBindBuffer(GL_ARRAY_BUFFER,buffer);
      const size_t scalar_bytes=render_detail::buffer_bytes(values.size(),sizeof(float));
      glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(scalar_bytes),values.data(),GL_DYNAMIC_DRAW);
      check_surface_buffer("Surface scalar buffer upload failed");
      glVertexAttribPointer(attribute,1,GL_FLOAT,GL_FALSE,sizeof(float),nullptr);glEnableVertexAttribArray(attribute);
      bytes=scalar_bytes;++uploads;
    };
    scalar(weights_,4,mesh->selected_weights,buffers_.weight_bytes,buffers_.weight_uploads);
    scalar(masks_,6,mesh->sculpt_masks,buffers_.mask_bytes,buffers_.mask_uploads);
    count_=mesh->mesh.indices.size();
    cached_=std::move(mesh);
  }
  // Disabled array constants are context state, rather than VAO state. Set
  // them for every draw, including stereo reuse and interleaved renderers.
  if(cached_->selected_weights.empty())glVertexAttrib1f(4,-1);
  if(cached_->sculpt_masks.empty())glVertexAttrib1f(6,-1);
  if(cached_->source_colors.empty())glVertexAttrib4f(10,1,1,1,1);
  if(cached_->normal_coordinates.empty())glVertexAttrib4f(11,0,0,0,0);
  if(cached_->source_normals.empty() && cached_->canonical_normals.empty() && cached_->source_tangents.empty()) {
    glVertexAttrib3f(7,0,0,1);glVertexAttrib4f(8,1,0,0,1);glVertexAttrib3f(9,0,0,1);
  }
  program_=textures_->program();
  glUseProgram(program_);
  glUniform1i(glGetUniformLocation(program_,"affine_motion"),0);
  glUniform1f(glGetUniformLocation(program_,"model_hand"),1);
  glFrontFace(GL_CCW);
  glUniform1i(glGetUniformLocation(program_,"packedSources"),14);
  glUniform1i(glGetUniformLocation(program_,"weight_pass"),0);
  glUniform1f(glGetUniformLocation(program_,"ghost_opacity"),1);
  glUniform1f(glGetUniformLocation(program_,"selection_amount"),0);
  glUniform1i(glGetUniformLocation(program_,"clip_enabled"),0);
  auto plane=cached_->clip_plane;
  glUniform4f(glGetUniformLocation(program_,"clip_plane"),plane.normal.x,plane.normal.y,plane.normal.z,plane.offset);
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
  auto blended = [&](const Material &m, const TextureMaps *maps,uint64_t id,std::optional<float> alpha={}) {
    if (m.alpha == Material::AlphaMode::Blend) return true;
    if (m.alpha != Material::AlphaMode::Automatic) return false;
    return alpha.value_or(m.color[3]) < 1 || (maps && (maps->color_projection?maps->pbr_transparent:maps->transparent)) || textures_->vertex_alpha.contains(id);
  };
  bool ghost_pass=false;
  auto draw_range = [&](const Material &original, size_t first, size_t count,
                        uint64_t id, const TextureMaps *maps,bool use_source=false,bool use_canonical=false,bool force_pbr=false,bool paint_domain=false,bool normal_domain=false,const SourceMaterialBindings *binding=nullptr,const detail::Vrm1SourceBackground *background=nullptr) {
    const bool native_float=TextureState::native_float(original,maps,binding);
    Material material = original;if(background){material.color=background->color;material.emissive=background->emission;}
    glUniform1f(glGetUniformLocation(program_,"relative_emission_strength"),background?background->relative_emission_strength:1);
    glDepthFunc(GL_LESS);glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    glUniform1i(glGetUniformLocation(program_,"source_frame"),use_source);glUniform1i(glGetUniformLocation(program_,"canonical_frame"),use_canonical);
    glUniform1i(glGetUniformLocation(program_,"prepared_pbr_frame"),maps&&(maps->normal_projection||(maps->baked_base&&maps->baked_base->normal_field)));
    glUniform1i(glGetUniformLocation(program_,"paint_domain"),paint_domain||(maps&&maps->overlay&&maps->overlay->original_uv0()));
    glUniform1i(glGetUniformLocation(program_,"retained_normal_domain"),normal_domain||(maps&&maps->normal_projection&&maps->normal_projection->version==2));
    const auto overlay=maps?maps->overlay:nullptr;
    const auto uv0=overlay?overlay->original_uv0():nullptr;
    const auto carrier=maps&&maps->source_maps?maps->source_maps->uv1:nullptr;
    glUniform1i(glGetUniformLocation(program_,"source_uv1_is_uv0"),overlay&&overlay->original_uv1()&&uv0&&uv0==carrier&&!uv0.owner_before(carrier)&&!carrier.owner_before(uv0));
    glUniform1i(glGetUniformLocation(program_,"mt_mode"),0);glUniform1i(glGetUniformLocation(program_,"mt_outline"),0);
    if (maps&&!maps->source_only) {
      material.color={1,1,1,1};material.metallic=material.roughness=1;
      material.emissive=native_float?original.emissive:std::array<float,3>{1,1,1};
    }
    std::array<GLint,5> present{},coordinates{};std::array<float,15> ux{},uy{};
    for(int n=0;n<5;++n){
      TextureState::Slot slot;if(maps)slot=textures_->bindings.at(id)[n];
      glActiveTexture(GL_TEXTURE0+n);glBindTexture(GL_TEXTURE_2D,slot.image);glBindSampler(n,slot.sampler);present[n]=slot.image!=0;coordinates[n]=slot.coordinate;
      ux[n*3]=slot.uv[0];ux[n*3+1]=slot.uv[1];ux[n*3+2]=slot.uv[4];uy[n*3]=slot.uv[2];uy[n*3+1]=slot.uv[3];uy[n*3+2]=slot.uv[5];
    }
    glUniform1iv(glGetUniformLocation(program_,"slots[0]"),5,present.data());
    glUniform1iv(glGetUniformLocation(program_,"source_coordinates[0]"),5,coordinates.data());
    glUniform3fv(glGetUniformLocation(program_,"uv_x[0]"),5,ux.data());glUniform3fv(glGetUniformLocation(program_,"uv_y[0]"),5,uy.data());
    const bool source_bake=use_source && use_canonical && maps && maps->baked_base && !maps->baked_normal.rgba.empty();
    glUniform1i(glGetUniformLocation(program_,"source_baked_frame"),source_bake);
    glUniform1i(glGetUniformLocation(program_,"original_normal_present"),0);
    if(source_bake){const auto &binding=textures_->bindings.at(id);
      for(int n:{5,6}){glActiveTexture(GL_TEXTURE0+n);glBindTexture(GL_TEXTURE_2D,binding[n].image);glBindSampler(n,binding[n].sampler);}
      const auto &view=binding[5];glUniform1i(glGetUniformLocation(program_,"original_normal_coordinate"),view.coordinate);glUniform1i(glGetUniformLocation(program_,"original_normal_present"),view.image!=0);
      glUniform3f(glGetUniformLocation(program_,"original_normal_x"),view.uv[0],view.uv[1],view.uv[4]);glUniform3f(glGetUniformLocation(program_,"original_normal_y"),view.uv[2],view.uv[3],view.uv[5]);}
    const bool visible_overlay=maps&&maps->overlay&&!maps->overlay->empty()&&(!maps->overlay->native_geometry()||native_float);
    glUniform1i(glGetUniformLocation(program_,"overlay_mode"),visible_overlay?(maps->overlay->native_geometry()?2:1):0);
    if(visible_overlay){const auto &overlay=textures_->paint_overlays.at(maps->overlay,true);
      constexpr std::array<int,5> slots{7,8,9,10,13};
      for(size_t n=0;n<slots.size();++n){glActiveTexture(GL_TEXTURE0+slots[n]);glBindTexture(GL_TEXTURE_2D,overlay.planes[n]);glBindSampler(slots[n],0);}}
    glUniform1i(glGetUniformLocation(program_,"map_mode"),maps?(maps->source_only?2:1):0);
    glUniform1i(glGetUniformLocation(program_,"baked_normal"),maps&&!maps->normal_projection&&!(maps->baked_base&&maps->baked_base->normal_field)&&!maps->normal.rgba.empty());
    for(int n=0;n<16;++n){auto name="tex"+std::to_string(n);glUniform1i(glGetUniformLocation(program_,name.c_str()),n);}
    glUniform1f(glGetUniformLocation(program_,"normal_scale"),maps&&maps->source_maps&&!maps->normal_projection&&!(maps->baked_base&&maps->baked_base->normal_field)?maps->source_maps->normal_scale:1);
    glUniform1f(glGetUniformLocation(program_,"ao_strength"),maps&&maps->source_maps?maps->source_maps->occlusion_strength:1);
    glUniform1i(glGetUniformLocation(program_, "toon"), !force_pbr&&(toon || material.toon));
    glUniform1i(glGetUniformLocation(program_, "unlit"), material.unlit);
    glUniform1i(glGetUniformLocation(program_, "alpha_mode"), int(material.alpha));
    glUniform1f(glGetUniformLocation(program_, "alpha_cutoff"), material.alpha_cutoff);
    glUniform4fv(glGetUniformLocation(program_, "base_color"), 1,
                 material.color.data());
    const auto physical=textures_->physical_backgrounds.find(id);
    glUniform3fv(glGetUniformLocation(program_, "emission"),1,
      physical==textures_->physical_backgrounds.end()?material.emissive.data():physical->second.data());
    glUniform1f(glGetUniformLocation(program_, "metallic"), material.metallic);
    glUniform1f(glGetUniformLocation(program_, "roughness"),
                material.roughness);
    if (material.double_sided)
      glDisable(GL_CULL_FACE);
    else {
      glEnable(GL_CULL_FACE);
      glCullFace(GL_BACK);
    }
    if (blended(material, maps,id)) {
      glEnable(GL_BLEND);
      glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE,
                          GL_ONE_MINUS_SRC_ALPHA);
      glDepthMask(GL_FALSE);
    } else {
      glDisable(GL_BLEND);
      glDepthMask(GL_TRUE);
    }
    if(ghost_pass){glEnable(GL_BLEND);glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);glDepthMask(GL_FALSE);glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);}
    glDrawElements(GL_TRIANGLES, GLsizei(count), GL_UNSIGNED_INT,
                   reinterpret_cast<void *>(first * sizeof(uint32_t)));
  };
  auto toon_state=[&](const SurfaceRange&r,const TextureState::Toon&t,bool outline){
    auto state=t.material.render_state(outline);const auto &source=t.native;
    auto original=Material::AlphaMode(source.alpha);
    if(r.material.alpha!=Material::AlphaMode::Automatic&&r.material.alpha!=original){bool blend=r.material.alpha==Material::AlphaMode::Blend;state.blend=blend;state.depth_write=!blend;state.source_rgb=blend?5:1;state.destination_rgb=blend?10:0;state.source_alpha=t.material.version()==MToonVersion::V0?state.source_rgb:1;state.destination_alpha=state.destination_rgb;state.queue=blend?3000:r.material.alpha==Material::AlphaMode::Mask?2450:2000;}
    if(!outline&&r.material.double_sided!=source.double_sided)state.cull=r.material.double_sided?MToonCull::Off:MToonCull::Back;
    return state;
  };
  auto draw_toon=[&](const SurfaceRange&r,const TextureState::Toon&t,bool outline){
    auto u=t.material.gpu_parameters();const auto &source=t.native;std::array<float,4> effective{};std::array<float,3> effective_emission{};
    for(int k=0;k<4;++k)effective[k]=mtoon_factor(u[0][k],source.color[k],r.material.color[k]);
    const auto &emission=t.material.parameters.emission;
    std::array<float,3> authored_emission{emission.x,emission.y,emission.z};
    float emission_strength=t.material.parameters.emission_strength;
    for(int k=0;k<3;++k) {
      effective_emission[k]=mtoon_factor(authored_emission[k],source.emission[k],r.material.emissive[k]);
      u[2][k]=effective_emission[k]*emission_strength;
    }
    u[0]=effective;
    if(t.derived){u[0]={1,1,1,1};u[2][0]=u[2][1]=u[2][2]=emission_strength;}
    const auto background=textures_->physical_backgrounds.find(r.object);
    if(t.overlay)u[0]={1,1,1,1};
    if(t.overlay||t.native_emission||background!=textures_->physical_backgrounds.end())u[2][0]=u[2][1]=u[2][2]=1;
    auto original=Material::AlphaMode(source.alpha);
    if(r.material.alpha!=Material::AlphaMode::Automatic&&r.material.alpha!=original)u[8][1]=r.material.alpha==Material::AlphaMode::Opaque?0:r.material.alpha==Material::AlphaMode::Mask?1:2;
    if(r.material.alpha_cutoff!=source.alpha_cutoff)u[6][3]=r.material.alpha_cutoff;
    std::array<GLint,16> present{},coordinates{},derived{},channels{};std::array<float,32> offsets{},scales{};std::array<float,16> rotations{};std::array<float,6> pbrx{},pbry{};
    auto &binding=textures_->bindings.at(r.object);
    for(int n=0;n<16;++n){auto &slot=binding[n];glActiveTexture(GL_TEXTURE0+n);
      if(n==14){glBindTexture(GL_TEXTURE_BUFFER,(t.derived||t.overlay)?textures_->packed_sources.at(t.packed_inputs).texture:0);glBindSampler(n,0);}
      else {glBindTexture(GL_TEXTURE_2D,slot.image);glBindSampler(n,slot.sampler);}
      present[n]=slot.image!=0;coordinates[n]=slot.coordinate;
      int role=n==12?0:n==13?3:n;scales[n*2]=scales[n*2+1]=1;
      if(role<11){auto &texture=t.material.textures[role];coordinates[n]=role==int(MToonSlot::Matcap)?0:mtoon_coordinate(texture);offsets[n*2]=texture.offset[0];offsets[n*2+1]=texture.offset[1];scales[n*2]=texture.scale[0];scales[n*2+1]=texture.scale[1];rotations[n]=texture.rotation;channels[n]=texture.channel;}
      if(n>=14){int k=n-14;pbrx[k*3]=slot.uv[0];pbrx[k*3+1]=slot.uv[1];pbrx[k*3+2]=slot.uv[4];pbry[k*3]=slot.uv[2];pbry[k*3+1]=slot.uv[3];pbry[k*3+2]=slot.uv[5];}
      auto name="tex"+std::to_string(n);glUniform1i(glGetUniformLocation(program_,name.c_str()),n);
    }
    if(t.derived||t.overlay) {
      if(t.derived){derived[0]=1;derived[3]=!t.native_emission;}
      const auto &pack=textures_->packed_sources.at(t.packed_inputs);
      std::array<std::array<GLint,4>,4> sampling{};
      for(size_t view=0;view<4;++view)sampling[view]=TextureState::sampler(t.packed_views[view]);
      glUniform1i(glGetUniformLocation(program_,"packedSources"),14);
      glUniform4iv(glGetUniformLocation(program_,"mt_pack_levels[0]"),GLsizei(MToonSourcePacks::view_count*14),pack.levels[0].data());
      glUniform2iv(glGetUniformLocation(program_,"mt_pack_info[0]"),4,pack.info[0].data());
      glUniform4iv(glGetUniformLocation(program_,"mt_pack_sampler[0]"),4,sampling[0].data());
      const auto &uv=t.packed_views[2].uv_transform;
      pbrx[3]=uv[0];pbrx[4]=uv[1];pbrx[5]=uv[4];pbry[3]=uv[2];pbry[4]=uv[3];pbry[5]=uv[5];
    }
    glUniform1i(glGetUniformLocation(program_,"source_frame"),r.source_frame);glUniform1i(glGetUniformLocation(program_,"canonical_frame"),r.canonical_frame);
    glUniform1i(glGetUniformLocation(program_,"prepared_pbr_frame"),0);
    glUniform1i(glGetUniformLocation(program_,"paint_domain"),bool(r.paint_sampling_attachment)||(r.textures&&r.textures->overlay&&r.textures->overlay->original_uv0()));
    glUniform1i(glGetUniformLocation(program_,"retained_normal_domain"),r.textures&&r.textures->normal_projection&&r.textures->normal_projection->version==2);
    glUniform1i(glGetUniformLocation(program_,"mt_mode"),1);glUniform1i(glGetUniformLocation(program_,"mt_outline"),outline);glUniform1i(glGetUniformLocation(program_,"mt_baked_normal"),t.baked_normal);glUniform4fv(glGetUniformLocation(program_,"mt[0]"),12,u[0].data());
    glUniform1iv(glGetUniformLocation(program_,"mt_present[0]"),16,present.data());glUniform1iv(glGetUniformLocation(program_,"mt_coord[0]"),16,coordinates.data());glUniform1iv(glGetUniformLocation(program_,"mt_derived[0]"),16,derived.data());glUniform1iv(glGetUniformLocation(program_,"mt_channel[0]"),16,channels.data());
    glUniform2fv(glGetUniformLocation(program_,"mt_offset[0]"),16,offsets.data());glUniform2fv(glGetUniformLocation(program_,"mt_scale[0]"),16,scales.data());glUniform1fv(glGetUniformLocation(program_,"mt_rotation[0]"),16,rotations.data());glUniform3fv(glGetUniformLocation(program_,"mt_pbr_x[0]"),2,pbrx.data());glUniform3fv(glGetUniformLocation(program_,"mt_pbr_y[0]"),2,pbry.data());
    glUniform3fv(glGetUniformLocation(program_,"mt_effective_emission"),1,effective_emission.data());
    glUniform1i(glGetUniformLocation(program_,"mt_overlay"),t.native_emission?(t.overlay?2:3):int(bool(t.overlay)));
    glUniform1f(glGetUniformLocation(program_,"mt_emission_strength"),emission_strength);
    glUniform1i(glGetUniformLocation(program_,"mt_absolute_background"),background!=textures_->physical_backgrounds.end());
    if(background!=textures_->physical_backgrounds.end())glUniform3fv(glGetUniformLocation(program_,"mt_background"),1,background->second.data());
    glUniform3fv(glGetUniformLocation(program_,"mt_native_emission"),1,r.material.emissive.data());
    glUniform4fv(glGetUniformLocation(program_,"mt_effective_color"),1,effective.data());glUniform4fv(glGetUniformLocation(program_,"mt_native_color"),1,r.material.color.data());glUniform1i(glGetUniformLocation(program_,"mt_shade_alias"),t.shade_alias);
    auto state=toon_state(r,t,outline);
    if(state.cull==MToonCull::Off)glDisable(GL_CULL_FACE);else{glEnable(GL_CULL_FACE);glCullFace(state.cull==MToonCull::Front?GL_FRONT:GL_BACK);}
    if(state.blend){if(state.destination_rgb==9||state.destination_alpha==9)throw std::runtime_error("MToon destination SRC_ALPHA_SATURATE is unsupported");glEnable(GL_BLEND);glBlendFuncSeparate(mtoon_blend(state.source_rgb),mtoon_blend(state.destination_rgb),mtoon_blend(state.source_alpha),mtoon_blend(state.destination_alpha));}else glDisable(GL_BLEND);
    glBlendEquationSeparate(GL_FUNC_ADD,state.maximum_alpha?GL_MAX:GL_FUNC_ADD);glDepthMask(state.depth_write?GL_TRUE:GL_FALSE);glDepthFunc(GL_LEQUAL);
    if(state.alpha_to_coverage)glEnable(GL_SAMPLE_ALPHA_TO_COVERAGE);else glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    if(outline&&t.material.version()==MToonVersion::V0){glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(1,1);}else glDisable(GL_POLYGON_OFFSET_FILL);
    if(ghost_pass){glEnable(GL_BLEND);glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);glDepthMask(GL_FALSE);glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);}
    glDrawElements(GL_TRIANGLES,GLsizei(r.count),GL_UNSIGNED_INT,reinterpret_cast<void*>(r.first*sizeof(uint32_t)));glDisable(GL_POLYGON_OFFSET_FILL);
  };

  if (cached_->ranges.empty()) {
    glUniform1i(glGetUniformLocation(program_,"weight_pass"),!cached_->selected_weights.empty());
    draw_range(Material{}, 0, count_, 0, nullptr,false,false,cached_->pbr_view);
  }
  else {
    std::vector<const SurfaceRange*> ordered;for(auto &r:cached_->ranges)ordered.push_back(&r);
    auto is_ghost=[&](uint64_t id){auto i=std::find_if(display.begin(),display.end(),[&](const auto &o){return o.id==id;});return i!=display.end() && i->ghost;};
    auto queue=[&](const SurfaceRange&r){if(is_ghost(r.object))return 3500;auto toon=textures_->toons.find(r.object);if(toon!=textures_->toons.end())return toon_state(r,toon->second,false).queue;const auto background=detail::vrm1_source_background(r.material,r.material_bindings.get(),r.appearance.get(),r.textures.get(),r.pbr_view);return blended(r.material,r.textures.get(),r.object,background?std::optional<float>(background->color[3]):std::nullopt)?3000:r.material.alpha==Material::AlphaMode::Mask?2450:2000;};
    std::stable_sort(ordered.begin(),ordered.end(),[&](auto a,auto b){return queue(*a)<queue(*b);});
    for(auto r:ordered){
      if(replaced_object&&r->object==replaced_object)continue;
      auto object_model=model;
      auto current=std::find_if(display.begin(),display.end(),[&](const auto &o){return o.id==r->object;});
      if(current!=display.end()&&!current->visible)continue;
      if(current!=display.end() && current->world_transform_valid && r->world_frame_valid)
        object_model=ob::compose(model,ob::compose(current->world_transform,ob::inverse(r->object_world)));
      auto local_model=model_matrix(object_model);
      const bool affine_preview=current!=display.end()&&current->affine_world&&r->affine_world&&current->affine_world->matrix()!=r->affine_world->matrix();
      bool reversing=false;
      if(affine_preview) {
        auto delta=(*current->affine_world)*r->affine_world->inverse();
        auto full=affine_similarity(model)*delta;
        local_model=full.float_matrix();const auto inverse=full.inverse().matrix();
        std::array<float,9> normal{};
        for(size_t c=0;c<3;++c)for(size_t row=0;row<3;++row){normal[c*3+row]=float(inverse[row*4+c]);if(!std::isfinite(normal[c*3+row]))throw std::overflow_error("Affine preview normal matrix exceeds GPU range");}
        glUniformMatrix3fv(glGetUniformLocation(program_,"normal_model"),1,GL_FALSE,normal.data());
        reversing=delta.orientation()==AffineOrientation::Reversing;
      }
      glUniform1i(glGetUniformLocation(program_,"affine_motion"),affine_preview);
      glUniform1f(glGetUniformLocation(program_,"model_hand"),reversing?-1.f:1.f);
      glFrontFace(reversing?GL_CW:GL_CCW);
      const auto local_mvp=multiply(vp,local_model);
      glUniformMatrix4fv(glGetUniformLocation(program_,"model"),1,GL_FALSE,local_model.data());
      glUniformMatrix4fv(glGetUniformLocation(program_,"mvp"),1,GL_FALSE,local_mvp.data());
      ghost_pass=is_ghost(r->object);glUniform1f(glGetUniformLocation(program_,"ghost_opacity"),ghost_pass?.18f:1.f);
      auto selected=std::find_if(display.begin(),display.end(),[&](const auto&o){return o.id==r->object;});
      const auto accent=selected==display.end()?0x6babff:selected->selection_accent;
      glUniform3f(glGetUniformLocation(program_,"selection_color"),float((accent>>16)&255)/255,float((accent>>8)&255)/255,float(accent&255)/255);
      glUniform1f(glGetUniformLocation(program_,"selection_amount"),selected==display.end()?0:selected->hovered?.3f:selected->active?.12f:selected->selected?.05f:0);

      glUniform1i(glGetUniformLocation(program_,"clip_enabled"),cached_->clipped_object&&r->object==cached_->clipped_object);
      bool weighted=!ghost_pass && textures_->weighted.contains(r->object);
      glUniform1i(glGetUniformLocation(program_,"weight_pass"),weighted);
      auto typed=textures_->toons.find(r->object);
      if(typed==textures_->toons.end()){const auto background=detail::vrm1_source_background(r->material,r->material_bindings.get(),r->appearance.get(),r->textures.get(),r->pbr_view);draw_range(r->material,r->first,r->count,r->object,r->textures.get(),r->source_frame,r->canonical_frame,r->pbr_view,bool(r->paint_sampling_attachment),bool(detail::retained_normal_coordinates(r->material_bindings.get(),r->textures.get())),r->material_bindings.get(),background?&*background:nullptr);}
      else{
        draw_toon(*r,typed->second,false);
        if(!ghost_pass&&!weighted&&typed->second.material.parameters.outline!=MToonOutline::None&&typed->second.material.parameters.outline_width>0)
          draw_toon(*r,typed->second,true);
      }
    }
  }
  glFrontFace(GL_CCW);
  glUniform1i(glGetUniformLocation(program_,"weight_pass"),0);
  for(int n=0;n<16;++n)glBindSampler(n,0);
  glActiveTexture(GL_TEXTURE0);
  glDepthMask(GL_TRUE);glDepthFunc(GL_LESS);glBlendEquationSeparate(GL_FUNC_ADD,GL_FUNC_ADD);glDisable(GL_SAMPLE_ALPHA_TO_COVERAGE);
  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);
  glBindVertexArray(0);
  glUseProgram(0);
  if (glGetError() != GL_NO_ERROR){textures_->clear(true);cached_.reset();count_=0;throw std::runtime_error("Sculpt GPU rendering failed");}
  if(!textures_->deferred)resolve();
  }catch(...){cached_.reset();count_=0;textures_->target.abort();throw;}
}

namespace {
struct CursorVertex {Vec3 p,color;};
// Positions remain in object space. Marker offsets are world-axis unit vectors
// scaled by one uniform, so navigation/object motion never rebuilds this buffer.
struct RetopoVertexGpu {Vec3 p,color,marker;};
struct CursorGlState {
  GLint program=0,vao=0,buffer=0,depth_function=0;GLboolean depth_mask=GL_TRUE,depth_test=GL_FALSE;
  CursorGlState(){glGetIntegerv(GL_CURRENT_PROGRAM,&program);glGetIntegerv(GL_VERTEX_ARRAY_BINDING,&vao);glGetIntegerv(GL_ARRAY_BUFFER_BINDING,&buffer);glGetIntegerv(GL_DEPTH_FUNC,&depth_function);glGetBooleanv(GL_DEPTH_WRITEMASK,&depth_mask);depth_test=glIsEnabled(GL_DEPTH_TEST);}
  ~CursorGlState(){glUseProgram(GLuint(program));glBindVertexArray(GLuint(vao));glBindBuffer(GL_ARRAY_BUFFER,GLuint(buffer));glDepthFunc(GLenum(depth_function));glDepthMask(depth_mask);if(depth_test)glEnable(GL_DEPTH_TEST);else glDisable(GL_DEPTH_TEST);}
};

}

namespace {
// Constant six-vertex projector plane. Camera/UV changes are uniforms; image
// upload is keyed by its immutable owner, independent of scene geometry.
struct ProjectionPreviewGpu {
  GLuint program=0,vao=0,buffer=0,texture=0;
  std::shared_ptr<const Image> image;
  void clear(bool release){if(release){if(texture)glDeleteTextures(1,&texture);if(buffer)glDeleteBuffers(1,&buffer);if(vao)glDeleteVertexArrays(1,&vao);if(program)glDeleteProgram(program);}program=vao=buffer=texture=0;image.reset();}
  ~ProjectionPreviewGpu(){clear(true);}
  void draw(const EditorState &state,const Mat4 &vp,bool linear){
    if(!state.projection||!state.projection->image||!state.projection->settings.preview){if(texture){glDeleteTextures(1,&texture);texture=0;}image.reset();return;}
    const auto &tool=*state.projection;const auto &settings=tool.settings;
    GLint old_active=0,old_texture=0,old_sampler=0,old_src=0,old_dst=0,old_src_a=0,old_dst_a=0;
    glGetIntegerv(GL_ACTIVE_TEXTURE,&old_active);glActiveTexture(GL_TEXTURE0);glGetIntegerv(GL_TEXTURE_BINDING_2D,&old_texture);glGetIntegerv(GL_SAMPLER_BINDING,&old_sampler);
    const bool blend=glIsEnabled(GL_BLEND),cull=glIsEnabled(GL_CULL_FACE);glGetIntegerv(GL_BLEND_SRC_RGB,&old_src);glGetIntegerv(GL_BLEND_DST_RGB,&old_dst);glGetIntegerv(GL_BLEND_SRC_ALPHA,&old_src_a);glGetIntegerv(GL_BLEND_DST_ALPHA,&old_dst_a);
    struct Restore {GLint active,texture,sampler,src,dst,srca,dsta;bool blend,cull;~Restore(){glBlendFuncSeparate(src,dst,srca,dsta);if(blend)glEnable(GL_BLEND);else glDisable(GL_BLEND);if(cull)glEnable(GL_CULL_FACE);else glDisable(GL_CULL_FACE);glBindTexture(GL_TEXTURE_2D,GLuint(texture));glBindSampler(0,GLuint(sampler));glActiveTexture(GLenum(active));}}restore{old_active,old_texture,old_sampler,old_src,old_dst,old_src_a,old_dst_a,blend,cull};
    if(!program){
      GLuint vs=0,fs=0;try{
        vs=shader(GL_VERTEX_SHADER,R"(#version 330 core
layout(location=0)in vec3 coordinate;uniform mat4 matrix;uniform vec3 extent;out vec2 uv;
void main(){uv=coordinate.xy;gl_Position=matrix*vec4((coordinate.x-.5)*extent.x,(.5-coordinate.y)*extent.y,-coordinate.z*extent.z,1);})");
        fs=shader(GL_FRAGMENT_SHADER,R"(#version 330 core
in vec2 uv;uniform sampler2D picture;uniform mat3 image_uv;uniform float opacity;uniform int srgb,linear_output,stencil,scalar,wire,coverage_alpha;out vec4 color;
vec3 decode(vec3 v){return mix(v/12.92,pow((v+.055)/1.055,vec3(2.4)),greaterThan(v,vec3(.04045)));}
vec3 encode(vec3 v){return mix(v*12.92,1.055*pow(max(v,vec3(0)),vec3(1./2.4))-.055,greaterThan(v,vec3(.0031308)));}
vec4 sampleImage(vec2 uv){ivec2 size=textureSize(picture,0);vec2 q=uv*vec2(size)-.5;ivec2 base=ivec2(floor(q));vec2 f=fract(q);vec4 result=vec4(0);for(int y=0;y<2;++y)for(int x=0;x<2;++x){vec4 c=texelFetch(picture,clamp(base+ivec2(x,y),ivec2(0),size-1),0);if(srgb==1)c.rgb=decode(c.rgb);if(coverage_alpha==1)c.rgb*=c.a;result+=c*(x==0?1-f.x:f.x)*(y==0?1-f.y:f.y);}if(coverage_alpha==1)result.rgb=result.a>1e-8?result.rgb/result.a:vec3(0);return clamp(result,0,1);}
void main(){if(wire==1){vec3 c=vec3(.2,.8,1);color=vec4(linear_output==1?decode(c):c,1);return;}vec2 p=(image_uv*vec3(uv,1)).xy;if(any(lessThan(p,vec2(0)))||any(greaterThan(p,vec2(1))))discard;vec4 v=sampleImage(p);if(stencil==1){float x=scalar==1?v.r:scalar==2?v.g:scalar==3?v.b:scalar==4?v.a:dot(v.rgb,vec3(.2126,.7152,.0722));v.rgb=vec3(x);}if(linear_output==0)v.rgb=encode(v.rgb);color=vec4(v.rgb,v.a*opacity);})");
        program=glCreateProgram();glAttachShader(program,vs);glAttachShader(program,fs);glLinkProgram(program);GLint linked=0;glGetProgramiv(program,GL_LINK_STATUS,&linked);if(!linked)throw std::runtime_error("Projector preview shader failed");glDeleteShader(vs);vs=0;glDeleteShader(fs);fs=0;
        const float quad[]={
          0,0,1, 1,0,1, 0,1,1, 0,1,1, 1,0,1, 1,1,1,
          0,0,1, 1,0,1, 1,0,1, 1,1,1, 1,1,1, 0,1,1, 0,1,1, 0,0,1,
          .5f,.5f,0, 0,0,1, .5f,.5f,0, 1,0,1, .5f,.5f,0, 1,1,1, .5f,.5f,0, 0,1,1};glGenVertexArrays(1,&vao);glGenBuffers(1,&buffer);glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,buffer);glBufferData(GL_ARRAY_BUFFER,sizeof(quad),quad,GL_STATIC_DRAW);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,3*sizeof(float),nullptr);glEnableVertexAttribArray(0);
      }catch(...){if(vs)glDeleteShader(vs);if(fs)glDeleteShader(fs);clear(true);throw;}
    }
    if(image!=tool.image->pixels){
      const auto &pixels=*tool.image->pixels;pixels.validate();GLint maximum=0;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&maximum);if(pixels.width>uint32_t(maximum)||pixels.height>uint32_t(maximum))throw std::length_error("Projector image exceeds GPU texture size");
      if(!texture)glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,GLsizei(pixels.width),GLsizei(pixels.height),0,GL_RGBA,GL_UNSIGNED_BYTE,pixels.rgba.data());glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
      if(glGetError()!=GL_NO_ERROR){glDeleteTextures(1,&texture);texture=0;image.reset();throw std::runtime_error("Projector preview image upload failed");}image=tool.image->pixels;
    }
    auto camera=settings.camera;const float depth=settings.preview_depth;
    float height=camera.mode==ProjectionMode::Perspective?2*std::tan(camera.vertical_fov*.5f)*depth:camera.height;
    float width=camera.mode==ProjectionMode::Perspective?height*camera.aspect:camera.width;
    auto matrix=model_matrix(ob::compose(state.canvas,camera.pose));
    const auto mvp=multiply(vp,matrix);auto &m=settings.image.uv_transform;const float uv[]={m[0],m[2],0,m[1],m[3],0,m[4],m[5],1};
    glUseProgram(program);glUniformMatrix4fv(glGetUniformLocation(program,"matrix"),1,GL_FALSE,mvp.data());glUniformMatrix3fv(glGetUniformLocation(program,"image_uv"),1,GL_FALSE,uv);glUniform3f(glGetUniformLocation(program,"extent"),width,height,depth);glUniform1i(glGetUniformLocation(program,"wire"),0);glUniform1i(glGetUniformLocation(program,"picture"),0);
    glUniform1i(glGetUniformLocation(program,"linear_output"),linear);glUniform1i(glGetUniformLocation(program,"srgb"),settings.image.color_space==ProjectionColorSpace::SRGB||(settings.image.color_space==ProjectionColorSpace::Automatic&&settings.image.usage==ProjectionUsage::Color&&(state.paint.channel==PaintChannel::Color||state.paint.channel==PaintChannel::Emission)));glUniform1i(glGetUniformLocation(program,"coverage_alpha"),settings.image.alpha==ProjectionAlpha::Coverage);glUniform1i(glGetUniformLocation(program,"stencil"),settings.image.usage==ProjectionUsage::Stencil);glUniform1i(glGetUniformLocation(program,"scalar"),int(settings.image.scalar));glUniform1f(glGetUniformLocation(program,"opacity"),settings.image.opacity*.45f);
    glBindTexture(GL_TEXTURE_2D,texture);glBindSampler(0,0);glBindVertexArray(vao);glEnable(GL_BLEND);glBlendFuncSeparate(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ONE_MINUS_SRC_ALPHA);glDisable(GL_CULL_FACE);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDepthMask(GL_FALSE);glDrawArrays(GL_TRIANGLES,0,6);glUniform1i(glGetUniformLocation(program,"wire"),1);glDrawArrays(GL_LINES,6,camera.mode==ProjectionMode::Perspective?16:8);
  }
};
}
struct CursorRenderer::OverlayCache {
  ProjectionPreviewGpu projection;
  GLuint vao=0,buffer=0;size_t count=0;uint64_t uploads=0,revision=0;
  std::shared_ptr<const RetopoMesh> topology;
  std::vector<TopologyID> selected;
  void clear(bool release){if(release){if(buffer)glDeleteBuffers(1,&buffer);if(vao)glDeleteVertexArrays(1,&vao);}vao=buffer=0;count=0;topology.reset();selected.clear();revision=0;}
  ~OverlayCache(){clear(true);}
  void prepare(const EditorState &state) {
    if(!state.retopology || !state.retopology->topology){clear(true);return;}
    const auto &view=*state.retopology;
    if(topology==view.topology && revision==view.revision && selected==view.selected)return;
    constexpr size_t max_vertices=100'000,max_edges=300'000,max_bytes=64*1024*1024;
    if(view.topology->vertices().size()>max_vertices || view.topology->edges().size()>max_edges || view.selected.size()>max_vertices)
      throw std::length_error("Retopology overlay exceeds native topology limits");
    const size_t vertices=view.topology->edges().size()*2+view.topology->vertices().size()*6+(view.selected.empty()?0:(view.selected.size()-1)*2);
    if(vertices>max_bytes/sizeof(RetopoVertexGpu))throw std::length_error("Retopology overlay GPU buffer exceeds64MiB");
    std::vector<RetopoVertexGpu> lines;lines.reserve(vertices);
    std::unordered_set<TopologyID> selection(view.selected.begin(),view.selected.end());
    for(auto id:view.selected)if(!view.topology->vertices().contains(id))throw std::invalid_argument("Retopology selection refers to a missing vertex");
    auto point=[&](TopologyID id){auto p=view.topology->vertices().at(id).position;if(!finite(p))throw std::invalid_argument("Retopology overlay coordinate is nonfinite");return p;};
    auto line=[&](Vec3 a,Vec3 b,Vec3 color){lines.push_back({a,color,{}});lines.push_back({b,color,{}});};
    auto marker=[&](Vec3 point,Vec3 offset,Vec3 color){lines.push_back({point,color,offset*-1});lines.push_back({point,color,offset});};
    for(const auto &[id,edge]:view.topology->edges()) {
      (void)id;line(point(edge.a),point(edge.b),selection.contains(edge.a)&&selection.contains(edge.b)?Vec3{1,.8f,.05f}:edge.seam?Vec3{.1f,.8f,1}:Vec3{.1f,1,.3f});
    }
    for(const auto &[id,vertex]:view.topology->vertices()) {
      auto p=point(id);Vec3 color=selection.contains(id)?Vec3{1,.8f,.05f}:vertex.pinned?Vec3{1,.2f,.8f}:Vec3{.1f,1,.3f};
      marker(p,{1,0,0},color);marker(p,{0,1,0},color);marker(p,{0,0,1},color);
    }
    for(size_t i=1;i<view.selected.size();++i)line(point(view.selected[i-1]),point(view.selected[i]),{1,.8f,.05f});
    if(lines.size()!=vertices)throw std::logic_error("Retopology overlay buffer accounting differs");
    if(!lines.empty()) {
      if(!vao)glGenVertexArrays(1,&vao);
      if(!buffer)glGenBuffers(1,&buffer);
      glBindVertexArray(vao);glBindBuffer(GL_ARRAY_BUFFER,buffer);
      glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(lines.size()*sizeof(RetopoVertexGpu)),lines.data(),GL_STATIC_DRAW);
      if(glGetError()!=GL_NO_ERROR){clear(true);throw std::runtime_error("Retopology overlay GPU upload failed");}
      glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(RetopoVertexGpu),nullptr);glEnableVertexAttribArray(0);
      glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(RetopoVertexGpu),reinterpret_cast<void*>(offsetof(RetopoVertexGpu,color)));glEnableVertexAttribArray(1);
      glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,sizeof(RetopoVertexGpu),reinterpret_cast<void*>(offsetof(RetopoVertexGpu,marker)));glEnableVertexAttribArray(2);++uploads;
    } else {if(buffer)glDeleteBuffers(1,&buffer);if(vao)glDeleteVertexArrays(1,&vao);buffer=vao=0;}
    count=lines.size();topology=view.topology;revision=view.revision;selected=view.selected;
  }
};
RetopoOverlayStats CursorRenderer::retopology_stats() const {return {overlay_->count,overlay_->count*sizeof(RetopoVertexGpu),overlay_->uploads};}
void CursorRenderer::abandon(){program_=vao_=buffer_=0;overlay_->projection.clear(false);overlay_->clear(false);}
CursorRenderer::CursorRenderer():overlay_(std::make_unique<OverlayCache>()) {
  const char *v = R"(#version 330 core
layout(location=0)in vec3 position;layout(location=1)in vec3 color;layout(location=2)in vec3 marker;
uniform mat4 vp,overlay_model;uniform float depth_bias,marker_radius;uniform int local_overlay;out vec3 tint;
void main(){tint=color;vec4 p=local_overlay==1?overlay_model*vec4(position,1)+vec4(marker*marker_radius,0):vec4(position,1);gl_Position=vp*p;gl_Position.z-=depth_bias*gl_Position.w;})";
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
  CursorGlState previous;
  overlay_->prepare(state);
  overlay_->projection.draw(state,vp,linear_output_);
  std::vector<CursorVertex> vertices;
  auto line = [&](ob::Vec3 a, ob::Vec3 b, Vec3 color) {
    vertices.push_back({{a.x, a.y, a.z}, color});
    vertices.push_back({{b.x, b.y, b.z}, color});
  };
  if(state.rig_fit){
    const auto &view=*state.rig_fit;
    // Required-role display labels include a suffix; landmark keys do not.
    // Keep existing-rig lookup/validation while using raw landmark identity.
    const auto selected=view.rest_source?rig_fit_selection_label(view):human_roles().at(view.role).name;
    auto point=[&](Vec3 p){auto w=view.world.point({p.x,p.y,p.z});return ob::transform(state.canvas,{float(w[0]),float(w[1]),float(w[2])});};
    const float radius=std::clamp(view.step*.3f,.002f,.02f)*state.canvas.scale;
    if(view.rest_source){for(size_t j=0;j<view.bone_heads.size();++j){const auto center=point(view.bone_heads[j]);const auto color=j==view.role?Vec3{1,.8f,.05f}:Vec3{.1f,1,.4f};for(auto axis:{ob::Vec3{radius,0,0},ob::Vec3{0,radius,0},ob::Vec3{0,0,radius}})line(center-axis,center+axis,color);const auto parent=view.bone_parents.at(j);if(parent>=0)line(point(view.bone_heads.at(size_t(parent))),center,color);}}
    else
    for(const auto &[name,p]:view.landmarks.points){const auto center=point(p);const auto color=name==selected?Vec3{1,.8f,.05f}:Vec3{.1f,1,.4f};
      for(auto axis:{ob::Vec3{radius,0,0},ob::Vec3{0,radius,0},ob::Vec3{0,0,radius}})line(center-axis,center+axis,color);
      auto role=std::find_if(human_roles().begin(),human_roles().end(),[&](const auto &r){return r.name==name;});
      if(role!=human_roles().end()){auto parent=role->parent;while(!parent.empty()&&!view.landmarks.points.contains(parent)){auto pr=std::find_if(human_roles().begin(),human_roles().end(),[&](const auto&r){return r.name==parent;});if(pr==human_roles().end())break;parent=pr->parent;}if(view.landmarks.points.contains(parent))line(point(view.landmarks.points.at(parent)),center,color);}
    }
  }
  if(state.cut && state.cut->ready){
    auto &view=*state.cut;
    auto plane=view.preview?view.preview->plane:cut_plane(view.settings,view.center);
    auto normal=plane.normal,center=view.center+normal*(plane.offset-dot(normal,view.center));
    auto u=normalized(cross(std::abs(normal.y)<.9f?Vec3{0,1,0}:Vec3{1,0,0},normal));
    auto v=cross(normal,u);float extent=view.radius*.8f;
    auto point=[&](Vec3 p){return ob::transform(state.canvas,{p.x,p.y,p.z});};
    Vec3 tint=view.error.empty()?Vec3{.1f,.8f,1}:Vec3{1,.2f,.1f};
    for(int k=-2;k<=2;++k){float offset=extent*k*.5f;
      line(point(center+u*offset-v*extent),point(center+u*offset+v*extent),tint);
      line(point(center+v*offset-u*extent),point(center+v*offset+u*extent),tint);}
    line(point(center),point(center+normal*std::min(.2f,view.radius*.5f)),{.1f,1,.3f});
  }
  for (size_t hand=0;hand<state.hands.size();++hand) {
    const auto &h=state.hands[hand];
    if (h.tracked.valid) {
      auto p = h.tracked.grip.position;
      auto axis = ob::rotate(h.tracked.grip.orientation, {0, 0, -.08f});
      line(p, p + axis, {.9f, .75f, .2f});
      if ((state.controls.parameter != VrParameter::None || (!state.menu&&state.hierarchy_open)) && h.tracked.aimValid) {
        auto from = h.tracked.aim.position;
        const auto &panel=!state.menu&&state.hierarchy_open?state.hierarchy_panel:state.panel;
        auto hit = panel_ray(panel, h.tracked.aim);
        auto to =
            hit ? hit->point
                : from + ob::rotate(h.tracked.aim.orientation, {0, 0, -.6f});
        Vec3 accent{float((state.accent >> 16) & 255) / 255,
                    float((state.accent >> 8) & 255) / 255,
                    float(state.accent & 255) / 255};
        line(from, to, accent);
        if (hit) {
          auto x = ob::rotate(panel.orientation, {.008f, 0, 0}),
               y = ob::rotate(panel.orientation, {0, .008f, 0});
          Vec3 contrast = state.light ? Vec3{0, 0, 0} : Vec3{1, 1, 1};
          line(to - x, to + x, contrast);
          line(to - y, to + y, contrast);
          line(to - x * .6f, to + x * .6f, accent);
          line(to - y * .6f, to + y * .6f, accent);
        }
      }
      if(state.polygon&&!state.menu&&h.tracked.aimValid){
        auto from=h.tracked.aim.position;
        auto to=from+ob::rotate(h.tracked.aim.orientation,{0,0,-.6f});
        line(from,to,{float((state.accent>>16)&255)/255,float((state.accent>>8)&255)/255,float(state.accent&255)/255});
      }
      if(state.selection_mode&&!state.menu&&state.hierarchy_hover[hand]<0){
        const bool hit=state.selection_hover[hand]!=0;
        auto from=h.tracked.aimValid?h.tracked.aim.position:p;
        auto point=state.selection_hit[hand];
        auto to=hit?ob::transform(state.canvas,{point.x,point.y,point.z}):from+ob::rotate(h.tracked.aim.orientation,{0,0,-.6f});
        Vec3 accent{float((state.accent>>16)&255)/255,float((state.accent>>8)&255)/255,float(state.accent&255)/255};
        line(from,to,hit?accent:Vec3{.45f,.5f,.55f});
        if(hit)for(int axis=0;axis<3;++axis){ob::Vec3 d=axis==0?ob::Vec3{.008f,0,0}:axis==1?ob::Vec3{0,.008f,0}:ob::Vec3{0,0,.008f};line(to-d,to+d,accent);}
      }
      if (!state.selection_mode && !state.menu && !state.cut && !state.polygon) {
        float r =
            state.radius * (h.tracked.trigger > .02f
                                ? .25f + .75f * std::sqrt(h.tracked.trigger)
                                : 1.f);
        if (state.retopology)
          r = std::min(.01f, state.retopology->snap_mm * .00025f) * state.canvas.scale;
        Vec3 color =
            state.tool == Tool::Carve ? Vec3{1, .2f, .2f} : Vec3{.1f, .9f, .5f};
        auto source = std::find_if(state.objects.begin(), state.objects.end(),
                                  [&](const auto &object) { return object.id == state.active_object; });
        bool sculpt = !state.mesh_active && !state.weights.enabled &&
                      !state.retopology && source != state.objects.end() && source->visible && !source->locked;
        if (sculpt && state.sculpt_settings.mask_edit)
          color = state.sculpt_settings.mask_tool == Volume::MaskTool::Subtract ? Vec3{1,.65f,.25f}
                    : state.sculpt_settings.mask_tool == Volume::MaskTool::Smooth ? Vec3{.8f,.65f,1}
                    : Vec3{.35f,.6f,1};
        std::vector<ob::Vec3> centers{p};
        if (sculpt && state.sculpt_settings.symmetry_axes) {
          auto frame = ob::compose(state.canvas, state.sculpt_transform);
          auto local = ob::transform(ob::inverse(frame), p);
          auto origin = state.sculpt_settings.symmetry_origin;
          unsigned axes = state.sculpt_settings.symmetry_axes & 7;
          if (local.x == origin.x) axes &= ~1u;
          if (local.y == origin.y) axes &= ~2u;
          if (local.z == origin.z) axes &= ~4u;
          for (unsigned bits = 1; bits < 8; ++bits) {
            if (bits & ~axes) continue;
            auto reflected = local;
            if (bits & 1) reflected.x = 2 * origin.x - reflected.x;
            if (bits & 2) reflected.y = 2 * origin.y - reflected.y;
            if (bits & 4) reflected.z = 2 * origin.z - reflected.z;
            centers.push_back(ob::transform(frame, reflected));
          }
        }
        for (auto center : centers)
        for (int axis = 0; axis < 3; ++axis)
          for (int k = 0; k < 32; ++k) {
            float a = k * 6.283185307f / 32, b = (k + 1) * 6.283185307f / 32;
            auto point = [&](float t) {
              float x = std::cos(t) * r, y = std::sin(t) * r;
              return center + (axis == 0   ? ob::Vec3{x, y, 0}
                          : axis == 1 ? ob::Vec3{x, 0, y}
                                      : ob::Vec3{0, x, y});
            };
            line(point(a), point(b), color);
          }
      }
    }
  }
  if(vertices.empty() && !overlay_->count)return;
  glDepthMask(GL_FALSE);glUseProgram(program_);
  glUniform1i(glGetUniformLocation(program_,"linear_output"),linear_output_);
  glUniformMatrix4fv(glGetUniformLocation(program_,"vp"),1,GL_FALSE,vp.data());
  if(overlay_->count) {
    // Topology is source-occluded even while a control ray ignores depth.
    glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);
    auto overlay_model=model_matrix(ob::compose(state.canvas,state.retopology->transform));
    const float radius=.0025f*state.canvas.scale;
    for(float v:overlay_model)if(!std::isfinite(v))throw std::invalid_argument("Retopology overlay transform is nonfinite");
    if(!std::isfinite(radius))throw std::invalid_argument("Retopology marker scale is nonfinite");
    glUniformMatrix4fv(glGetUniformLocation(program_,"overlay_model"),1,GL_FALSE,overlay_model.data());
    glUniform1f(glGetUniformLocation(program_,"marker_radius"),radius);glUniform1i(glGetUniformLocation(program_,"local_overlay"),1);
    glUniform1f(glGetUniformLocation(program_,"depth_bias"),.0001f);
    glBindVertexArray(overlay_->vao);glDrawArrays(GL_LINES,0,GLsizei(overlay_->count));
  }
  if(vertices.empty())return;
  glDepthFunc(GLenum(previous.depth_function));
  if(state.controls.parameter==VrParameter::None&&!state.hierarchy_open)glEnable(GL_DEPTH_TEST);else glDisable(GL_DEPTH_TEST);
  glUniform1i(glGetUniformLocation(program_,"local_overlay"),0);
  glUniform1f(glGetUniformLocation(program_,"depth_bias"),(state.retopology||state.cut)?.0001f:0);
  glBindVertexArray(vao_);glBindBuffer(GL_ARRAY_BUFFER,buffer_);
  glBufferData(GL_ARRAY_BUFFER,GLsizeiptr(vertices.size()*sizeof(CursorVertex)),vertices.data(),GL_STREAM_DRAW);
  glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(CursorVertex),nullptr);
  glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,sizeof(CursorVertex),reinterpret_cast<void*>(offsetof(CursorVertex,color)));
  glEnableVertexAttribArray(0);glEnableVertexAttribArray(1);
  glDrawArrays(GL_LINES,0,GLsizei(vertices.size()));
}
} // namespace dome
