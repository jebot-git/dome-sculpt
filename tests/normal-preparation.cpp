// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/normal-preparation.hpp"
#include "dome/source-uv-bake.hpp"
#include "cgltf.h"
#include "normal-preparation-fixtures.hpp"
#include "../src/normal-preparation-cache.hpp"
#include <iostream>
#include <cstring>
using namespace dome;
namespace {
unsigned checks=0;
void check(bool value,const char *message){if(!value)throw std::runtime_error(message);++checks;std::cout<<"PASS "<<message<<'\n';}
bool refuses(auto function){try{function();return false;}catch(const std::exception&){return true;}}
using normal_preparation_fixture::model;
const cgltf_accessor *attribute(const cgltf_attribute *a,size_t count,cgltf_attribute_type type){for(size_t n=0;n<count;++n)if(a[n].type==type)return a[n].data;return nullptr;}
}
int main(int argc,char **argv)try {
  if(argc==3&&std::string(argv[1])=="--ui-fixture"){
    auto scene=model();auto &source=scene.object(2);auto mesh=source.painted->mesh();for(auto &p:mesh.positions)p.z=.04f;source.painted=std::make_shared<PaintedMesh>(std::move(mesh),64);scene.save(argv[2]);return 0;
  }
  if(argc==3&&std::string(argv[1])=="--inspect"){
    auto scene=Scene::load(argv[2]);auto encoded=scene.encode();auto isolated=nlohmann::json::array();
    for(const auto &object:scene.objects()){auto copy=scene;for(const auto &other:scene.objects())if(other.id!=object.id)copy.erase(other.id);copy.select(object.id);isolated.push_back(copy.encode());}
    encoded["isolated_objects"]=std::move(isolated);std::cout<<encoded.dump()<<'\n';return 0;
  }
  if(argc!=2)return 2;const std::filesystem::path prefix=argv[1];std::filesystem::create_directories(prefix.parent_path());
  auto scene=model();const auto original=scene.encode();auto &target=scene.active();
  auto prepared=prepare_source_normals({target.painted,target.appearance,target.material_bindings,target.deformation,target.material});
  check(prepared.converted&&prepared.paint->normal_projection()&&prepared.material_bindings->normal_preparation,"Explicit conversion creates owning intrinsic planes and its original-state recipe");
  check(!target.painted->normal_projection()&&!target.material_bindings&&scene.encode()==original,"Detached normal authoring leaves the complete original scene unchanged");
  auto recipe=prepared.material_bindings->normal_preparation;
  const auto cache_before=detail::normal_preparation_cache_stats();recipe->validate();const auto cache_after=detail::normal_preparation_cache_stats();
  check(cache_after.hits>cache_before.hits&&cache_after.builds==cache_before.builds,"Immutable original-owner proof cache reuses full validation without repeating source rasterization or key generation");
  check(recipe->target_frames->pbr==recipe->target_frames->mtoon&&!recipe->target_frames->canonical&&recipe->target_frames->pbr->targets[0].tangents.size()==6,"Curved native morph target has full explicit N/T authority without canonical relabeling");
  auto converted=scene;converted.active().painted=prepared.paint;converted.active().material_bindings=prepared.material_bindings;converted.validate();
  auto stored=converted.encode();check(stored.at("version")==15&&stored.at("source_materials").at("version")==4,"New interpretation emits explicit Scene15 and material archive4");
  check(Scene::decode(stored).encode()==stored,"Native restart retains intrinsic normals, all targets, mixed skin and exact before-state");
  auto changed_uv=converted.active().painted->mesh();changed_uv.uv[0][0]=.125f;
  check(refuses([&]{recipe->validate_current(changed_uv,nullptr);}),"Changed sampling UV cannot reuse intrinsic normal planes without source-aware reprojection");
  auto malformed_uv=stored;auto &uv_bytes=malformed_uv["objects"][0]["painted"]["mesh"]["uv"].get_binary();const float altered_uv=.125f;std::memcpy(uv_bytes.data(),&altered_uv,sizeof altered_uv);
  check(refuses([&]{Scene::decode(malformed_uv);}),"Native archive admission rejects changed prepared sampling UV while preserving the original receipt");
  for(auto version:{9,10,11,12}){auto old=stored;old["version"]=version;check(refuses([&]{Scene::decode(old);}),"Older scene versions reject prepared normal authority instead of silently ignoring it");}
  auto unbound=converted;unbound.active().material_bindings.reset();check(refuses([&]{unbound.validate();}),"Unbound intrinsic normal planes cannot be smuggled into an ordinary object");
  auto fake=std::make_shared<SourceNormalPreparation>(*recipe);auto projection=std::make_shared<PaintNormalProjection>(*recipe->projection);projection->pbr=std::make_shared<const Image>(*projection->pbr);fake->projection=projection;
  check(refuses([&]{fake->validate();}),"An unproven current projection cannot borrow an unrelated initial plane witness");
  auto alias=std::make_shared<SourceNormalPreparation>(*recipe);auto alias_projection=std::make_shared<PaintNormalProjection>(*recipe->projection);
  alias_projection->pbr=std::shared_ptr<const Image>(recipe->projection->pbr.get(),[](const Image*){});alias->projection=alias_projection;
  check(refuses([&]{alias->validate();}),"Identical image addresses with unrelated ownership cannot impersonate the retained projection witness");
  auto before_binding=std::make_shared<SourceMaterialBindings>();auto cyclic=std::make_shared<SourceNormalPreparation>(*recipe);before_binding->normal_preparation=cyclic;cyclic->original_material=before_binding;
  check(refuses([&]{before_binding->validate();}),"A cyclic material before-state is bounded and refused before recursive archive publication");before_binding->normal_preparation.reset();cyclic->original_material.reset();
  Document document;document.replace(scene);check(document.wait_mesh(),"Native worker publishes the original curved target before explicit conversion");
  ObjectBakeSettings settings;settings.prepare_normals=true;settings.front_mm=500;settings.back_mm=50;settings.ao_samples=1;settings.padding=2;
  document.bake_from_object(2,settings);check(document.wait_asset()&&document.frame().state->error.empty()&&document.wait_mesh(),"Actual Document bake projects directly into the declared PBR and MToon attribute planes");
  auto baked=document.copy_scene();auto paint=baked.active().painted;auto captured=baked.active().material_bindings->normal_preparation;
  check(paint->baked()&&paint->baked()->normal_interpretation==BakedNormalInterpretation::PreparedAttributes&&!paint->baked()->maps.mtoon_normal.rgba.empty(),"Baked payload carries paired planes under its explicit new interpretation");
  check(document.frame().surface->ranges[0].source_frame&&!document.frame().surface->ranges[0].canonical_frame,"Mesh worker uses original/native target attributes and skips historical dynamic canonical fallback");
  document.undo();check(document.wait_mesh()&&document.copy_scene().encode()==original,"One Undo restores exact original paint, source attributes, deformation and materials");document.redo();check(document.wait_mesh()&&document.copy_scene().encode()==baked.encode(),"One Redo restores the complete prepared interpretation and native bake");
  auto project=prefix;project+=".dsculpt";document.save(project);Document restarted;restarted.open(project);check(restarted.wait_asset()&&restarted.wait_mesh()&&restarted.copy_scene().encode()==baked.encode(),"Actual new Document save/reopen preserves the Scene15 prepared normal pipeline");
  auto output=prefix;output+=".glb";restarted.export_asset(output).get();cgltf_options options{};cgltf_data *raw=nullptr;
  check(cgltf_parse_file(&options,output.c_str(),&raw)==cgltf_result_success,"Independent cgltf parses actual native prepared-normal GLB");std::unique_ptr<cgltf_data,decltype(&cgltf_free)> file(raw,cgltf_free);
  check(cgltf_load_buffers(&options,raw,output.c_str())==cgltf_result_success&&cgltf_validate(raw)==cgltf_result_success,"Actual output passes independent geometry/skin/morph/image consistency checks");const auto &p=raw->meshes[0].primitives[0];
  check(attribute(p.attributes,p.attributes_count,cgltf_attribute_type_tangent)&&p.targets_count==1&&attribute(p.targets[0].attributes,p.targets[0].attributes_count,cgltf_attribute_type_tangent)&&attribute(p.targets[0].attributes,p.targets[0].attributes_count,cgltf_attribute_type_normal),"Generic native normal export includes base TANGENT and full NORMAL/TANGENT morph deltas");
  auto repeat_recipe=restarted.copy_scene().active().material_bindings->normal_preparation;
  settings.prepare_normals=false;restarted.bake_from_object(2,settings);check(restarted.wait_asset()&&restarted.frame().state->error.empty()&&restarted.wait_mesh(),"Repeat native bake reuses the explicit intrinsic and target-frame interpretation");
  check(restarted.copy_scene().active().material_bindings->normal_preparation==repeat_recipe&&restarted.copy_scene().active().material_bindings->normal_preparation->receipt.encode()==captured->receipt.encode(),"Repeat bake preserves the first conversion receipt rather than relabeling its normal authority");
  auto cleared=restarted.copy_scene();cleared.active().painted=cleared.active().painted->snapshot();cleared.active().painted->clear_bake();cleared.validate();check(cleared.active().painted->normal_projection()&&!cleared.active().painted->baked(),"Clear bake preserves the explicitly prepared intrinsic model and original-state provenance");
  auto prepared_source=restarted.copy_scene();auto &readonly=prepared_source.object(2);const auto &prepared_target=prepared_source.active();
  readonly.painted=prepared_target.painted;readonly.material_bindings=prepared_target.material_bindings;readonly.deformation=prepared_target.deformation;readonly.appearance=prepared_target.appearance;
  const auto source_before=prepared_source.encode();
  check(refuses([&]{bake_object(prepared_source,2,settings);})&&prepared_source.encode()==source_before,"Prepared source normals are explicitly guarded until their exact source frame adapter exists; no posed-atlas Mikk substitution is allowed");
  const auto before_uv=restarted.copy_scene().encode();restarted.begin_uv();check(restarted.wait_uv()&&restarted.frame().state->uv&&restarted.frame().state->uv->valid,"Prepared normal target enters the real native UV workspace without changing its source interpretation");
  restarted.edit_uv(UvAction::Scale,.9f);check(restarted.wait_uv()&&restarted.frame().state->uv->valid,"A real changed UV draft validates its geometry before explicit guarded adoption");
  const auto draft_uv=restarted.frame().state->uv->mesh->uv;
  restarted.end_uv(true);check(restarted.wait_asset()&&!restarted.frame().state->error.empty()&&restarted.copy_scene().encode()==before_uv,"Unsupported prepared-normal UV adoption refuses without changing any source, current bake or native scene bytes");
  check(restarted.frame().state->uv&&restarted.frame().state->uv->mesh->uv==draft_uv&&!restarted.frame().state->uv->committing,"Failed prepared-normal UV adoption preserves the exact editable draft and clears only its busy state");
  restarted.end_uv(false);check(restarted.wait_mesh()&&restarted.copy_scene().encode()==before_uv,"Cancelling the retained UV draft restores ordinary editing with the exact pre-UV normal model");
  check(restarted.undo()&&!restarted.frame().state->can_undo&&restarted.redo()&&restarted.copy_scene().encode()==before_uv,"Failed UV adoption contributes no hidden history entry after the single repeat-bake action");
  std::cout<<"PASS "<<checks<<" prepared-normal native workflow checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
