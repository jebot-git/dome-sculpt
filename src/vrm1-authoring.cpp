// SPDX-License-Identifier: GPL-3.0-or-later
#include "vrm1-authoring.hpp"
#include "dome/vrm-authoring.hpp"
#include "vrm1-export.hpp"
#include "vrm1-metadata.hpp"
#include "source-paint-export.hpp"
#include "paint-overlay-sample.hpp"
#include "emission-authority.hpp"
#include "dome/source-paint-bake.hpp"
#include <numeric>
namespace dome::detail {
namespace {
using J=nlohmann::json;
uint32_t word(std::span<const uint8_t> bytes,size_t at){if(at>bytes.size()||bytes.size()-at<4)throw std::invalid_argument("VRM1 original GLB is truncated");return uint32_t(bytes[at])|(uint32_t(bytes[at+1])<<8)|(uint32_t(bytes[at+2])<<16)|(uint32_t(bytes[at+3])<<24);}
J original_json(const ObjectSurface &surface,const std::function<bool()> &cancel){
 if(!surface.appearance||!surface.appearance->original_bytes)throw std::invalid_argument("VRM1 authoring requires immutable original metadata");
 const auto &bytes=*surface.appearance->original_bytes;if(bytes.size()<28||word(bytes,0)!=0x46546c67||word(bytes,4)!=2||word(bytes,8)!=bytes.size()||word(bytes,16)!=0x4e4f534a)throw std::invalid_argument("VRM1 original source is not a captured GLB");
 const size_t count=word(bytes,12);if(count>16*1024*1024||count>bytes.size()-20)throw std::length_error("VRM1 original JSON exceeds bounded source metadata");
 size_t visits=0;auto json=J::parse(bytes.begin()+20,bytes.begin()+20+count,[&](int depth,J::parse_event_t,const J&){if(depth>128||++visits>1024*1024)throw std::length_error("VRM1 source metadata depth/count exceeded");if((visits&4095)==0&&cancel&&cancel())throw std::runtime_error("VRM1 source metadata cancelled");return true;});validate_vrm1_metadata(json,cancel,true);return json;
}
void expressions(const J &original,int material,const std::set<std::string> &properties){
 const auto &vrm=original.at("extensions").at("VRMC_vrm");if(!vrm.contains("expressions"))return;
 for(const auto *table:{"preset","custom"})if(vrm.at("expressions").contains(table))for(const auto &expression:vrm.at("expressions").at(table)){
  for(const auto &bind:expression.value("textureTransformBinds",J::array()))if(bind.at("material").get<int>()==material)throw std::invalid_argument("VRM1 painted UV expressions require an exact source/paint expression-layer adapter");
  for(const auto &bind:expression.value("materialColorBinds",J::array()))if(bind.at("material").get<int>()==material&&properties.contains(bind.at("type").get<std::string>()))throw std::invalid_argument("VRM1 painted colour expressions require an exact factor/contribution adapter");
 }
}
}
std::shared_ptr<const Vrm1AuthoringCompilation> compile_vrm1_authoring(
 std::shared_ptr<const NativeAuthoringCapture> capture,const SourceUvBakeOptions &policy,
 size_t maximum_output,const std::function<bool()> &cancelled){
 auto stop=[a=cancelled,b=policy.cancelled]{return (a&&a())||(b&&b());};auto check=[&]{if(stop())throw std::runtime_error("VRM1 authoring compilation cancelled");};check();
 if(!capture||capture->originals().empty())throw std::invalid_argument("VRM1 authoring needs genuine native source surfaces");
 auto result=std::shared_ptr<Vrm1AuthoringCompilation>(new Vrm1AuthoringCompilation);result->capture_=std::move(capture);result->original_=original_json(result->capture_->originals().front(),stop);
 size_t working=sizeof(Scene)+sizeof(Vrm1AuthoringCompilation),images=0,samples=0,raster=0;
 auto reserve=[&](size_t n,size_t unit){if(working>policy.max_working_bytes||n>(policy.max_working_bytes-working)/unit)throw std::length_error("VRM1 authoring geometry staging exceeds working budget");working+=n*unit;};
 reserve(result->capture_->originals().size(),sizeof(ObjectSurface));reserve(result->capture_->scene().objects().size(),sizeof(SculptObject));reserve(result->capture_->scene().groups().size(),sizeof(SceneGroup));
 for(const auto &surface:result->capture_->originals()){
  check();const auto &m=surface.mesh;reserve(m.positions.size()+m.normals.size(),sizeof(Vec3));reserve(m.indices.size(),sizeof(uint32_t));reserve(m.uv.size(),sizeof(std::array<float,2>));reserve(surface.name.size()+1,1);reserve(surface.groups.size(),sizeof(SceneGroup));
  if(!surface.appearance||surface.appearance->asset!=result->capture_->originals().front().appearance->asset||surface.appearance->original_bytes!=result->capture_->originals().front().appearance->original_bytes)throw std::invalid_argument("VRM1 authoring cannot mix independent original avatars");
 }
 auto staged=result->capture_->scene();
 struct View {std::shared_ptr<const TextureMaps> texture;std::shared_ptr<const SourceMaterialBindings> material;};std::map<uint64_t,View> views;
 const size_t image_limit=std::min(maximum_output,policy.max_output_bytes);
 for(const auto &surface:result->capture_->originals()){
  check();const auto &object=result->capture_->scene().object(surface.id);if(!object.painted)throw std::invalid_argument("VRM1 authoring source has no captured native paint");
  if(object.painted->has_physical_emission()||hdr_emission(object.material.emissive)||(object.material_bindings&&physical_emission(*object.material_bindings)))throw std::invalid_argument("VRM1 physical authoring requires its precision-qualified typed adapter");
  auto packet=PaintOverlayFactory::capture(*object.painted,stop);if(packet->empty())continue;
  if(!object.material_bindings)throw std::invalid_argument("VRM1 painted source lacks its material owner");
  uint32_t resolution=policy.resolution?policy.resolution:object.painted->resolution();if(!policy.resolution){for(const auto &role:object.material_bindings->roles)if(role.image)resolution=std::max({resolution,role.image->width,role.image->height});}
  if(resolution<64||resolution>4096)throw std::length_error("VRM1 portable paint resolution exceeds64..4096");
  const size_t pixels=size_t(resolution)*resolution;
  if(pixels>policy.max_image_pixels||pixels>image_limit/12)throw std::length_error("VRM1 typed paint output forecast exceeds budget");
  const size_t extra_images=pixels*12,extra_samples=pixels*2*(32+4*packet->layers().size());
  if(images>image_limit||extra_images>image_limit-images||working>policy.max_working_bytes||images>policy.max_working_bytes-working||extra_images>policy.max_working_bytes-working-images||samples>policy.max_sample_operations||extra_samples>=policy.max_sample_operations-samples||raster>=policy.max_raster_samples)throw std::length_error("VRM1 typed paint cumulative budget exceeded");
  auto bounded=policy;bounded.resolution=resolution;bounded.cancelled=stop;bounded.max_output_bytes=image_limit-images-extra_images;bounded.max_working_bytes=policy.max_working_bytes-working-images-extra_images;bounded.max_sample_operations=policy.max_sample_operations-samples-extra_samples;bounded.max_raster_samples=policy.max_raster_samples-raster;if(!bounded.max_output_bytes||!bounded.max_working_bytes)throw std::length_error("VRM1 typed paint has no role-sampling budget");
  SourceUvBakeInput input;input.paint=object.painted;input.appearance=object.appearance;input.current_material=object.material_bindings;input.current_deformation=object.deformation;input.target=object.painted->mesh();input.source_vertices.resize(input.target.positions.size());std::iota(input.source_vertices.begin(),input.source_vertices.end(),0);
  if(object.material_bindings->has_mtoon){
   if(object.material_bindings->material.version()!=MToonVersion::V1)throw std::invalid_argument("VRM1 authoring material has a different source version");
   auto sampled=bake_source_uv(input,bounded);if(!sampled.mtoon)throw std::invalid_argument("VRM1 typed sampler lost its genuine MToon1 owner");
   auto composed=compose_vrm1_paint(surface,*sampled.mtoon,sampled.mtoon_roles,*object.material_bindings,*packet,resolution,extra_images,stop);expressions(result->original_,surface.appearance->source_material,composed.rewritten_properties);
   auto adopted=adopt_source_uv_bake(sampled,stop);auto &next=staged.object(object.id);next.painted=std::move(adopted.paint);next.deformation=std::move(adopted.deformation);next.appearance=std::move(adopted.appearance);next.material_bindings=std::move(adopted.material_bindings);
   result->diagnostics_.push_back("Object "+std::to_string(object.id)+": measured maximum linear PNG quantization error base="+std::to_string(composed.maximum_quantization_error[0])+", shade="+std::to_string(composed.maximum_quantization_error[1])+", emission="+std::to_string(composed.maximum_quantization_error[2]));
   images+=sampled.image_bytes+composed.image_bytes;samples+=sampled.sample_operations+extra_samples;raster+=sampled.raster_samples;result->materials_.emplace(object.id,std::move(composed));
  }else{
   expressions(result->original_,surface.appearance->source_material,{"color","emissionColor"});auto compiled=bake_source_paint(input,object.material,bounded);auto adopted=adopt_source_uv_bake(compiled.sampling,stop);auto &next=staged.object(object.id);next.painted=std::move(adopted.paint);next.deformation=std::move(adopted.deformation);next.appearance=std::move(adopted.appearance);next.material_bindings=std::move(adopted.material_bindings);views.emplace(object.id,View{std::move(compiled.textures),std::move(compiled.material_bindings)});images+=compiled.sampling.image_bytes;samples+=compiled.sampling.sample_operations;raster+=compiled.sampling.raster_samples;
  }
  if(images>image_limit||samples>policy.max_sample_operations||raster>policy.max_raster_samples)throw std::length_error("VRM1 compiled material exceeds cumulative output/work budget");
  result->compiled_.insert(object.id);result->diagnostics_.push_back("Object "+std::to_string(object.id)+": explicit typed V1 portable material, complete original Scene retained");
 }
 check();staged.validate();result->surfaces_=staged.preview_surfaces(stop);
 if(result->surfaces_.size()!=result->capture_->ids().size())throw std::invalid_argument("VRM1 compilation changed native emitted membership");
 for(size_t i=0;i<result->surfaces_.size();++i){auto &surface=result->surfaces_[i];if(surface.id!=result->capture_->ids()[i])throw std::invalid_argument("VRM1 compilation changed native emitted order");if(result->compiled_.contains(surface.id)){if(auto at=views.find(surface.id);at!=views.end()){surface.textures=at->second.texture;surface.material_bindings=at->second.material;}}else{const auto &object=result->capture_->scene().object(surface.id);if(object.painted&&object.painted->source_sampling_prepared())source_paint_export(*object.painted,surface,stop);}}
 check();return result;
}
}

