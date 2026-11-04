// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/import-native.hpp"
#include "import-glb-builder.hpp"
#include "import-obj-libraries.hpp"
#include "text-utf8.hpp"
#include <array>
#include <exception>
#include <map>
#include <ufbx.h>
namespace dome {
namespace {
std::string string(ufbx_string s) {return {s.data,s.length};}
ufbx_vec3 subtract(ufbx_vec3 a,ufbx_vec3 b){return {a.x-b.x,a.y-b.y,a.z-b.z};}
ufbx_vec3 cross3(ufbx_vec3 a,ufbx_vec3 b){return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x};}
double dot3(ufbx_vec3 a,ufbx_vec3 b){return a.x*b.x+a.y*b.y+a.z*b.z;}
ufbx_vec3 normalize3(ufbx_vec3 a){auto length=std::hypot(a.x,a.y,a.z);if(!std::isfinite(length)||length==0)throw std::runtime_error("Native triangle has no valid normal");return {a.x/length,a.y/length,a.z/length};}
float real(double value) {
  if(!std::isfinite(value)||std::abs(value)>std::numeric_limits<float>::max())
    throw std::runtime_error("Native source attribute exceeds finite float range");
  return float(value);
}
struct Reader {
  ImportSourceCapture &capture;
  const ImportLimits &limits;
  std::exception_ptr failure;
  static ufbx_progress_result progress(void *user,const ufbx_progress*) noexcept {
    auto &r=*static_cast<Reader*>(user);
    try {if(r.limits.cancelled&&r.limits.cancelled())return UFBX_PROGRESS_CANCEL;}
    catch(...){r.failure=std::current_exception();return UFBX_PROGRESS_CANCEL;}
    return UFBX_PROGRESS_CONTINUE;
  }
  static bool open(void *user,ufbx_stream *stream,const char *path,size_t length,const ufbx_open_file_info *info) noexcept {
    auto &r=*static_cast<Reader*>(user);
    try {
      if(info->type!=UFBX_OPEN_FILE_OBJ_MTL)
        throw std::runtime_error("External FBX geometry caches are not supported by authoring import");
      auto bytes=r.capture.resource(std::string_view(path,length));
      ufbx_open_memory_opts options{};options.no_copy=true;
      return ufbx_open_memory_ctx(stream,info->context,bytes->data(),bytes->size(),&options,nullptr);
    } catch(...){r.failure=std::current_exception();return false;}
  }
};
void options_supported(const ImportOptions &o) {
  // These are shared authoring choices, not permission to silently discard a
  // channel. Non-preserving modes are implemented explicitly in later adapters.
  if(!o.hierarchy || !o.instances || !o.skins || !o.morphs)
    throw std::runtime_error("Flattening hierarchy, instances, skins or morphs is not implemented; use Preserve");
}
struct Converter {
  detail::ImportGlbBuilder builder;
  ImportSourceCapture &capture;
  const ImportOptions &options;
  ufbx_scene &source;
  std::vector<std::string> diagnostics;
  std::map<std::pair<const ufbx_texture*,std::string>,uint32_t> textures;
  std::map<std::pair<uint32_t,std::array<int,4>>,uint32_t> material_variants;
  std::map<std::string,std::string> material_directories;
  std::map<uint32_t,std::vector<uint32_t>> joint_helpers;
  size_t vertices=0,triangles=0,triangulation_work=0;
  size_t deformation_work=0;
  bool deformers_converted=false;
  bool generated_morph_normals=false;
  double coordinate_scale=1;
  std::array<double,4> root_rotation{0,0,0,1};
  Converter(ImportSourceCapture &c,const ImportOptions &o,ufbx_scene &s,ImportLimits l)
      :builder(std::move(l)),capture(c),options(o),source(s){}
  nlohmann::json identity(const ufbx_element &element) {
    // ufbx element_id is deterministic for this pinned parser/settings, NOT a
    // claim to be FBX's raw 64-bit object ID (retained in the original bytes).
    return {{"dome_source_ufbx_element",element.element_id}};
  }
  std::shared_ptr<const std::vector<uint8_t>> texture_bytes(const ufbx_texture &t,const std::string &directory) {
    if(t.content.size){
      if(t.content.size>builder.limits.max_resource_bytes)throw std::length_error("Embedded native texture byte budget exceeded");
      auto p=static_cast<const uint8_t*>(t.content.data);
      return std::make_shared<const std::vector<uint8_t>>(p,p+t.content.size);
    }
    auto name=string(t.filename);
    if(name.empty() || name.front()=='/' || name.find(':')!=name.npos)name=string(t.relative_filename);
    std::replace(name.begin(),name.end(),'\\','/');
    if(capture.source().format==ImportSourceFormat::Obj){
      auto relative=string(t.relative_filename);std::replace(relative.begin(),relative.end(),'\\','/');
      name=(std::filesystem::path(directory)/relative).lexically_normal().generic_string();
    }
    return capture.resource(name);
  }
  void retain_texture_sources(){
    if(capture.source().format==ImportSourceFormat::Fbx){
      for(auto *t:source.textures){builder.stop();if(t->type==UFBX_TEXTURE_FILE&&!t->content.size)(void)texture_bytes(*t,{});}
    }else{
      std::set<std::pair<const ufbx_texture*,std::string>> retained;
      for(auto *m:source.materials){auto directory=material_directories.contains(string(m->name))?material_directories.at(string(m->name)):std::string{};
        for(const auto &link:m->textures)if(link.texture)for(auto *t:link.texture->file_textures){builder.stop();
          if(!t->content.size&&retained.emplace(t,directory).second)(void)texture_bytes(*t,directory);}
      }
    }
  }
  uint32_t texture(const ufbx_texture &t,const std::string &directory) {
    if(auto found=textures.find({&t,directory});found!=textures.end())return found->second;
    if(t.type!=UFBX_TEXTURE_FILE || t.has_uv_transform)
      throw std::runtime_error("Layered/procedural/transformed FBX/OBJ texture requires an explicit native material adapter");
    auto encoded=texture_bytes(t,directory);
    std::string mime;
    constexpr uint8_t png[]={137,80,78,71,13,10,26,10};
    if(encoded->size()>=8&&std::equal(std::begin(png),std::end(png),encoded->begin()))mime="image/png";
    else if(encoded->size()>=3&&(*encoded)[0]==255&&(*encoded)[1]==216&&(*encoded)[2]==255)mime="image/jpeg";
    else throw std::runtime_error("Native material texture must be standard PNG or JPEG");
    auto &j=builder.json;
    if(!j.contains("images")){j["images"]=nlohmann::json::array();j["textures"]=nlohmann::json::array();j["samplers"]=nlohmann::json::array();}
    if(j["images"].size()>=256)throw std::length_error("Native source image count exceeds 256");
    auto view=j["bufferViews"].size();j["bufferViews"].push_back({{"buffer",0},{"byteOffset",builder.bytes(*encoded)},{"byteLength",encoded->size()}});
    auto image=j["images"].size();j["images"].push_back({{"name",string(t.name)},{"mimeType",mime},{"bufferView",view},{"extras",identity(t.element)}});
    auto sampler=j["samplers"].size();j["samplers"].push_back({{"wrapS",t.wrap_u==UFBX_WRAP_CLAMP?33071:10497},{"wrapT",t.wrap_v==UFBX_WRAP_CLAMP?33071:10497},{"minFilter",9729},{"magFilter",9729}});
    auto id=j["textures"].size();j["textures"].push_back({{"source",image},{"sampler",sampler}});textures.emplace(std::pair{&t,directory},uint32_t(id));return uint32_t(id);
  }
  void materials() {
    auto &j=builder.json;j["materials"]=nlohmann::json::array();
    // Adapter fallback is explicitly synthesized; absence of an FBX material is
    // never confused with original glTF's material zero/optional authority.
    j["materials"].push_back({{"name","Native default (adapter)"},{"pbrMetallicRoughness",{{"baseColorFactor",{1,1,1,1}},{"metallicFactor",0},{"roughnessFactor",1}}}});
    for(auto *m:source.materials) {
      builder.stop();auto &p=m->pbr;
      auto directory=material_directories.contains(string(m->name))?material_directories.at(string(m->name)):std::string{};
      float factor=p.base_factor.has_value?real(p.base_factor.value_real):1;
      auto c=p.base_color.value_vec3;
      std::array<float,4> color{real(c.x)*factor,real(c.y)*factor,real(c.z)*factor, p.opacity.has_value?real(p.opacity.value_real):1};
      nlohmann::json material={{"name",string(m->name)},{"extras",identity(m->element)}};
      if(!options.materials){j["materials"].push_back(j["materials"][0]);continue;}
      // Legacy FBX Lambert/Phong advertise transmission even for opaque
      // materials: TransparencyFactor=1 multiplied by TransparentColor=black.
      // Only this proven inactive legacy contribution is ignored. PBR layer
      // defaults, explicit optical effects and texture-driven terms still need
      // their own material adapter rather than losing authored shading.
      const bool opaque_legacy_transmission=
        (m->shader_type==UFBX_SHADER_FBX_LAMBERT||m->shader_type==UFBX_SHADER_FBX_PHONG)&&
        !m->features.pbr.enabled&&
        !(p.transmission_factor.texture_enabled&&p.transmission_factor.texture)&&
        !(p.transmission_color.texture_enabled&&p.transmission_color.texture)&&
        p.transmission_color.has_value&&p.transmission_color.value_vec3.x==0&&
        p.transmission_color.value_vec3.y==0&&p.transmission_color.value_vec3.z==0;
      if((m->features.coat.enabled&&p.coat_factor.value_real!=0) ||
          (m->features.sheen.enabled&&p.sheen_factor.value_real!=0) ||
          (m->features.transmission.enabled&&p.transmission_factor.value_real!=0&&!opaque_legacy_transmission) ||
          p.subsurface_factor.value_real!=0 || (p.displacement_map.texture_enabled&&p.displacement_map.texture))
        throw std::runtime_error("Native extended material requires explicit semantic conversion instead of dropping it");
      for(const auto &map:p.maps)if(map.texture_enabled&&map.texture && &map!=&p.base_color && &map!=&p.emission_color && &map!=&p.normal_map && &map!=&p.ambient_occlusion)
        throw std::runtime_error("Native material map requires explicit packing/conversion instead of dropping it");
      nlohmann::json pbr={{"baseColorFactor",color},{"metallicFactor",p.metalness.has_value?real(p.metalness.value_real):0},{"roughnessFactor",p.roughness.has_value?real(p.roughness.value_real):1}};
      if(p.base_color.texture && p.base_color.texture_enabled)pbr["baseColorTexture"]={{"index",texture(*p.base_color.texture,directory)}};
      material["pbrMetallicRoughness"]=std::move(pbr);
      if(color[3]<1)material["alphaMode"]="BLEND";
      if(m->features.double_sided.enabled)material["doubleSided"]=true;
      if(m->features.unlit.enabled){material["extensions"]["KHR_materials_unlit"]=nlohmann::json::object();j["extensionsUsed"]=nlohmann::json::array({"KHR_materials_unlit"});}
      float ef=p.emission_factor.has_value?real(p.emission_factor.value_real):1;auto ec=p.emission_color.value_vec3;
      if(p.emission_color.has_value)material["emissiveFactor"]={real(ec.x)*ef,real(ec.y)*ef,real(ec.z)*ef};
      if(p.emission_color.texture&&p.emission_color.texture_enabled)material["emissiveTexture"]={{"index",texture(*p.emission_color.texture,directory)}};
      if(p.normal_map.texture&&p.normal_map.texture_enabled)material["normalTexture"]={{"index",texture(*p.normal_map.texture,directory)}};
      if(p.ambient_occlusion.texture&&p.ambient_occlusion.texture_enabled)material["occlusionTexture"]={{"index",texture(*p.ambient_occlusion.texture,directory)}};
      j["materials"].push_back(std::move(material));
      if(!m->features.pbr.enabled)diagnostics.push_back("Material '"+string(m->name)+"': legacy shading converted to ufbx PBR approximation; exact original retained.");
    }
    if(!options.materials)diagnostics.push_back("Materials explicitly disabled; raw original materials and images remain retained.");
  }
  uint32_t texture_uv(const ufbx_texture &texture,const ufbx_mesh &mesh) {
    const auto name=string(texture.uv_set);
    if(!mesh.vertex_uv.exists)
      throw std::runtime_error("Native textured primitive has no UV coordinates");
    // OBJ has one unnamed coordinate stream. FBX names are actual identities,
    // including UVMap: it is not necessarily the first set on every mesh.
    if(name.empty()||(capture.source().format==ImportSourceFormat::Obj&&
        (name=="default"||name=="UVMap")))return 0;
    std::optional<uint32_t> selected;
    for(size_t i=0;i<mesh.uv_sets.count;++i) {
      builder.stop();
      if(string(mesh.uv_sets[i].name)!=name)continue;
      if(selected)throw std::runtime_error("Native texture UV set name is ambiguous on its primitive");
      if(i>1||!mesh.uv_sets[i].vertex_uv.exists)
        throw std::runtime_error("Native texture UV set exceeds supported coordinate streams");
      selected=uint32_t(i);
    }
    if(!selected)throw std::runtime_error("Native texture UV set is missing from its primitive");
    return *selected;
  }
  uint32_t material_for(const ufbx_material &source_material,const ufbx_mesh &mesh) {
    const auto base=source_material.typed_id+1;
    if(!options.materials)return base;
    const auto &p=source_material.pbr;
    const std::array<const ufbx_material_map*,4> maps={&p.base_color,&p.emission_color,&p.normal_map,&p.ambient_occlusion};
    const std::array<const char*,4> roles={"baseColorTexture","emissiveTexture","normalTexture","occlusionTexture"};
    std::array<int,4> coordinates{-1,-1,-1,-1};bool variant=false;
    for(size_t i=0;i<maps.size();++i)if(maps[i]->texture_enabled&&maps[i]->texture) {
      coordinates[i]=int(texture_uv(*maps[i]->texture,mesh));
      variant|=coordinates[i]!=0;
    }
    if(!variant)return base;
    const auto key=std::pair{base,coordinates};
    if(auto found=material_variants.find(key);found!=material_variants.end())return found->second;
    auto &materials=builder.json["materials"];
    if(materials.size()>=256)throw std::length_error("Native per-primitive material UV bindings exceed256 materials");
    auto material=materials.at(base);auto bindings=nlohmann::json::array();
    for(size_t i=0;i<maps.size();++i)if(coordinates[i]>=0) {
      auto &info=i==0?material["pbrMetallicRoughness"][roles[i]]:material[roles[i]];
      info["texCoord"]=coordinates[i];
      bindings.push_back({{"role",roles[i]},{"source_texture",maps[i]->texture->element_id},
        {"source_uv_set",string(maps[i]->texture->uv_set)},{"texcoord",coordinates[i]}});
    }
    material["extras"]["dome_source_ufbx_uv_bindings"]=std::move(bindings);
    const auto index=uint32_t(materials.size());materials.push_back(std::move(material));
    material_variants.emplace(key,index);return index;
  }
  struct SkinData {
    uint32_t index=0;
    size_t sets=0;
    std::optional<uint32_t> fallback_node;
    std::vector<std::vector<std::pair<uint16_t,float>>> weights;
  };
  std::map<uint32_t,SkinData> skins;
  const SkinData *skin(const ufbx_node &node) {
    const auto &mesh=*node.mesh;
    if(!mesh.skin_deformers.count)return nullptr;
    if(auto found=skins.find(node.typed_id);found!=skins.end())return &found->second;
    if(mesh.skin_deformers.count!=1)throw std::runtime_error("Layered FBX skin deformers require explicit composition rather than dropping a deformer");
    const auto &source_skin=*mesh.skin_deformers[0];
    if(source_skin.skinning_method!=UFBX_SKINNING_METHOD_LINEAR&&source_skin.skinning_method!=UFBX_SKINNING_METHOD_RIGID)
      throw std::runtime_error("FBX dual-quaternion skinning requires a native deformation adapter");
    if(source_skin.clusters.count==0||source_skin.clusters.count>256||source_skin.vertices.count!=mesh.num_vertices||mesh.num_vertices>builder.limits.max_vertices||source_skin.weights.count>8'000'000)
      throw std::length_error("Native FBX skin dimensions exceed bounded authoring support");
    auto &j=builder.json;
    std::vector<float> inverse_bind;inverse_bind.reserve(source_skin.clusters.count*16+16);
    nlohmann::json joints=nlohmann::json::array();std::set<uint32_t> identities;
    for(auto *cluster:source_skin.clusters){
      builder.stop();if(!cluster->bone_node||cluster->bone_node->typed_id>=source.nodes.count)
        throw std::runtime_error("FBX skin has a missing source joint identity");
      auto bone=cluster->bone_node->typed_id,joint=bone;
      if(!identities.insert(bone).second){
        // FBX permits separate affine clusters driven by one bone. Distinct
        // identity children preserve both binds and their common pose driver;
        // they are explicitly labelled conversion nodes, not authored bones.
        if(j["nodes"].size()>=8192)throw std::length_error("Native FBX bind helpers exceed node budget");
        joint=uint32_t(j["nodes"].size());j["nodes"].push_back({{"name",string(cluster->bone_node->name)+" / bind (adapter)"},
          {"extras",{{"dome_source_ufbx_cluster",cluster->element_id},{"dome_source_ufbx_bone",bone}}}});
        joint_helpers[bone].push_back(joint);
      }
      joints.push_back(joint);
      const auto &m=cluster->geometry_to_bone;
      // Uniform unit conversion conjugates the complete authored affine bind.
      // Do not reconstruct it from a bone's current/default transform.
      for(size_t col=0;col<4;++col){for(size_t row=0;row<3;++row)inverse_bind.push_back(real(m.cols[col].v[row]*(col==3?coordinate_scale:1)));inverse_bind.push_back(col==3?1.f:0.f);}
    }
    SkinData result;result.weights.resize(mesh.num_vertices);bool fallback=false;
    for(size_t vertex=0;vertex<mesh.num_vertices;++vertex){
      builder.stop();const auto &v=source_skin.vertices[vertex];
      if(v.weight_begin>source_skin.weights.count||v.num_weights>source_skin.weights.count-v.weight_begin||v.num_weights>source_skin.clusters.count)
        throw std::length_error("Native FBX skin vertex has invalid influence dimensions");
      if(v.dq_weight!=0)throw std::runtime_error("FBX vertex uses dual-quaternion blending");
      if(v.num_weights>builder.limits.max_metadata_visits||deformation_work>builder.limits.max_metadata_visits-v.num_weights)
        throw std::length_error("Native FBX deformation work budget exceeded");
      deformation_work+=v.num_weights;double sum=0;std::set<uint32_t> used;
      for(size_t k=0;k<v.num_weights;++k){const auto &w=source_skin.weights[v.weight_begin+k];
        if(w.cluster_index>=source_skin.clusters.count||!std::isfinite(w.weight)||w.weight<0||!used.insert(w.cluster_index).second)
          throw std::runtime_error("Native FBX skin has invalid/repeated influence authority");
        sum+=w.weight;
      }
      if(!std::isfinite(sum))throw std::runtime_error("Native FBX influence sum is not finite");
      if(sum==0){fallback=true;continue;}
      // Match the pinned evaluator's near-zero matrix convention explicitly.
      if(sum<=std::sqrt(std::numeric_limits<ufbx_real>::min()))
        throw std::runtime_error("FBX influence sum is below the evaluator's valid matrix range");
      for(size_t k=0;k<v.num_weights;++k){const auto &w=source_skin.weights[v.weight_begin+k];if(w.weight==0)continue;
        float value=real(w.weight/sum);if(value==0)throw std::runtime_error("FBX influence cannot be preserved in canonical float precision");
        result.weights[vertex].push_back({uint16_t(w.cluster_index),value});
      }
      if(result.weights[vertex].size()>32)throw std::length_error("Native FBX skin vertex exceeds32 positive influences");
      if(source_skin.skinning_method==UFBX_SKINNING_METHOD_RIGID&&result.weights[vertex].size()!=1)
        throw std::runtime_error("FBX rigid skin vertex has multiple positive influences");
      result.sets=std::max(result.sets,(result.weights[vertex].size()+3)/4);
    }
    if(fallback){
      if(joints.size()>=256)throw std::length_error("FBX unweighted geometry anchor exceeds256 native joints");
      // FBX's unweighted vertices follow geometry_to_world. An explicitly
      // labelled child anchor expresses that exact rule in standard glTF.
      if(j["nodes"].size()>=8192)throw std::length_error("Native FBX geometry anchor exceeds node budget");
      uint16_t slot=uint16_t(joints.size());uint32_t anchor=uint32_t(j["nodes"].size());
      j["nodes"].push_back({{"name","Unweighted FBX geometry (adapter)"},{"extras",{{"dome_source_ufbx_geometry_anchor",node.typed_id}}}});
      joints.push_back(anchor);result.fallback_node=anchor;
      inverse_bind.insert(inverse_bind.end(),{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1});
      for(auto &w:result.weights)if(w.empty())w.push_back({slot,1});
      result.sets=std::max(result.sets,size_t(1));
    }
    if(!j.contains("skins"))j["skins"]=nlohmann::json::array();
    result.index=uint32_t(j["skins"].size());
    j["skins"].push_back({{"name",string(source_skin.name)},{"joints",std::move(joints)},{"inverseBindMatrices",builder.floats(inverse_bind,16)},
      {"extras",identity(source_skin.element)}});
    deformers_converted=true;return &skins.emplace(node.typed_id,std::move(result)).first->second;
  }
  struct MorphData {const ufbx_blend_shape *shape;float weight;std::string name;nlohmann::json source;};
  std::vector<float> morph_normals(const ufbx_mesh &mesh,const ufbx_blend_shape &shape,
      std::span<const uint32_t> vertices,std::span<const float> base){
    const bool smooth=options.normals==ImportNormals::GenerateSmooth;
    auto charge=[&](size_t count){
      if(count>builder.limits.max_metadata_visits-std::min(deformation_work,builder.limits.max_metadata_visits))
        throw std::length_error("Generated FBX morph normals exceed deformation work budget");
      deformation_work+=count;
    };
    charge(mesh.num_vertices);charge(vertices.size());
    if(smooth){charge(mesh.num_vertices);charge(mesh.num_indices);charge(mesh.num_indices);}
    const size_t fields=smooth?2:1;
    const auto bytes=builder.limits.max_resource_bytes;
    if(mesh.num_vertices>bytes/(sizeof(ufbx_vec3)*fields))
      throw std::length_error("Generated FBX morph normals exceed temporary byte budget");
    const size_t geometry_bytes=mesh.num_vertices*sizeof(ufbx_vec3)*fields;
    if(vertices.size()>(bytes-geometry_bytes)/(3*sizeof(float)))
      throw std::length_error("Generated FBX morph normals exceed temporary byte budget");
    std::vector<ufbx_vec3> positions;positions.reserve(mesh.num_vertices);
    for(size_t v=0;v<mesh.num_vertices;++v){
      builder.stop();auto p=mesh.vertices[v];auto offset=ufbx_get_blend_shape_offset_index(&shape,v);
      if(offset!=UFBX_NO_INDEX){
        const auto delta=shape.position_offsets[offset];
        const double weight=offset<shape.offset_weights.count?shape.offset_weights[offset]:1;
        p.x+=delta.x*weight;p.y+=delta.y*weight;p.z+=delta.z*weight;
      }
      if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(p.z))
        throw std::runtime_error("Generated FBX morph positions are not finite");
      positions.push_back(p);
    }
    std::vector<ufbx_vec3> normals;
    if(smooth){
      normals.resize(mesh.num_vertices);
      auto coordinates=mesh.vertex_position;
      coordinates.values={positions.data(),positions.size()};coordinates.indices=mesh.vertex_indices;
      for(auto face:mesh.faces){
        builder.stop();auto n=ufbx_get_weighted_face_normal(&coordinates,face);
        for(uint32_t c=0;c<face.num_indices;++c){
          if((c&4095)==0)builder.stop();auto &sum=normals.at(mesh.vertex_indices[face.index_begin+c]);
          sum.x+=n.x;sum.y+=n.y;sum.z+=n.z;
        }
      }
      // Isolated source vertices may be unused by this material part. Only
      // normalize emitted vertices; a genuinely cancelled surface normal refuses.
    }
    if(vertices.size()%3||base.size()!=vertices.size()*3)
      throw std::logic_error("Generated FBX morph basis lost its ordered triangles");
    std::vector<float> result;result.reserve(base.size());
    for(size_t t=0;t<vertices.size();t+=3){
      builder.stop();ufbx_vec3 flat{};
      if(!smooth){const auto a=positions.at(vertices[t]),b=positions.at(vertices[t+1]),c=positions.at(vertices[t+2]);
        flat=normalize3(cross3(subtract(b,a),subtract(c,a)));}
      for(size_t k=0;k<3;++k){
        const auto n=smooth?normalize3(normals.at(vertices[t+k])):flat;
        result.insert(result.end(),{real(n.x-base[(t+k)*3]),real(n.y-base[(t+k)*3+1]),real(n.z-base[(t+k)*3+2])});
      }
    }
    generated_morph_normals=true;return result;
  }
  std::vector<MorphData> morphs(const ufbx_mesh &mesh){
    std::vector<MorphData> result;
    for(auto *deformer:mesh.blend_deformers)for(auto *channel:deformer->channels){
      builder.stop();if(channel->keyframes.count==0)throw std::runtime_error("FBX blend channel has no editable target shape");
      for(size_t k=0;k<channel->keyframes.count;++k){const auto &key=channel->keyframes[k];
        if(!key.shape||result.size()>=128||key.shape->position_offsets.count!=key.shape->num_offsets||
            key.shape->offset_vertices.count!=key.shape->num_offsets||
            (key.shape->normal_offsets.count&&key.shape->normal_offsets.count!=key.shape->num_offsets))
          throw std::runtime_error("FBX morph shape dimensions exceed native support");
        auto name=string(channel->name);if(channel->keyframes.count>1)name+=" / key "+std::to_string(k);
        auto provenance=identity(channel->element);provenance["dome_source_ufbx_shape"]=key.shape->element_id;
        provenance["dome_source_ufbx_key"]=k;provenance["dome_source_ufbx_target_weight"]=real(key.target_weight);
        result.push_back({key.shape,real(key.effective_weight),std::move(name),std::move(provenance)});
      }
    }
    if(!result.empty())deformers_converted=true;return result;
  }
  uint32_t mesh(const ufbx_node &node) {
    auto &m=*node.mesh;
    if(m.cache_deformers.count)throw std::runtime_error("FBX geometry caches require an explicit native authoring adapter");
    if(m.num_point_faces || m.num_line_faces || m.num_empty_faces || m.color_sets.count>1 || m.uv_sets.count>2)
      throw std::runtime_error("Native non-triangle attributes exceed current authoring channel support");
    if(m.num_triangles>builder.limits.max_triangles-triangles || m.num_indices>builder.limits.max_vertices || m.num_vertices>builder.limits.max_vertices ||
        m.max_face_triangles>builder.limits.max_triangles)
      throw std::length_error("Native geometry budget exceeded");
    if(!m.vertex_normal.exists && options.normals==ImportNormals::Preserve)
      diagnostics.push_back("Mesh '"+string(m.name)+"': missing normals generated flat; original absence retained in raw source.");
    std::vector<ufbx_vec3> smooth;
    if(options.normals==ImportNormals::GenerateSmooth){
      smooth.resize(m.num_vertices);
      for(auto face:m.faces){
        builder.stop();auto weighted=ufbx_get_weighted_face_normal(&m.vertex_position,face);
        for(uint32_t i=0;i<face.num_indices;++i){if((i&4095)==0)builder.stop();auto vertex=m.vertex_indices[face.index_begin+i];auto &n=smooth.at(vertex);n.x+=weighted.x;n.y+=weighted.y;n.z+=weighted.z;}
      }
      for(auto &n:smooth){builder.stop();if(dot3(n,n)>0)n=normalize3(n);}
      diagnostics.push_back("Mesh '"+string(m.name)+"': explicit area-weighted smooth normals generated across shared control points.");
    }
    const auto *binding=skin(node);auto shapes=morphs(m);
    std::vector<uint32_t> tri(m.max_face_triangles*3);
    struct Attributes {std::vector<float> position,normal,uv,uv1,color,tangent;std::vector<uint32_t> source_vertex,source_corner;nlohmann::json polygons=nlohmann::json::array();};
    std::map<uint32_t,Attributes> parts;
    for(size_t f=0;f<m.faces.count;++f) {
      builder.stop();auto face=m.faces[f];
      const size_t budget=std::min(builder.limits.max_metadata_visits,size_t(50'000'000));
      if(!face.num_indices || face.num_indices>budget/face.num_indices ||
          size_t(face.num_indices)*face.num_indices>budget-triangulation_work)
        throw std::length_error("Native polygon triangulation exceeds bounded work budget");
      triangulation_work+=size_t(face.num_indices)*face.num_indices;
      if(m.face_hole.count&&m.face_hole[f])throw std::runtime_error("Native polygon holes require explicit topology conversion");
      size_t count=ufbx_triangulate_face(tri.data(),tri.size(),&m,face);
      if(count!=face.num_indices-2)throw std::runtime_error("Native polygon triangulation failed");
      triangles+=count;
      uint32_t slot=m.face_material.count?m.face_material[f]:0;
      if(node.materials.count && slot>=node.materials.count)throw std::runtime_error("Native face material slot is outside instance binding");
      uint32_t material=node.materials.count?material_for(*node.materials[slot],m):0;
      const auto &material_json=builder.json["materials"].at(material);
      const uint32_t normal_uv=material_json.contains("normalTexture")?
        material_json["normalTexture"].value("texCoord",0u):0u;
      const auto &tangent=normal_uv==0?m.vertex_tangent:m.uv_sets[normal_uv].vertex_tangent;
      const auto &bitangent=normal_uv==0?m.vertex_bitangent:m.uv_sets[normal_uv].vertex_bitangent;
      auto &a=parts[material];const size_t first_vertex=a.source_vertex.size();
      for(size_t t=0;t<count;++t) {
        if((t&4095)==0)builder.stop();
        auto p0=ufbx_get_vertex_vec3(&m.vertex_position,tri[t*3]),p1=ufbx_get_vertex_vec3(&m.vertex_position,tri[t*3+1]),p2=ufbx_get_vertex_vec3(&m.vertex_position,tri[t*3+2]);
        auto flat=normalize3(cross3(subtract(p1,p0),subtract(p2,p0)));
        for(size_t k=0;k<3;++k) {
          uint32_t corner=tri[t*3+k];
          if(++vertices>builder.limits.max_vertices)throw std::length_error("Native expanded vertex budget exceeded");
          auto p=ufbx_get_vertex_vec3(&m.vertex_position,corner);
          auto n=options.normals==ImportNormals::GenerateSmooth?smooth.at(m.vertex_indices[corner]):
              options.normals==ImportNormals::GenerateFlat||!m.vertex_normal.exists?flat:ufbx_get_vertex_vec3(&m.vertex_normal,corner);
          if(dot3(n,n)==0)throw std::runtime_error("Explicit smooth normals cancel at a source vertex");
          a.position.insert(a.position.end(),{real(p.x*coordinate_scale),real(p.y*coordinate_scale),real(p.z*coordinate_scale)});a.normal.insert(a.normal.end(),{real(n.x),real(n.y),real(n.z)});
          if(m.vertex_uv.exists){auto uv=ufbx_get_vertex_vec2(&m.vertex_uv,corner);a.uv.insert(a.uv.end(),{real(uv.x),real(uv.y)});}
          if(m.uv_sets.count>1){auto uv=ufbx_get_vertex_vec2(&m.uv_sets[1].vertex_uv,corner);a.uv1.insert(a.uv1.end(),{real(uv.x),real(uv.y)});}
          if(m.vertex_color.exists){auto c=ufbx_get_vertex_vec4(&m.vertex_color,corner);a.color.insert(a.color.end(),{real(c.x),real(c.y),real(c.z),real(c.w)});}
          if(tangent.exists && options.normals==ImportNormals::Preserve && m.vertex_normal.exists) {
            if(!bitangent.exists)throw std::runtime_error("Native authored tangent has no bitangent handedness authority");
            auto t=ufbx_get_vertex_vec3(&tangent,corner),b=ufbx_get_vertex_vec3(&bitangent,corner);
            auto sign=dot3(cross3(n,t),b)<0?-1.f:1.f;
            a.tangent.insert(a.tangent.end(),{real(t.x),real(t.y),real(t.z),sign});
          }
          a.source_vertex.push_back(m.vertex_indices[corner]);a.source_corner.push_back(corner);
        }
      }
      std::map<uint32_t,uint32_t> corners;
      for(size_t v=first_vertex;v<a.source_corner.size();++v)corners.try_emplace(a.source_corner[v],uint32_t(v));
      std::vector<uint32_t> loop;loop.reserve(face.num_indices);
      for(uint32_t c=0;c<face.num_indices;++c){auto found=corners.find(face.index_begin+c);
        if(found==corners.end())throw std::runtime_error("Native triangulation omitted an original face corner");
        loop.push_back(found->second);}
      a.polygons.push_back({{"source_face",f},{"vertices",std::move(loop)},{"first_triangle",first_vertex/3},{"triangle_count",count}});
    }
    nlohmann::json primitives=nlohmann::json::array();
    for(auto &[material,a]:parts) {
      nlohmann::json attributes={{"POSITION",builder.floats(a.position,3,true)},{"NORMAL",builder.floats(a.normal,3)}};
      if(!a.uv.empty())attributes["TEXCOORD_0"]=builder.floats(a.uv,2);
      if(!a.uv1.empty())attributes["TEXCOORD_1"]=builder.floats(a.uv1,2);
      if(!a.color.empty())attributes["COLOR_0"]=builder.floats(a.color,4);
      if(!a.tangent.empty())attributes["TANGENT"]=builder.floats(a.tangent,4);
      if(binding)for(size_t set=0;set<binding->sets;++set){
        if(a.source_vertex.size()>builder.limits.max_file_bytes/24)throw std::length_error("Native FBX skin attribute byte budget exceeded");
        std::vector<uint16_t> joint;std::vector<float> weight;joint.reserve(a.source_vertex.size()*4);weight.reserve(a.source_vertex.size()*4);
        for(auto v:a.source_vertex){builder.stop();const auto &w=binding->weights.at(v);for(size_t k=0;k<4;++k){size_t i=set*4+k;joint.push_back(i<w.size()?w[i].first:0);weight.push_back(i<w.size()?w[i].second:0);}}
        attributes["JOINTS_"+std::to_string(set)]=builder.joints(joint);attributes["WEIGHTS_"+std::to_string(set)]=builder.floats(weight,4);
      }
      // Exact canonical vertex -> pinned parser control-point/corner identities.
      auto provenance=identity(m.element);provenance["dome_source_vertices"]=a.source_vertex;provenance["dome_source_corners"]=a.source_corner;
      provenance["dome_source_polygons_version"]=1;provenance["dome_source_polygons"]=std::move(a.polygons);
      provenance["dome_normals_generated"]=!m.vertex_normal.exists || options.normals!=ImportNormals::Preserve;
      nlohmann::json primitive={{"attributes",std::move(attributes)},{"material",material},{"mode",4},{"extras",std::move(provenance)}};
      if(!shapes.empty()){
        if(a.source_vertex.size()>builder.limits.max_metadata_visits/shapes.size()||deformation_work>builder.limits.max_metadata_visits-a.source_vertex.size()*shapes.size())
          throw std::length_error("Native FBX morph expansion work budget exceeded");
        deformation_work+=a.source_vertex.size()*shapes.size();primitive["targets"]=nlohmann::json::array();
        for(const auto &shape:shapes){
          std::vector<float> position,normal;position.reserve(a.source_vertex.size()*3);
          const bool generated=options.normals!=ImportNormals::Preserve;
          if(generated)normal=morph_normals(m,*shape.shape,a.source_vertex,a.normal);
          else if(shape.shape->normal_offsets.count)normal.reserve(a.source_vertex.size()*3);
          for(auto v:a.source_vertex){builder.stop();auto offset=ufbx_get_blend_shape_offset_index(shape.shape,v);
            auto p=offset==UFBX_NO_INDEX?ufbx_vec3{}:shape.shape->position_offsets[offset];
            double influence=offset!=UFBX_NO_INDEX&&offset<shape.shape->offset_weights.count?shape.shape->offset_weights[offset]:1;
            position.insert(position.end(),{real(p.x*coordinate_scale*influence),real(p.y*coordinate_scale*influence),real(p.z*coordinate_scale*influence)});
            if(!generated&&shape.shape->normal_offsets.count){auto n=offset==UFBX_NO_INDEX?ufbx_vec3{}:shape.shape->normal_offsets[offset];normal.insert(normal.end(),{real(n.x*influence),real(n.y*influence),real(n.z*influence)});}
          }
          nlohmann::json target={{"POSITION",builder.floats(position,3,true)}};if(!normal.empty())target["NORMAL"]=builder.floats(normal,3);primitive["targets"].push_back(std::move(target));
        }
      }
      primitives.push_back(std::move(primitive));
    }
    auto &j=builder.json;auto id=j["meshes"].size();nlohmann::json mesh={{"name",string(m.name)},{"primitives",std::move(primitives)},{"extras",identity(m.element)}};
    if(!shapes.empty()){mesh["weights"]=nlohmann::json::array();mesh["extras"]["targetNames"]=nlohmann::json::array();mesh["extras"]["dome_source_ufbx_morphs"]=nlohmann::json::array();
      for(const auto &shape:shapes){mesh["weights"].push_back(shape.weight);mesh["extras"]["targetNames"].push_back(shape.name);mesh["extras"]["dome_source_ufbx_morphs"].push_back(shape.source);}}
    j["meshes"].push_back(std::move(mesh));return uint32_t(id);
  }
  std::shared_ptr<const std::vector<uint8_t>> convert() {
    auto &j=builder.json;
    if(source.nodes.count>4096 || source.materials.count>255 || source.elements.count>builder.limits.max_metadata_visits)
      throw std::length_error("Native source hierarchy/material metadata exceeds authoring bounds");
    if(source.constraints.count)throw std::runtime_error("Native constrained node evaluation requires an explicit authoring adapter");
    if(source.nurbs_surfaces.count || source.nurbs_curves.count || source.procedural_geometries.count)
      throw std::runtime_error("Native curved/procedural source geometry requires explicit tessellation options");
    if(source.anim_stacks.count)diagnostics.push_back("Animation tracks are retained in original bytes; playback is outside authoring import.");
    if(source.cameras.count || source.lights.count)diagnostics.push_back("Source cameras and lights retained; final rendering is outside authoring import.");
    retain_texture_sources();materials();j["nodes"]=nlohmann::json::array();j["meshes"]=nlohmann::json::array();
    for(size_t i=0;i<source.nodes.count;++i)j["nodes"].push_back(nullptr);
    std::map<std::vector<uint32_t>,uint32_t> shared_meshes;
    for(auto *n:source.nodes) {
      builder.stop();auto t=n->local_transform;
      nlohmann::json node={{"name",string(n->name)},{"translation",{real(t.translation.x*coordinate_scale),real(t.translation.y*coordinate_scale),real(t.translation.z*coordinate_scale)}},
          {"rotation",{real(t.rotation.x),real(t.rotation.y),real(t.rotation.z),real(t.rotation.w)}},
          {"scale",{real(t.scale.x),real(t.scale.y),real(t.scale.z)}},{"extras",identity(n->element)}};
      if(n->inherit_mode!=UFBX_INHERIT_MODE_NORMAL || (n->mesh&&n->has_geometry_transform))
        throw std::runtime_error("Native transform helper policy did not resolve geometric/inheritance authority");
      if(n->children.count){node["children"]=nlohmann::json::array();for(auto *child:n->children)node["children"].push_back(child->typed_id);}
      if(n->mesh) {
        if(auto binding=skin(*n)){node["skin"]=binding->index;if(binding->fallback_node)node["children"].push_back(*binding->fallback_node);}
        std::vector<uint32_t> key{n->mesh->typed_id};for(auto *m:n->materials)key.push_back(m->typed_id);
        auto found=shared_meshes.find(key);
        uint32_t id=found==shared_meshes.end()?mesh(*n):found->second;
        shared_meshes.emplace(std::move(key),id);node["mesh"]=id;
      }
      if(!n->visible)throw std::runtime_error("Native hidden-node state needs explicit authoring visibility adoption");
      j["nodes"].at(n->typed_id)=std::move(node);
    }
    for(const auto &[parent,children]:joint_helpers)for(auto child:children)j["nodes"].at(parent)["children"].push_back(child);
    if(!triangles)throw std::runtime_error("Native source has no triangle mesh");
    auto root=source.root_node->typed_id;
    if(root_rotation!=std::array<double,4>{0,0,0,1}){
      root=uint32_t(j["nodes"].size());j["nodes"].push_back({{"name","Input units / axes"},{"children",{source.root_node->typed_id}},
        {"rotation",root_rotation},{"extras",{{"dome_import_conversion",true}}}});
    }
    j["scenes"]=nlohmann::json::array({{{"nodes",{root}}}});j["scene"]=0;
    j["extras"]={{"dome_source_adapter","ufbx-0.23.1-native-4"},{"dome_canonical_position_units","metres"},{"dome_source_geometry_unit_scale",source.metadata.geometry_scale},{"dome_source_unit_override_scale",coordinate_scale}};
    return builder.finish();
  }
};
} // namespace
CapturedImportSource capture_native_source(const std::filesystem::path &path,ImportOptions options,ImportLimits limits) {
  options.validate();options_supported(options);
  ImportSourceCapture capture(path,options,limits);const auto &original=capture.source();
  if(original.format!=ImportSourceFormat::Obj&&original.format!=ImportSourceFormat::Fbx)
    throw std::invalid_argument("Native source adapter expects OBJ or FBX");
  detail::ObjLibraries libraries;
  if(original.format==ImportSourceFormat::Obj)libraries=detail::capture_obj_libraries(capture,limits);
  Reader reader{capture,limits,{}};ufbx_load_opts load{};
  load.temp_allocator.memory_limit=256*1024*1024;load.result_allocator.memory_limit=256*1024*1024;
  load.temp_allocator.allocation_limit=2'000'000;load.result_allocator.allocation_limit=2'000'000;
  load.strict=true;load.force_single_thread_ascii_parsing=true;
  load.file_format=original.format==ImportSourceFormat::Obj?UFBX_FILE_FORMAT_OBJ:UFBX_FILE_FORMAT_FBX;
  load.filename={original.root.name.data(),original.root.name.size()};
  load.open_file_cb.fn=Reader::open;load.open_file_cb.user=&reader;load.load_external_files=original.format==ImportSourceFormat::Fbx&&limits.external_resources;
  if(!libraries.canonical_mtl.empty())load.obj_mtl_data={libraries.canonical_mtl.data(),libraries.canonical_mtl.size()};
  load.progress_cb.fn=Reader::progress;load.progress_cb.user=&reader;load.progress_interval_hint=65536;
  load.geometry_transform_handling=UFBX_GEOMETRY_TRANSFORM_HANDLING_HELPER_NODES;
  load.inherit_mode_handling=UFBX_INHERIT_MODE_HANDLING_HELPER_NODES;
  if(original.format==ImportSourceFormat::Obj || options.axes==ImportAxes::FromFile)
    load.target_axes=ufbx_axes_right_handed_y_up;
  load.target_unit_meters=1;load.space_conversion=UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;
  load.obj_axes=options.axes==ImportAxes::ZUp?ufbx_axes_right_handed_z_up:ufbx_axes_right_handed_y_up;
  double unit=options.units==ImportUnits::Centimetres?.01:options.units==ImportUnits::Millimetres?.001:options.units==ImportUnits::Custom?options.custom_metres_per_unit:1;
  load.obj_unit_meters=unit;
  ufbx_error error{};auto &bytes=*original.root.bytes;
  std::unique_ptr<ufbx_scene,decltype(&ufbx_free_scene)> scene(ufbx_load_memory(bytes.data(),bytes.size(),&load,&error),ufbx_free_scene);
  if(reader.failure)std::rethrow_exception(reader.failure);
  if(!scene){char message[1024];ufbx_format_error(message,sizeof(message),&error);throw std::runtime_error(std::string("Native model import failed: ")+message);}
  if(scene->metadata.warnings.count)
    throw std::runtime_error("Native source requires an explicit repair: "+string(scene->metadata.warnings[0].description));
  Converter converter(capture,options,*scene,limits);
  converter.material_directories=std::move(libraries.material_directories);
  if(original.format==ImportSourceFormat::Fbx){
    if(options.units!=ImportUnits::FromFile){
      auto file_unit=scene->settings.unit_meters;
      if(!std::isfinite(file_unit)||file_unit<=0)throw std::runtime_error("FBX unit metadata is not a finite positive distance");
      converter.coordinate_scale=unit/file_unit;
      converter.diagnostics.push_back("FBX authored units explicitly overridden; original metadata retained.");
    }
    if(options.axes==ImportAxes::ZUp){const double s=std::sqrt(.5);converter.root_rotation={-s,0,0,s};}
    if(options.axes!=ImportAxes::FromFile)converter.diagnostics.push_back("FBX authored axes explicitly overridden; original metadata retained.");
  }
  if(original.format==ImportSourceFormat::Obj && options.units==ImportUnits::FromFile)
    converter.diagnostics.push_back("OBJ has no authored unit metadata; assumed one metre per unit (stored import choice).");
  auto result=converter.convert();
  // Every canonical accessor is validated by the independent existing importer
  // before the detached authoring Scene can adopt any object.
  auto validation=limits;if(converter.deformers_converted)validation.mode=ImportMode::PreserveGltf;
  (void)import_glb(*result,validation);
  if(converter.generated_morph_normals)
    converter.diagnostics.push_back("Explicit generated FBX morph normals preserve the selected flat/smooth basis for every target endpoint; original attributes remain retained.");
  return capture.finish(result,converter.generated_morph_normals?
    "ufbx v0.23.1 26a482ae native adapter 5 / generated morph normals":
    "ufbx v0.23.1 26a482ae native adapter 4",std::move(converter.diagnostics));
}
} // namespace dome
