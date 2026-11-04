// SPDX-License-Identifier: GPL-3.0-or-later
#include "native-authoring-compiler.hpp"
#include "dome/source-paint-bake.hpp"
#include "source-paint-export.hpp"
#include "source-paint-physical.hpp"
#include "emission-authority.hpp"
#include "gltf-document.hpp"
#include "raw-source-emission.hpp"
#include <set>
#include <algorithm>
#include <map>
#include <numeric>
namespace dome::detail {
NativeAuthoringCompilation compile_native_authoring(
    const NativeAuthoringCapture &capture, const SourceUvBakeOptions &policy,
    size_t output_limit, const std::function<bool()> &cancelled) {
  const auto stop = [first=cancelled,second=policy.cancelled] {
    return (first && first()) || (second && second());
  };
  auto check = [&] {
    if (stop()) throw std::runtime_error("Native authoring compilation cancelled");
  };
  check();
  if(!policy.max_working_bytes||policy.max_working_bytes>512*1024*1024)
    throw std::invalid_argument("Invalid native authoring working budget");
  size_t raw_proof_peak=0;
  size_t fixed = sizeof(Scene) + sizeof(NativeAuthoringCompilation);
  auto reserve = [&](size_t count, size_t unit) {
    if (fixed > policy.max_working_bytes ||
        count > (policy.max_working_bytes-fixed)/unit)
      throw std::length_error("Native authoring geometry staging exceeds working budget");
    fixed += count*unit;
  };
  reserve(capture.originals().size(),sizeof(ObjectSurface));
  reserve(capture.scene().objects().size(),sizeof(SculptObject));
  reserve(capture.scene().groups().size(),sizeof(SceneGroup));
  auto physical=[](const SculptObject &o){return hdr_emission(o.material.emissive)||
      (o.painted&&o.painted->has_physical_emission())||
      (o.material_bindings&&physical_emission(*o.material_bindings));};
  bool painted = false;
  for (const auto &surface : capture.originals()) {
    check();
    const auto &mesh = surface.mesh;
    reserve(mesh.positions.size()+mesh.normals.size(),sizeof(Vec3));
    reserve(mesh.uv.size(),sizeof(std::array<float,2>));
    reserve(mesh.indices.size(),sizeof(uint32_t));
    reserve(mesh.uv_pins.size()+mesh.uv_seams.size(),sizeof(uint8_t));
    reserve(surface.name.size()+1,sizeof(char));
    reserve(surface.groups.size(),sizeof(SceneGroup));
    for(const auto &group:surface.groups)reserve(group.name.size()+1,sizeof(char));
    const auto &object = capture.scene().object(surface.id);
    if(physical(object)&&(!object.painted||!object.painted->source_sampling_prepared())) {
      if(object.appearance){const auto scratch=raw_emission_proof_bytes(*object.appearance,stop);
        if(scratch>policy.max_working_bytes-fixed)throw std::length_error("Raw source material proof exceeds available working budget");
        raw_proof_peak=std::max(raw_proof_peak,scratch);}
      if(!raw_source_emission(capture,surface,stop))
        throw std::invalid_argument("Physical editable export requires its actual original-source sampling adapter");
      reserve(1,sizeof(TextureMaps)+64+256);
    }
    if (object.painted && object.painted->source_sampling_prepared()) {
      const auto &maps = surface.textures;
      if (!maps || !maps->overlay || !object.painted->valid_source_sampling_attachment())
        throw std::invalid_argument("Native authoring lost its original source paint owner");
      painted |= !maps->overlay->empty()||physical(object);
    }
  }
  reserve(raw_proof_peak,1); // Keep the single proof peak available during writer replay.
  NativeAuthoringCompilation result;
  if (!painted) {
    result.surfaces = capture.originals();
    for (auto &surface : result.surfaces) {
      check();
      const auto &object = capture.scene().object(surface.id);
      if (object.painted && object.painted->source_sampling_prepared())
        source_paint_export(*object.painted,surface,stop);
      else if(physical(object)) {
        if(!raw_source_emission(capture,surface,stop))
          throw std::invalid_argument("Raw emission lost its captured original sampling authority");
        auto maps=std::make_shared<TextureMaps>(*surface.textures);
        maps->overlay.reset();surface.textures=std::move(maps);
        result.diagnostics.push_back("Object "+std::to_string(object.id)+
          ": original raw emission images/samplers/coordinates retained; current radiance uses explicit factor and strength");
      }
    }
    return result;
  }
  // The capture already owns an immutable snapshot. A shallow Scene copy only
  // replaces completed paint/material pointers; it never edits borrowed meshes,
  // volumes, recipes or controls. Reserve vector/string/adoption staging first.
  for(const auto &group:capture.scene().groups())reserve(group.name.size()+1,sizeof(char));
  for (const auto &object : capture.scene().objects()) {
    reserve(object.name.size()+object.material.name.size()+2,sizeof(char));
    if (object.painted) {
      reserve(object.painted->layers().size(),sizeof(PaintLayer));
      for (const auto &layer : object.painted->layers()) {
        reserve(layer.name.size()+1,sizeof(char));
        reserve(layer.tiles.size()+layer.pbr_tiles.size()+layer.emission_tiles.size(),
                sizeof(std::shared_ptr<const PaintTile>));
      }
    }
  }
  auto staged = capture.scene();
  struct View {
    std::shared_ptr<const TextureMaps> textures;
    std::shared_ptr<const SourceMaterialBindings> material;
  };
  std::map<uint64_t,View> views;
  size_t retained_images = 0, retained_memory = 0, samples = 0, raster = 0;
  const size_t maximum_images = std::min(policy.max_output_bytes,output_limit);
  for (const auto &surface : capture.originals()) {
    check();
    const auto &object = capture.scene().object(surface.id);
    if (!object.painted || !object.painted->source_sampling_prepared() ||
        (surface.textures->overlay->empty()&&!physical(object))) continue;
    SourceUvBakeOptions bounded = policy;
    bounded.cancelled = stop;
    if (retained_images >= maximum_images || fixed >= policy.max_working_bytes ||
        retained_memory >= policy.max_working_bytes-fixed ||
        samples >= policy.max_sample_operations || raster >= policy.max_raster_samples)
      throw std::length_error("Native authoring cumulative material budget exhausted");
    bounded.max_output_bytes = maximum_images-retained_images;
    bounded.max_working_bytes = policy.max_working_bytes-fixed-retained_memory;
    bounded.max_sample_operations = policy.max_sample_operations-samples;
    bounded.max_raster_samples = policy.max_raster_samples-raster;
    SourceUvBakeInput input;
    input.paint = object.painted;
    input.appearance = object.appearance;
    input.current_material = object.material_bindings;
    input.current_deformation = object.deformation;
    input.target = object.painted->mesh();
    input.source_vertices.resize(input.target.positions.size());
    std::iota(input.source_vertices.begin(),input.source_vertices.end(),0);
    SourcePaintBakeResult compiled;
    std::optional<SourcePaintPhysicalResult> emission;
    if(physical(object)){
      emission=bake_source_paint_physical(input,object.material,bounded);
      compiled=std::move(emission->material);
    }else compiled=bake_source_paint(input,object.material,bounded);
    const auto &sampling = compiled.sampling;
    const auto available=policy.max_working_bytes-fixed-retained_memory;
    if (sampling.image_bytes > maximum_images-retained_images ||
        sampling.image_bytes > available || sampling.layer_bytes > available-sampling.image_bytes ||
        sampling.baked_bytes > available-sampling.image_bytes-sampling.layer_bytes ||
        sampling.sample_operations > policy.max_sample_operations-samples ||
        sampling.raster_samples > policy.max_raster_samples-raster)
      throw std::length_error("Native authoring material result exceeds cumulative budget");
    retained_images += sampling.image_bytes;
    retained_memory += sampling.image_bytes+sampling.layer_bytes+sampling.baked_bytes;
    samples += sampling.sample_operations;
    raster += sampling.raster_samples;
    auto adopted = adopt_source_uv_bake(sampling,stop);
    auto &next = staged.object(object.id);
    next.painted = std::move(adopted.paint);
    next.deformation = std::move(adopted.deformation);
    next.appearance = std::move(adopted.appearance);
    next.material_bindings = std::move(adopted.material_bindings);
    if(emission){
      next.material.emissive={};
      result.emission.push_back({object.id,std::move(emission->emission),emission->factor,
        emission->strength,emission->maximum_absolute_error,emission->maximum_relative_error,
        emission->erased_nonzero_samples,object.material.unlit,
        object.material.unlit?emission->nonzero_samples:0});
      result.diagnostics.push_back("Object "+std::to_string(object.id)+
        ": finite physical-emission PNG/strength view; maximum texel quantization error "+
        std::to_string(emission->maximum_absolute_error)+
        (object.material.unlit?"; unlit material ignores emission":" linear radiance units"));
    }
    views.emplace(object.id,View{std::move(compiled.textures),
                                 std::move(compiled.material_bindings)});
    result.diagnostics.push_back("Object "+std::to_string(object.id)+
        ": explicit finite PBR paint view, original editable source retained");
  }
  check();
  staged.validate();
  result.surfaces = staged.export_surfaces(stop);
  for (auto &surface : result.surfaces) {
    check();
    if (auto view=views.find(surface.id);view!=views.end()) {
      surface.textures = view->second.textures;
      surface.material_bindings = view->second.material;
    }
  }
  if (result.surfaces.size()!=capture.ids().size())
    throw std::invalid_argument("Native authoring compilation changed emitted object membership");
  for(size_t i=0;i<result.surfaces.size();++i)
    if(result.surfaces[i].id!=capture.ids()[i])
      throw std::invalid_argument("Native authoring compilation changed emitted object ordering");
  return result;
}
void finish_native_authoring_emission(GltfDocument &document,
    const NativeAuthoringCompilation &compiled,size_t maximum,
    const std::function<bool()> &cancelled) {
  using Json=nlohmann::json;
  auto stop=[&]{if(cancelled&&cancelled())throw std::runtime_error("Physical authoring finish cancelled");};
  auto &j=document.json;auto &binary=document.binary;std::set<uint64_t> identities;
  for(const auto &entry:compiled.emission){
    stop();
    for(const char *table:{"meshes","materials","bufferViews","images","samplers","textures"})
      if(j.contains(table)&&(!j.at(table).is_array()||j.at(table).size()>=65536))
        throw std::length_error("Physical portable resource table exceeds limits");
    const auto found=std::find_if(compiled.surfaces.begin(),compiled.surfaces.end(),
      [&](const auto &surface){return surface.id==entry.object;});
    if(found==compiled.surfaces.end()||!identities.insert(entry.object).second)
      throw std::logic_error("Physical compiler lost its exact object membership");
    const size_t index=size_t(found-compiled.surfaces.begin());
    auto &identity=document.objects.at(index);
    if(identity.node<0||identity.mesh<0||identity.material<0||
       j.at("nodes").at(identity.node).at("mesh")!=identity.mesh)
      throw std::logic_error("Physical compiler object mapping is not its emitted node");
    // A future mesh/material deduplicator must not let this object's field
    // overwrite another object's emission. Rebind an actual unique copy.
    size_t mesh_users=0;for(const auto &node:j.at("nodes"))
      if(node.contains("mesh")&&node.at("mesh")==identity.mesh)++mesh_users;
    if(mesh_users>1){auto mesh=j.at("meshes").at(identity.mesh);identity.mesh=int(j.at("meshes").size());
      j["meshes"].push_back(std::move(mesh));j["nodes"][identity.node]["mesh"]=identity.mesh;}
    auto &primitives=j.at("meshes").at(identity.mesh).at("primitives");
    if(primitives.size()!=1||primitives.at(0).at("material")!=identity.material||
       !primitives.at(0).at("attributes").contains("TEXCOORD_0"))
      throw std::logic_error("Physical compiler requires its exact UV0 primitive mapping");
    size_t material_users=0;for(const auto &mesh:j.at("meshes"))for(const auto &primitive:mesh.at("primitives"))
      if(primitive.contains("material")&&primitive.at("material")==identity.material)++material_users;
    if(material_users>1){auto material=j.at("materials").at(identity.material);identity.material=int(j.at("materials").size());
      j["materials"].push_back(std::move(material));primitives[0]["material"]=identity.material;}
    if(entry.image.rgba.size()>maximum||binary.size()>maximum-entry.image.rgba.size())
      throw std::length_error("Physical portable image exceeds container budget before encoding");
    const auto png=entry.image.png();stop();const size_t padding=(4-binary.size()%4)%4;
    if(binary.size()>maximum||padding>maximum-binary.size()||png.size()>maximum-binary.size()-padding)
      throw std::length_error("Physical portable PNG exceeds output budget");
    binary.insert(binary.end(),padding,0);const size_t offset=binary.size();binary.insert(binary.end(),png.begin(),png.end());
    const size_t view=j["bufferViews"].size();j["bufferViews"].push_back({{"buffer",0},{"byteOffset",offset},{"byteLength",png.size()}});
    const size_t image=j["images"].size();j["images"].push_back({{"bufferView",view},{"mimeType","image/png"}});
    const size_t sampler=j["samplers"].size();j["samplers"].push_back({{"wrapS",33071},{"wrapT",33071},{"minFilter",9729},{"magFilter",9729}});
    const size_t texture=j["textures"].size();j["textures"].push_back({{"source",image},{"sampler",sampler}});
    auto &material=j["materials"].at(identity.material);
    const bool unlit=material.contains("extensions")&&material.at("extensions").contains("KHR_materials_unlit");
    if(unlit!=entry.unlit)throw std::logic_error("Physical finite view changed the actual unlit interpretation");
    material["emissiveTexture"]={{"index",texture}};material["emissiveFactor"]=entry.factor;
    if(entry.strength>1){material["extensions"]["KHR_materials_emissive_strength"]={{"emissiveStrength",entry.strength}};
      auto &used=j["extensionsUsed"];if(used.is_null())used=Json::array();
      if(std::find(used.begin(),used.end(),"KHR_materials_emissive_strength")==used.end())used.push_back("KHR_materials_emissive_strength");}
    else if(material.contains("extensions"))material["extensions"].erase("KHR_materials_emissive_strength");
    material["extras"]["domeSculptPortableEmission"]={{"version",1},
      {"evaluation","finite-current-core-pbr-uv0"},{"units","native-linear-radiance"},
      {"lossless",false},{"unlitIgnoresEmission",entry.unlit},
      {"errorScope","encoded-texel-quantization"},{"betweenTexelErrorBound",nullptr},
      {"maximumAbsoluteTexelQuantizationError",entry.maximum_absolute_error},
      {"maximumRelativeTexelQuantizationError",entry.maximum_relative_error},
      {"erasedNonzeroTexelSamples",entry.erased_nonzero_samples},{"clampedTexelSamples",0},
      {"unlitIgnoredNonzeroTexelSamples",entry.unlit_ignored_samples},
      {"maximumUnlitIgnoredRadiance",entry.unlit?double(entry.strength)*std::max({entry.factor[0],entry.factor[1],entry.factor[2]}):0.}};
  }
  stop();
}
} // namespace dome::detail
