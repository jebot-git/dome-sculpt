// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/scene.hpp"
#include "dome/shapekey-authoring.hpp"
#include "key-archive-preflight.hpp"
#include "polygon-conversion.hpp"
#include "normal-key-derivation.hpp"
#include "canonical-paint-history.hpp"
#include "canonical-normal-owner.hpp"
#include "native-paint-frame-source.hpp"
#include <iostream>
using namespace dome;
namespace {
size_t checks=0;
void check(bool ok,const char *message){++checks;std::cout<<(ok?"PASS ":"FAIL ")<<message<<std::endl;if(!ok)throw std::runtime_error(message);}
size_t model(const nlohmann::json &d){
 if(d.is_null())return 0;size_t n=d.at("pose").size()+d.at("morph_weights").size();
 for(const auto &p:d.at("pose"))if(p.contains("full_local_matrix"))n+=16;
 if(!d.at("skin").is_null())for(const auto &row:d.at("skin").at("vertices"))n+=row.size();
 if(!d.at("morphs").is_null())for(const auto &t:d.at("morphs").at("targets"))n+=t.at("delta").size();return n;
}
size_t actual_models(const nlohmann::json &j){
 size_t n=j.contains("source_materials")?SourceMaterialArchive::deformation_entries(j.at("source_materials")):0;
 for(const auto &o:j.at("objects"))n+=model(o.at("deformation"));
 if(j.contains("polygon_sources"))for(const auto &p:j.at("polygon_sources").at("origins"))n+=model(p.at("deformation"));
 if(j.contains("baked_normal_fields"))for(const auto &p:j.at("baked_normal_fields").at("origins"))n+=model(p.at("deformation"));return n;
}
size_t actual_frames(const Scene &s){
 SourceMaterialArchive materials;detail::PolygonSceneArchive origins;
 for(const auto &o:s.objects()){
  if(o.material_bindings)materials.add(o.material_bindings,false);
  if(o.polygon)origins.add(o.polygon,false);if(o.shape_source)origins.add_origin(o.shape_source,false);if(o.rig_source)origins.add_rig_origin(o.rig_source,false);
 }
 detail::collect_canonical_paint_controls(materials,origins,false);
 NormalFrameArchive frames;
 for(const auto &b:materials.bindings()){
  if(b->normal_frames)frames.add(b->normal_frames,false);
  if(b->canonical_attributes)frames.add(b->canonical_attributes->frames(),false);
  if(b->native_paint_frames){frames.add(b->native_paint_frames->base_frames(),false);frames.add(b->native_paint_frames->frames(),false);}
 }
 return frames.storage_bytes();
}
detail::KeyArchiveProspect plan(const Scene &after,const detail::SourceKeyDerivation *reuse){
 const auto &o=after.active();const auto &frames=*o.material_bindings->normal_frames;const auto domain=frames.position_domain;
 return {domain,frames.storage_bytes()-domain->storage_bytes(),o.deformation->resource_entries(),reuse};
}
void oracle(const Scene &before,const Scene &after,const detail::SourceKeyDerivation *reuse){
 const auto usage=detail::preflight_key_archive(before,before.active().id,plan(after,reuse));const auto j=after.encode();const auto models=actual_models(j),frames=actual_frames(after);
 std::cout<<"USAGE prospective_models="<<usage.deformation_entries<<" archive_models="<<models<<" prospective_frames="<<usage.frame_bytes<<" archive_frames="<<frames<<std::endl;
 check(usage.deformation_entries==models,"Prospective model count equals every actual inline current and retained archive slot");
 check(usage.frame_bytes==frames,"Prospective frame bytes equal exact current and retained pooled owner closure");
}
ShapeKeyEdit replacement(const Scene &s){ShapeKeyEdit edit;edit.action=ShapeKeyAction::Replace;edit.target=s.active().deformation->morphs->targets.size()-1;edit.positions=s.active().painted->mesh().positions;for(size_t v=0;v<edit.positions.size();++v)edit.positions[v]+=s.active().deformation->morphs->targets.back().delta[v];edit.positions.at(0).z+=.002f;return edit;}
void small(Scene source){
 const auto exact=source.encode();ShapeKeyEdit create;create.name="Actual post37 quota Create";auto created=source;author_shapekey(created,created.active().id,create);oracle(source,created,nullptr);
 const auto old_anchor=created.active().material_bindings->key_derivation;auto edit=replacement(created);auto replaced=created;author_shapekey(replaced,replaced.active().id,edit);oracle(created,replaced,old_anchor.get());check(replaced.active().material_bindings->key_derivation->before_material()==old_anchor->before_material(),"Compatible replacement keeps the actual immutable before material anchor");
 auto shared=created;const auto id=shared.active().id;const auto other=shared.duplicate(id);shared.select(id);auto shared_after=shared;author_shapekey(shared_after,id,edit);oracle(shared,shared_after,old_anchor.get());check(shared_after.object(other).material_bindings==shared.object(other).material_bindings,"Shared second actor retains its real old frame owner across replacement");
 auto retained=created;retained.active().rig_source=capture_polygon_origin(retained,retained.active().id);retained.validate();auto retained_after=retained;author_shapekey(retained_after,retained.active().id,edit);oracle(retained,retained_after,old_anchor.get());
 const auto prospect=plan(replaced,old_anchor.get());size_t polls=0;detail::preflight_key_archive(created,created.active().id,prospect,[&]{++polls;return false;});const auto unchanged=created.encode();for(size_t cut:{size_t(1),polls/2,polls}){size_t at=0;bool refused=false;try{detail::preflight_key_archive(created,created.active().id,prospect,[&]{return ++at>=cut;});}catch(const std::runtime_error&e){refused=std::string_view(e.what()).find("cancelled")!=std::string_view::npos;}check(refused&&created.encode()==unchanged,"Actual early middle late archive traversal Cancel preserves full source");}
 oracle(created,replaced,old_anchor.get());check(source.encode()==exact,"Independent archive and prospective proof leave original caller Scene exact");
}
std::shared_ptr<const DeformationState> padding_model(Mesh &mesh,size_t entries){
 const size_t vertices=entries/128,remainder=entries%128;
 mesh.positions.assign(vertices,{0,0,0});mesh.normals.assign(vertices,{0,0,1});mesh.uv.assign(vertices,{0,0});
 mesh.positions.at(1)={1,0,0};mesh.positions.at(2)={0,1,0};mesh.uv.at(1)={1,0};mesh.uv.at(2)={0,1};mesh.indices={0,1,2};mesh.validate();
 auto d=std::make_shared<DeformationState>(mesh);d->morphs=MorphSet(mesh);
 for(size_t t=0;t<128;++t)d->morphs->targets.push_back({"Legal padding target "+std::to_string(t),std::vector<Vec3>(vertices)});
 if(remainder){const bool matrix=remainder>=17;const size_t bones=remainder-(matrix?16:0);d->skeleton=Skeleton{};for(size_t i=0;i<bones;++i)d->skeleton->bones.push_back({"Padding joint "+std::to_string(i),-1});d->pose.resize(bones);if(matrix)d->pose[0].full_local_matrix=RigMatrix{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};}
 d->validate(mesh);check(d->resource_entries()==entries,"Actual independent legal morph/pose actor has exact requested model entries including full pose16");return d;
}
uint64_t append_padding(Scene &s,size_t entries){
 const auto selected=s.active().id;Mesh mesh;auto d=padding_model(mesh,entries);auto paint=std::make_shared<PaintedMesh>(std::move(mesh),64);const auto id=s.add("Independent exact resource actor",Volume{});auto &o=s.object(id);o.mesh_active=true;o.visible=false;o.painted=paint;o.deformation=d;s.select(selected);s.validate();return id;
}
void boundary(Scene source){
 constexpr size_t cap=8'000'000;ShapeKeyEdit seed;seed.name="Actual first current37 quota anchor";author_shapekey(source,source.active().id,seed);const auto baseline=source;
 ShapeKeyEdit create;create.name="Exact model cap current37 key";auto tiny_after=source;author_shapekey(tiny_after,tiny_after.active().id,create);const auto prospective=plan(tiny_after,source.active().material_bindings->key_derivation.get());const auto actual=actual_models(tiny_after.encode());
 for(size_t excess:{size_t(0),size_t(1)}){
  auto input=baseline;const auto padding=append_padding(input,cap-actual+excess);const auto initial_binding=input.active().material_bindings;const auto initial_paint=input.active().painted;const auto initial_deform=input.active().deformation;const auto other_paint=input.object(padding).painted;const auto other_deform=input.object(padding).deformation;
  bool refused=false;detail::KeyArchiveUsage usage;
  try{usage=detail::preflight_key_archive(input,input.active().id,prospective);}catch(const std::length_error &e){refused=std::string_view(e.what()).find("Prospective retained key deformation")!=std::string_view::npos;}
  if(excess){check(refused,"One entry above actual prospective aggregate model limit refuses while current Scene remains valid");bool authored=false;try{author_shapekey(input,input.active().id,create);}catch(const std::length_error &e){authored=std::string_view(e.what()).find("Prospective retained key deformation")!=std::string_view::npos;}check(authored&&input.active().material_bindings==initial_binding&&input.active().painted==initial_paint&&input.active().deformation==initial_deform&&input.object(padding).painted==other_paint&&input.object(padding).deformation==other_deform,"Actual Create just above aggregate limit preserves every current and independent actor owner");}
  else{
   check(!refused&&usage.deformation_entries==cap,"Exactly8M prospective reachable inline model entries are accepted without changing cap");author_shapekey(input,input.active().id,create);input.validate();check(input.object(padding).painted==other_paint&&input.object(padding).deformation==other_deform&&input.active().deformation->morphs->targets.size()==tiny_after.active().deformation->morphs->targets.size(),"Actual Create at exact cap publishes only selected key and shares independent legal resource actor");
   const auto edit=replacement(input);const auto old_key=input.active().material_bindings->key_derivation;auto p=plan(input,old_key.get());auto replacement_usage=detail::preflight_key_archive(input,input.active().id,p);check(replacement_usage.deformation_entries==cap,"Compatible replacement at cap excludes obsolete current model but retains immutable named anchor");author_shapekey(input,input.active().id,edit);input.validate();check(input.object(padding).deformation==other_deform&&input.active().material_bindings->key_derivation->before_material()==old_key->before_material(),"Actual independent nonzero Replace succeeds at exact model cap without recapturing or losing history");
  }
 }
 check(source.active().material_bindings==baseline.active().material_bindings,"Boundary controls leave original actual current37 anchor exact");
}

}
int main(int argc,char **argv)try{if(argc<2||argc>3)return 2;if(argc==3&&std::string_view(argv[2])=="boundary")boundary(Scene::load(argv[1]));else small(Scene::load(argv[1]));std::cout<<"PASS "<<checks<<" genuine canonical37 key quota checks\n";return 0;}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
