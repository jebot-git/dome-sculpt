// SPDX-License-Identifier: GPL-3.0-or-later
#include "bake-fixture-identity.hpp"
#include "current-color-target-fixture.hpp"
#include "dome/document.hpp"
#include "dome/uv.hpp"
#include "dome/source-uv-bake.hpp"
#include "polygon-color-target.hpp"
#include "uv-canonical-transport.hpp"
#include "dome/tangents.hpp"
#include <iostream>
using namespace dome;
namespace {size_t checks=0;void check(bool v,const char *s){if(!v)throw std::runtime_error(s);++checks;std::cout<<"PASS "<<s<<std::endl;}template<class F>void refuse(F f,const char *s){bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed,s);}}
int main()try{
 for(bool seam:{false,true})for(bool posed:{false,true}){
  auto initial=current_color_target_fixture::fixture(seam);const auto id=initial.active().id,source=initial.objects().back().id;
  Document document;document.replace(initial.snapshot());ObjectBakeSettings settings;settings.prepare_colors=true;settings.ao_samples=0;
  document.bake_from_object(source,settings);check(document.wait_asset()&&document.wait_mesh()&&document.frame().state->error.empty(),"Actual native Bake prepares the current-corner target");
  auto scene=document.copy_scene();if(posed)cut_color_fixture::pose(scene);auto &old=scene.object(id);const auto original=scene.encode();
  check(old.painted->baked()&&!old.painted->baked()->maps.normal.rgba.empty(),"Prepared UV fixture includes actual Bake normal pixels rather than a colour-only substitute");
  UvWorkspace workspace(old.painted->mesh(),64);const auto &mesh=old.painted->mesh();size_t front=0;while(front<mesh.indices.size()/3&&mesh.normals[mesh.indices[front*3]].z<.9f)++front;
  std::array<float,2> cursor{};for(size_t k=0;k<3;++k){auto uv=mesh.uv[mesh.indices[front*3+k]];cursor[0]+=uv[0]/3;cursor[1]+=uv[1]/3;}workspace.edit(UvAction::Pick,cursor[0],cursor[1]);workspace.edit(UvAction::Move,1.f/64,0);
  auto view=workspace.view();auto derivative=prepare_polygon_uv_derivative(old,*view.mesh,*view.source_vertices,{});
  SourceUvBakeInput input;input.paint=old.painted;input.current_material=old.material_bindings;input.current_deformation=old.deformation;input.polygon_authority=old.polygon;input.target_polygon=derivative.authority;input.target=derivative.mesh;input.source_vertices=derivative.source_vertices;
  auto unbound=input;unbound.polygon_authority.reset();refuse([&]{bake_source_uv(unbound);},"Prepared current colour cannot borrow a target field without its own active source polygon witness");
  if(posed){
   check(detail::preserves_canonical_uv_derivatives(mesh,input.target,input.source_vertices,256*1024*1024),"Exact triangle derivatives and global UV seam partition certify native translation without replacing the canonical axes");
   bool exact=true;for(size_t pose=0;pose<37;++pose){auto state=*old.deformation;state.morph_weights[0]=float(pose)/36;float a=(float(pose)-18)*.013f;state.pose[1].rotation={std::cos(a),0,std::sin(a),0};state.pose[1].scale={1+.003f*pose,1-.002f*pose,1+.001f*pose};auto a_mesh=state.preview(mesh),b_mesh=state.preview(input.target);exact&=tangent_corners(a_mesh)==tangent_corners(b_mesh);}
   check(exact,"Independent actual Mikk generation stays bit-exact across 37 mixed skin/morph/anisotropic poses after certified translation");
  }
  auto result=bake_source_uv(input);auto adopted=adopt_source_uv_bake(result);
  check(result.color_reprojection&&result.color_reprojection->uv_reprojection&&adopted.material_bindings->color_preparation==result.color_reprojection,"Detached preparation produces an independently owned current-domain UV recipe");
  auto prior=old.material_bindings->color_preparation,current=adopted.material_bindings->color_preparation;
  check(current->receipt.encode()==prior->receipt.encode()&&current->initial_projection==prior->initial_projection&&current->polygon_colors==prior->polygon_colors&&current->original_paint==prior->original_paint,"UV editing retains exact initial colour authority and measured initial receipt");
  check(current->current_descriptor()->field==old.polygon->colors&&current->uv_reprojection->before_material==old.material_bindings&&current->uv_reprojection->before_paint==old.painted,"New current receipt owns the actual prior paint/material and independent corner field");
  check(adopted.paint->layers().size()==old.painted->layers().size()&&adopted.paint->baked()&&!adopted.paint->baked()->maps.normal.rgba.empty()&&!adopted.paint->baked()->maps.pbr_color.rgba.empty(),"Actual UV transfer preserves editable common/PBR channels and baked normal/PBR planes");
  auto changed=scene.snapshot();auto &target=changed.object(id);target.painted=adopted.paint;target.material_bindings=adopted.material_bindings;target.deformation=adopted.deformation;target.appearance=adopted.appearance;target.polygon=derivative.authority;changed.validate();auto encoded=changed.encode();
  check(encoded.at("version")==30&&target.bake_recipe==old.bake_recipe&&bool(old.bake_recipe)&&encoded.at("source_materials").at("version")==8,"Actual saved recipe and current-domain UV replay compose Scene30/material8");
  auto loaded=Scene::decode(encoded);check(loaded.encode()==encoded,"Scene30 restarts the bake and UV recipes, current field, initial receipt and before-state");
  const auto saved=loaded.object(id).material_bindings->color_preparation;check(saved->current_descriptor()->field==loaded.object(id).polygon->colors&&saved->uv_reprojection->before_material->color_preparation->polygon_colors==saved->polygon_colors,"Restart preserves current-field aliases and immutable initial descriptor ownership");
  auto control=changed;control.active().bake_recipe.reset();auto uv_only=control.encode();check(uv_only.at("version")==23,"Actual UV-only capability remains Scene23 when the independent bake recipe is absent");auto legacy=uv_only;legacy["version"]=20;refuse([&]{Scene::decode(legacy);},"Old Scene20 cannot silently ignore new current-colour UV authority");
  auto wrong=encoded;wrong["source_materials"]["version"]=6;refuse([&]{Scene::decode(wrong);},"Legacy material6 cannot admit new UV receipt metadata");
  auto before_control=scene;before_control.active().bake_recipe.reset();check(original.at("version")==30&&bool(old.bake_recipe)&&before_control.encode().at("version")==20&&original.at("source_materials").at("version")==6,"Actual recipe selects Scene30 while removing only that recipe restores the genuine pre-UV Scene20/material6 capability");
  for(const auto mode:{0,1,2,3,4,5}){auto bad=encoded;auto &entries=bad["source_materials"]["color_preparations"];size_t at=0;while(!entries[at].contains("uv_reprojection"))++at;auto &node=entries[at]["uv_reprojection"];
   if(mode==0)node["before_binding"]=bad["source_materials"]["bindings"].size()-1;
   if(mode==1)node["paint"]=entries[at]["paint"];
   if(mode==2)entries[at]["initial_projection"]=entries[at]["projection"];
   if(mode==3){auto &bytes=node["layout"]["source_vertices"].get_binary();bytes[0]=255;}
   if(mode==4)bad["polygon_sources"]["color_fields"].push_back(bad["polygon_sources"]["color_fields"][0]);
   if(mode==5)node["receipt"]["raster_budget"]=1;
   refuse([&]{Scene::decode(bad);},"Cross-before-state, initial projection, exact layout, orphan and quota forgeries cannot borrow current-field authority");
  }
  auto unsupported=uv_only;unsupported["version"]=21;refuse([&]{Scene::decode(unsupported);},"Standalone colour23 does not borrow the reserved normal21 format");
  size_t calls=0;SourceUvBakeOptions measured;measured.cancelled=[&]{++calls;return false;};bake_source_uv(input,measured);check(calls>100,"Actual prepared UV cancellation work is measured across geometry, all-channel raster and current field replay");
  for(auto boundary:{calls/2,calls}){size_t current_call=0;SourceUvBakeOptions cancel;cancel.cancelled=[&]{return ++current_call>=boundary;};refuse([&]{bake_source_uv(input,cancel);},"Middle and final actual prepared UV cancellation refuse before adoption");check(scene.encode()==original,"Cancelled prepared UV retains exact whole source paint, field, pose and initial receipt");}
  auto forged=*current->uv_reprojection;forged.receipt.projection_error_bound[0]+=.001f;refuse([&]{forged.validate();},"Current UV quality cannot borrow or forge observed initial bounds");
  auto wrong_target=input;wrong_target.target.positions[0].x+=.001f;refuse([&]{bake_source_uv(wrong_target);},"Equal-sized changed REST cannot borrow the actual own corner descriptor");
  check(scene.encode()==original,"All detached attempts preserve full caller paint, source, normal and colour archives");
 }
 std::cout<<checks<<" prepared current-corner UV23 checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
