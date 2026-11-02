// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/creation.hpp"
#include "dome/normal-preparation.hpp"
#include "dome/document.hpp"
#include "dome/shapekey-authoring.hpp"
#include <iostream>
using namespace dome;
int main()try{
 for(const char *baseline:{"vroid-beta-female","vroid-beta-male"}){
  Scene s;auto group=append_humanoid(s,humanoid_baseline_defaults(baseline),"Prepared real VRoid",{},64);
  auto i=std::find_if(s.objects().begin(),s.objects().end(),[&](const auto &o){return o.group==group&&o.name.find("Face_00_SKIN")!=std::string::npos&&o.deformation&&o.deformation->morphs;});
  if(i==s.objects().end())throw std::runtime_error("Missing real VRoid artist face");s.select(i->id);auto &o=s.active();std::cout<<"REAL_VROID "<<baseline<<" faces="<<o.painted->mesh().indices.size()/3<<" targets="<<o.deformation->morphs->targets.size()<<std::endl;
  const auto prior=s.encode();auto p=prepare_source_normals({o.painted,o.appearance,o.material_bindings,o.deformation,o.material});o.painted=p.paint;o.material_bindings=p.material_bindings;s.validate();
  const auto prepared=s.encode();Document d;d.replace(s);if(!d.wait_mesh())throw std::runtime_error("Real prepared model worker timeout");ShapeKeyEdit edit;edit.name="Prepared real avatar key";d.author_shape(edit);if(!d.wait_asset()||!d.wait_mesh()||!d.frame().state->error.empty())throw std::runtime_error(d.frame().state->error);
  auto after=d.copy_scene();if(after.active().painted->encode()!=s.active().painted->encode()||after.active().deformation->morphs->targets.size()!=s.active().deformation->morphs->targets.size()+1)throw std::runtime_error("Real prepared source changed paint or lost authored key");edit.action=ShapeKeyAction::Restore;author_shapekey(after,after.active().id,edit);if(after.encode()!=prepared)throw std::runtime_error("Real prepared Restore differs");
  std::cout<<"PASS real "<<baseline<<" prepared Create + exact paint/Restore; raw scene bytes="<<prior.dump().size()<<std::endl;
 }
 return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
