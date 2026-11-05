// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/scene.hpp"
#include "dome/shapekey-authoring.hpp"
#include "key-archive-preflight.hpp"
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
}
int main(int argc,char **argv)try{if(argc!=2)return 2;small(Scene::load(argv[1]));std::cout<<"PASS "<<checks<<" genuine canonical37 key quota checks\n";return 0;}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
