// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/skin-authoring.hpp"
#include "dome/rig-rest.hpp"
#include "polygon-conversion.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
namespace {
size_t checks=0;void check(bool yes,const char*label){if(!yes)throw std::runtime_error(label);++checks;std::cout<<"PASS "<<label<<std::endl;}
template<class F>bool rejects(F f){try{f();return false;}catch(const std::exception&e){std::cout<<"expected refusal: "<<e.what()<<std::endl;return true;}}
bool files_equal(const std::filesystem::path&a,const std::filesystem::path&b){std::ifstream x(a,std::ios::binary),y(b,std::ios::binary);std::array<char,65536>p{},q{};do{x.read(p.data(),p.size());y.read(q.data(),q.size());if(x.gcount()!=y.gcount()||!std::equal(p.begin(),p.begin()+x.gcount(),q.begin()))return false;}while(x.gcount());return true;}
}
int main(int argc,char**argv)try{
 if(argc!=4)throw std::invalid_argument("authentic fitted-unskinned, sealed expected native field (oracle only), output");
 const std::filesystem::path out=argv[3];std::filesystem::create_directories(out);setenv("XDG_STATE_HOME",(out/"state").c_str(),1);setenv("XDG_CONFIG_HOME",(out/"config").c_str(),1);
 const auto input=Scene::load(argv[1]);uint64_t reference=0;std::vector<uint64_t>ids;
 for(const auto&o:input.objects()){if(o.deformation&&o.deformation->skeleton)reference=o.id;if(o.mesh_active&&o.painted&&o.appearance)ids.push_back(o.id);}
 check(reference&&ids.size()==19,"Actual unskinned19 and explicit native humanoid reference");
 Document doc;doc.replace(input);doc.select_objects(ids);check(doc.wait_mesh(std::chrono::seconds(120)),"Actual complete viewed source frame precedes owning admission");
 const auto before=doc.copy_scene();const auto before_value=before.encode();auto preview=doc.frame().surface;
 SkinAuthoringOptions options;options.mode=SkinAuthoringMode::Anatomical;options.reference=reference;options.limits={1,.9f};
 doc.bind_skin_selected(options,{},ids);auto save=doc.save_async(out/"full.dsculpt");save.get();check(doc.wait_asset(std::chrono::seconds(180))&&doc.frame().state->error.empty(),"Actual public selected worker generates all19 bounded anatomical fields");
 // This expected native field is read only AFTER actual production Save. It is
 // not a geometry/energy/helper input and contains no original artist weights.
 const auto sealed=Scene::load(argv[2]);auto expected=before;
 for(auto id:ids){auto&o=expected.object(id);o.deformation=sealed.object(id).deformation;o.rig_source=dome::capture_polygon_origin(before,id);}
 auto accepted=doc.copy_scene();check(accepted.encode()==expected.encode()&&Scene::load(out/"full.dsculpt").encode()==expected.encode(),"All19 actual worker fields/capsules and immediate accepted Save equal independently sealed native output");
 check(doc.frame().state->skin_options.anatomical.maximum_work==500000000&&doc.frame().state->skin_options.anatomical.maximum_workspace==256u*1024u*1024u&&doc.frame().state->skin_options.anatomical.maximum_total==512u*1024u*1024u,"Actual owning admission retains distinct500M/256MiB/512MiB bounds");
 check(doc.undo()&&doc.copy_scene().encode()==before_value&&!doc.frame().state->can_undo&&doc.redo()&&doc.copy_scene().encode()==expected.encode(),"Complete nineteen-output binding owns exactly one Undo and Redo");
 doc.edit_scene("Restore complete original source",[ids](Scene&s,const auto&cancel){for(auto id:ids)restore_rig_source(s,id,false,cancel);});doc.save_async(out/"restored.dsculpt").get();check(doc.wait_asset(std::chrono::seconds(180))&&doc.frame().state->error.empty()&&doc.copy_scene().encode()==before_value&&Scene::load(out/"restored.dsculpt").encode()==before_value,"Actual full original unskinned19 paint/morph/material/source/group Restore and ordered Save exact");
 check(doc.undo()&&doc.copy_scene().encode()==expected.encode(),"Complete Restore owns one recoverable Undo");
 // Explicit current user values, not an artist-value solver input. The public
 // locks vector must survive typed admission and override the fixed4 preset.
 auto locked=accepted;const auto target=ids.front();auto d=std::make_shared<DeformationState>(*locked.object(target).deformation);size_t row=SIZE_MAX;uint32_t joint=0;
 for(size_t v=0;v<d->skin->vertices.size();++v)if(d->skin->vertices[v].size()==1&&d->skin->vertices[v][0].weight==1){row=v;joint=d->skin->vertices[v][0].joint;break;}
 check(row!=SIZE_MAX,"Actual generated singleton row supplies explicit independent current hard-lock edit");uint32_t other=0;while(other==joint||!d->skeleton->bones.at(other).deform)++other;
 d->skin->vertices[row]={{joint,.375f},{other,.625f}};d->validate(locked.object(target).painted->mesh());locked.object(target).deformation=d;locked.select(target);
 Document locks;locks.replace(locked);locks.select_objects(std::span<const uint64_t>(&target,1));check(locks.wait_mesh(std::chrono::seconds(120)),"Current edited native skin is viewed before public lock admission");auto current=locks.copy_scene();const auto current_value=current.encode();
 auto all=options;all.locks.assign(d->skeleton->bones.size(),1);locks.bind_skin_selected(all,{},std::vector<uint64_t>{target});locks.save_async(out/"locked.dsculpt").get();
 check(locks.wait_asset(std::chrono::seconds(180))&&locks.frame().state->error.empty()&&locks.copy_scene().encode()==current_value&&Scene::load(out/"locked.dsculpt").encode()==current_value&&!locks.frame().state->can_undo,"Actual public all-lock worker preserves current edited rows and complete Scene with no false Undo");
 check(locks.frame().state->skin_use_locks&&locks.frame().state->weights.locks==all.locks&&locks.frame().state->skin_options.locks.empty(),"Typed actual admission keeps explicit joint locks separately from staged options");
 auto partial=options;partial.locks.assign(d->skeleton->bones.size(),0);partial.locks[joint]=1;auto prior=out/"locked-prior.dsculpt";std::filesystem::copy_file(out/"locked.dsculpt",prior,std::filesystem::copy_options::overwrite_existing);
 locks.bind_skin_selected(partial,{},std::vector<uint64_t>{target});auto failed=locks.save_async(out/"locked.dsculpt");
 check(rejects([&]{failed.get();})&&locks.wait_asset(std::chrono::seconds(180))&&locks.copy_scene().encode()==current_value&&!locks.frame().state->can_undo&&files_equal(out/"locked.dsculpt",prior),"Incompatible real public partial lock fails exact dependent Save and preserves current rows/history/prior file");
 locks.bind_skin_selected(options,{},std::vector<uint64_t>{target});locks.save_async(out/"unlocked-retry.dsculpt").get();auto original=accepted;original.select(target);
 check(locks.wait_asset(std::chrono::seconds(180))&&locks.frame().state->error.empty()&&locks.copy_scene().encode()==original.encode()&&Scene::load(out/"unlocked-retry.dsculpt").encode()==original.encode(),"Explicit unlocked same-controller retry computes genuine original geometry field rather than copying edited values");
 check(!locks.frame().state->skin_use_locks&&locks.undo()&&locks.copy_scene().encode()==current_value&&!locks.frame().state->can_undo,"Unlocked retry stages lock mode explicitly and owns exactly one Undo");
 std::cout<<"Actual public anatomical full/lock workflow checks "<<checks<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
