// SPDX-License-Identifier: GPL-3.0-or-later
#include "native-authoring-compiler.hpp"
#include "dome/source-paint-bake.hpp"
#include "source-paint-export.hpp"
#include "source-paint-physical.hpp"
#include "emission-authority.hpp"
#include "gltf-document.hpp"
#include "raw-source-emission.hpp"
#include "emission-paint-access.hpp"
#include "paint-overlay-sample.hpp"
#include "dome/color-paint-sampling.hpp"
#include <cmath>
#include <set>
#include <algorithm>
#include <map>
#include <numeric>
namespace dome::detail {
namespace {
bool physical_object(const SculptObject &o) {
  return hdr_emission(o.material.emissive)||(o.painted&&o.painted->has_physical_emission())||
    (o.material_bindings&&physical_emission(*o.material_bindings));
}
bool native_physical(const SculptObject &o) {
  return physical_object(o)&&(!o.mesh_active||(o.painted&&
    !o.painted->source_sampling_prepared()&&
    o.painted->surface_diagnostics().policy!=PaintUvPolicy::SourceAuthored));
}
// Compilation changes only this temporary view's current emission. Complete
// original float planes and every historical material remain in the capture.
void omit_current_emission(SculptObject &o) {
  if(o.painted&&o.painted->has_physical_emission())
    o.painted=EmissionExportAccess::without_emission(*o.painted);
  o.material.emissive={};
  if(o.material_bindings){auto binding=std::make_shared<SourceMaterialBindings>(*o.material_bindings);
    binding->physical_emission.reset();binding->native_reference.emission={};
    o.material_bindings=std::move(binding);}
}
struct NativePhysicalField {
  NativeAuthoringEmission emission;
  size_t images=0,memory=0,samples=0,raster=0;
};
NativePhysicalField native_physical_field(const SculptObject &o,
    const SourceUvBakeOptions &policy,const std::function<bool()> &cancelled) {
  auto stop=[&]{if(cancelled&&cancelled())throw std::runtime_error("Native physical compilation cancelled");};
  stop();NativePhysicalField out;out.emission.object=o.id;out.emission.unlit=o.material.unlit;
  auto background=o.material.emissive;
  if(o.material_bindings&&o.material_bindings->physical_emission)
    background=*o.material_bindings->physical_emission;
  if(!o.mesh_active){
    out.emission.strength=std::max({1.f,background[0],background[1],background[2]});
    for(size_t k=0;k<3;++k)out.emission.factor[k]=background[k]/out.emission.strength;
    return out;
  }
  if(!o.painted)throw std::invalid_argument("Native physical view requires its actual paint owner");
  const auto &paint=*o.painted;
  if(paint.paint_sampling_domain()) {
    const auto colors=o.material_bindings?o.material_bindings->color_preparation:nullptr;
    if(!paint.valid_paint_sampling_attachment()||!colors||!colors->paint_sampling||
       !colors->paint_sampling->matches_current(paint))
      throw std::invalid_argument("Native physical paint lost its independently authenticated sampling domain");
    colors->paint_sampling->validate_current(paint.mesh(),cancelled);
  }
  const auto maps=paint.source_maps();
  // A retained paint basis is not an original-image UV certificate. Its source
  // emission image needs an actual face/barycentric adapter before admission.
  if(maps&&maps->emission.image&&(paint.paint_sampling_domain()||maps->emission.texcoord!=0))
    throw std::invalid_argument("Native physical source emission requires its actual paint/source coordinate adapter");
  const uint32_t r=paint.resolution();const size_t pixels=size_t(r)*r;
  // Four composed texels, five independent RGBA planes, then six output
  // radiance/quantization channels. Reserve both passes before dense storage.
  constexpr size_t work_per_pixel=86;
  if(pixels>policy.max_image_pixels||pixels>policy.max_output_bytes/4||
     pixels>policy.max_sample_operations/work_per_pixel||pixels>policy.max_raster_samples)
    throw std::length_error("Native physical field exceeds cumulative sample/image budget");
  size_t scratch=pixels*(sizeof(std::array<float,3>)+4);
  auto charge=[&](size_t n,size_t unit){if(scratch>policy.max_working_bytes||n>(policy.max_working_bytes-scratch)/unit)
    throw std::length_error("Native physical field exceeds working budget before allocation");scratch+=n*unit;};
  // Packet capture retains the already bounded immutable grid; its pointer
  // vectors and owner sets are also bounded before capture allocates them.
  charge(paint.storage_bytes(),1);
  for(const auto &layer:paint.layers())charge(layer.tiles.size()+layer.pbr_tiles.size()+layer.emission_tiles.size(),sizeof(void*)*8);
  auto packet=PaintOverlayFactory::capture(paint,cancelled);
  charge(packet->storage_bytes(),1);
  std::vector<std::array<float,3>> radiance(pixels);std::array<float,3> peak{};
  auto linear=[](float v){return v<=.04045f?v/12.92f:std::pow((v+.055f)/1.055f,2.4f);};
  auto srgb=[](float v){return v<=.0031308f?v*12.92f:1.055f*std::pow(v,1/2.4f)-.055f;};
  for(size_t at=0;at<pixels;++at){if((at&1023)==0)stop();
    const float u=(float(at%r)+.5f)/r,v=(float(at/r)+.5f)/r;
    const auto overlay=sample_overlay_values(*packet,u,v,true);
    std::array<float,4> source{1,1,1,1};
    if(maps&&maps->emission.image&&!(o.material_bindings&&o.material_bindings->physical_emission))
      source=sample_source_texture(maps->emission,u,v,true);
    for(size_t k=0;k<3;++k){const double value=double(background[k])*source[k]*(1-overlay[3][3])+overlay[2][k]+overlay[4][k];
      if(!std::isfinite(value)||value<0||value>30000)throw std::invalid_argument("Native physical radiance is outside its finite range");
      radiance[at][k]=float(value);peak[k]=std::max(peak[k],float(value));}
  }
  auto &entry=out.emission;entry.strength=std::max({1.f,peak[0],peak[1],peak[2]});
  for(size_t k=0;k<3;++k)entry.factor[k]=peak[k]/entry.strength;
  entry.image={r,r,std::vector<uint8_t>(pixels*4)};
  for(size_t at=0;at<pixels;++at){if((at&1023)==0)stop();
    for(size_t k=0;k<3;++k){const float actual=radiance[at][k];
      const auto q=uint8_t(std::clamp(std::lround(srgb(peak[k]>0?actual/peak[k]:0)*255),0l,255l));
      entry.image.rgba[at*4+k]=q;const double restored=double(linear(q/255.f))*entry.factor[k]*entry.strength;
      const double error=std::abs(double(actual)-restored);
      entry.maximum_absolute_error=std::max(entry.maximum_absolute_error,error);
      if(actual>0){entry.maximum_relative_error=std::max(entry.maximum_relative_error,error/actual);
        if(restored==0)++entry.erased_nonzero_samples;if(entry.unlit)++entry.unlit_ignored_samples;}}
    entry.image.rgba[at*4+3]=255;
  }
  out.images=pixels*4;out.memory=out.images;out.samples=pixels*work_per_pixel;out.raster=pixels;
  stop();return out;
}
}
std::vector<ObjectSurface> native_authoring_surfaces(const Scene &scene,
    const std::function<bool()> &cancelled) {
  auto detached=scene.snapshot();
  for(const auto &original:scene.objects()){
    if(cancelled&&cancelled())throw std::runtime_error("Native physical surface extraction cancelled");
    if(native_physical(original))omit_current_emission(detached.object(original.id));
  }
  auto surfaces=detached.preview_surfaces(cancelled);
  for(auto &surface:surfaces){const auto &original=scene.object(surface.id);
    if(native_physical(original)){surface.material=original.material;surface.material_bindings=original.material_bindings;}}
  return surfaces;
}
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
    if(native_physical(object))painted=true;
    else if(physical(object)&&(!object.painted||!object.painted->source_sampling_prepared())) {
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
  result.surfaces = native_authoring_surfaces(staged,stop);
  for(auto &surface:result.surfaces){const auto &object=staged.object(surface.id);
    if(object.mesh_active&&object.painted->source_sampling_prepared())
      source_paint_export(*object.painted,surface,stop);
  }
  for (auto &surface : result.surfaces) {
    check();
    const auto &object=capture.scene().object(surface.id);
    if(native_physical(object)) {
      SourceUvBakeOptions bounded=policy;bounded.cancelled=stop;
      if(retained_images>=maximum_images||fixed>=policy.max_working_bytes||
         retained_memory>=policy.max_working_bytes-fixed||samples>=policy.max_sample_operations||raster>=policy.max_raster_samples)
        throw std::length_error("Native physical cumulative material budget exhausted");
      bounded.max_output_bytes=maximum_images-retained_images;
      bounded.max_working_bytes=policy.max_working_bytes-fixed-retained_memory;
      bounded.max_sample_operations=policy.max_sample_operations-samples;
      bounded.max_raster_samples=policy.max_raster_samples-raster;
      auto field=native_physical_field(object,bounded,stop);
      retained_images+=field.images;retained_memory+=field.memory;samples+=field.samples;raster+=field.raster;
      // Geometry, retained coordinates, role frames and normal proof come from
      // the actual captured surface. Only this emitted current material changes.
      surface.material.emissive={};
      if(surface.material_bindings){auto binding=std::make_shared<SourceMaterialBindings>(*surface.material_bindings);
        binding->physical_emission.reset();binding->native_reference.emission={};surface.material_bindings=std::move(binding);}
      result.emission.push_back(std::move(field.emission));
      result.diagnostics.push_back("Object "+std::to_string(object.id)+
        ": finite physical-emission view in the actual writable paint domain; exact float project retained");
    }
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
       (entry.image.width&& !primitives.at(0).at("attributes").contains("TEXCOORD_0")))
      throw std::logic_error("Physical compiler requires its exact emitted primitive mapping");
    unsigned coordinate=0;
    if(entry.image.width&&found->paint_sampling_attachment){
      const auto &material=j.at("materials").at(identity.material);
      if(!material.contains("emissiveTexture"))throw std::logic_error("Retained physical field lacks its compiled paint coordinate association");
      coordinate=material.at("emissiveTexture").value("texCoord",0u);
      if(coordinate>7||!primitives[0].at("attributes").contains("TEXCOORD_"+std::to_string(coordinate)))
        throw std::logic_error("Retained physical field references a missing genuine coordinate accessor");
    }
    size_t material_users=0;for(const auto &mesh:j.at("meshes"))for(const auto &primitive:mesh.at("primitives"))
      if(primitive.contains("material")&&primitive.at("material")==identity.material)++material_users;
    if(material_users>1){auto material=j.at("materials").at(identity.material);identity.material=int(j.at("materials").size());
      j["materials"].push_back(std::move(material));primitives[0]["material"]=identity.material;}
    if(entry.image.width) {
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
    material["emissiveTexture"]={{"index",texture}};
    if(coordinate)material["emissiveTexture"]["texCoord"]=coordinate;
    }
    auto &material=j["materials"].at(identity.material);
    const bool unlit=material.contains("extensions")&&material.at("extensions").contains("KHR_materials_unlit");
    if(unlit!=entry.unlit)throw std::logic_error("Physical finite view changed the actual unlit interpretation");
    material["emissiveFactor"]=entry.factor;
    if(entry.strength>1){material["extensions"]["KHR_materials_emissive_strength"]={{"emissiveStrength",entry.strength}};
      auto &used=j["extensionsUsed"];if(used.is_null())used=Json::array();
      if(std::find(used.begin(),used.end(),"KHR_materials_emissive_strength")==used.end())used.push_back("KHR_materials_emissive_strength");}
    else if(material.contains("extensions"))material["extensions"].erase("KHR_materials_emissive_strength");
    material["extras"]["domeSculptPortableEmission"]={{"version",1},
      {"evaluation",entry.image.width?(found->paint_sampling_attachment?"finite-current-core-pbr-paint-domain":"finite-current-core-pbr-uv0"):"current-native-constant"},{"units","native-linear-radiance"},
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