namespace dome {
VrmExportReport export_vrm_authoring(const NativeAuthoringExport &owner,
 const std::filesystem::path &path,const VrmExportOptions &options) {
 return export_vrm_authoring(owner,path,options,SourceUvBakeOptions{});
}
VrmExportReport export_vrm_authoring(const NativeAuthoringExport &owner,
 const std::filesystem::path &path,const VrmExportOptions &requested,
 const SourceUvBakeOptions &paint_policy) {
 auto options=requested;options.cancelled=[a=requested.cancelled,b=paint_policy.cancelled]{return (a&&a())||(b&&b());};
 if(options.cancelled&&options.cancelled())throw std::runtime_error("VRM1 authoring export cancelled");
 if(options.emission)throw std::invalid_argument("Competing native HDR and VRM1 authoring export owners");
 if(path.extension()!=".vrm")throw std::invalid_argument("Native VRM1 authoring export requires.vrm");
 if(options.max_output_bytes<28||options.max_output_bytes>256*1024*1024)throw std::invalid_argument("Invalid VRM1 authoring output limit");
 if(options.paint_resolution&&paint_policy.resolution&&options.paint_resolution!=paint_policy.resolution)throw std::invalid_argument("Conflicting explicit VRM1 paint resolutions");
 auto policy=paint_policy;if(options.paint_resolution)policy.resolution=options.paint_resolution;
 // Forecast the actual preview geometry before capture creates its copies.
 // Sparse paint and original assets remain shared immutable owners.
 if(!owner.scene)throw std::invalid_argument("VRM1 authoring requires immutable Scene owner");
 size_t bytes=sizeof(Scene);auto charge=[&](size_t n,size_t width){if(bytes>policy.max_working_bytes||n>(policy.max_working_bytes-bytes)/width)throw std::length_error("VRM1 capture exceeds prospective working budget");bytes+=n*width;};
 charge(owner.scene->objects().size(),sizeof(SculptObject));charge(owner.scene->groups().size(),sizeof(SceneGroup));
 size_t vertices=0,triangles=0;
 for(const auto &object:owner.scene->objects()){
  if(options.cancelled&&options.cancelled())throw std::runtime_error("VRM1 authoring capture cancelled");
  if(!object.visible)continue;
  if(owner.scope==NativeAuthoringExport::Scope::ExportedObjects&&std::find(owner.objects.begin(),owner.objects.end(),object.id)==owner.objects.end())continue;
  if(!object.painted)throw std::invalid_argument("VRM1 authoring requires source-owned mesh surfaces");
  const auto &mesh=object.painted->mesh();
  if(vertices>policy.max_vertices||mesh.positions.size()>policy.max_vertices-vertices||triangles>policy.max_triangles||mesh.indices.size()/3>policy.max_triangles-triangles)throw std::length_error("VRM1 capture geometry exceeds policy limits");
  vertices+=mesh.positions.size();triangles+=mesh.indices.size()/3;
  charge(mesh.positions.size()+mesh.normals.size(),sizeof(Vec3)*2);charge(mesh.indices.size(),sizeof(uint32_t)*2);charge(mesh.uv.size(),sizeof(std::array<float,2>)*2);
 }
 auto capture=detail::NativeAuthoringCapture::create(owner,options.cancelled);
 auto compiled=detail::compile_vrm1_authoring(std::move(capture),policy,options.max_output_bytes,options.cancelled);
 return detail::export_vrm1_authoring(*compiled,path,options);
}
}
