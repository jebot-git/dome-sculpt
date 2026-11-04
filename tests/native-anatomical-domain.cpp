// SPDX-License-Identifier: GPL-3.0-or-later
#include "native-anatomical-weights.hpp"
#include <iostream>
using namespace dome;using namespace dome::detail;
int main(int argc,char**argv)try{
 if(argc!=4)throw std::invalid_argument("authentic unskinned source907 full-field907 output");auto scene=Scene::load(argv[1]);const auto expected=Scene::load(argv[2]);uint64_t reference=0;std::vector<uint64_t>original;
 for(auto&o:scene.objects()){if(o.deformation&&o.deformation->skeleton)reference=o.id;if(o.mesh_active&&o.painted&&o.appearance)original.push_back(o.id);}if(original.size()!=19||!reference)throw std::invalid_argument("Actual CC0 cohort required by this fixture only");
 size_t checks=0;auto check=[&](bool b,const char*s){if(!b)throw std::runtime_error(s);++checks;std::cout<<"PASS "<<s<<std::endl;};
 auto collect=[](const Scene&s,std::span<const uint64_t>ids){weight_surface::Budget b{0,1000000,{}};auto d=native_anatomical_domain(s,ids,b);return d;};
 auto equal=[](auto a,auto b){std::sort(a.begin(),a.end());std::sort(b.begin(),b.end());return a==b;};
 check(equal(collect(scene,original).objects,original),"Actual original complete current source remains exact");
 const auto original_groups=scene.groups();std::vector<SculptObject>copies;for(auto id:original)copies.push_back(scene.object(id));
 const auto container=scene.add_group("User collection");for(const auto&g:original_groups)if(!g.parent)scene.group(g.id).parent=container;
 std::set<uint64_t>needed;for(const auto&o:copies)for(const auto&g:scene.group_path(o.group))if(g.id!=container)needed.insert(g.id);
 std::map<uint64_t,uint64_t>groups;for(const auto&g:original_groups)if(needed.contains(g.id)){auto t=g.transform;if(!g.parent)t.position.x+=7;auto id=scene.add_group("Independent copy "+g.name,t,g.parent?groups.at(g.parent):container,g.humanoid);groups[g.id]=id;scene.group(id).source_placement=g.source_placement;scene.group(id).affine_edit=g.affine_edit;}
 std::vector<uint64_t>foreign;for(auto old:copies){auto id=scene.add("Instance copy",Volume{});old.id=id;old.group=groups.at(old.group);old.painted=old.painted->snapshot();old.volume=std::make_shared<Volume>(old.volume->geometry_snapshot());scene.object(id)=std::move(old);foreign.push_back(id);}scene.select(original.front());scene.validate();
 check(equal(collect(scene,original).objects,original),"Common user parent cannot merge two authentic current instances");
 check(equal(collect(scene,foreign).objects,foreign),"The second current instance independently owns its complete support scope");
 auto refuse=[&](const Scene&s,std::span<const uint64_t>ids){try{auto d=collect(s,ids);(void)d;return false;}catch(const std::invalid_argument&){return true;}};
 const std::array cross{original.front(),foreign.front()};check(refuse(scene,cross),"Cross-instance selected outputs refuse without borrowed siblings");
 auto missing=scene;missing.erase(original.back());check(refuse(missing,std::span<const uint64_t>(original.data(),1)),"A complete foreign instance cannot fill a missing local primitive");
 auto duplicate=scene;auto extra=duplicate.duplicate(original.back());duplicate.object(extra).group=duplicate.object(original.back()).group;check(refuse(duplicate,std::span<const uint64_t>(original.data(),1)),"Ambiguous extra same-primitive copy inside scope refuses");
 auto forged=scene;auto appearance=std::make_shared<CreatorAppearance>(*forged.object(original.back()).appearance);appearance->source_primitive=forged.object(original.front()).appearance->source_primitive;appearance->source_mesh=forged.object(original.front()).appearance->source_mesh;forged.object(original.back()).appearance=appearance;check(refuse(forged,std::span<const uint64_t>(original.data(),1)),"Matching counts cannot authenticate a fabricated primitive pairing");
 auto before=scene;auto request=capture_native_anatomical(scene,original,reference);auto prepared=prepare_native_anatomical(request);check(request.matches(scene),"Actual scoped solve preserves every current input owner and value lease");auto report=prepared.report();check(adopt_native_anatomical(scene,request,std::move(prepared)),"Actual mixed-project scoped owning publication succeeds");
 for(auto id:original)check(scene.object(id).deformation->encode(scene.object(id).painted->mesh())==expected.object(id).deformation->encode(expected.object(id).painted->mesh()),"Original instance emits exact independently sealed907 anatomical field");
 for(auto id:foreign)check(same_object_authority(before.object(id),scene.object(id))&&before.object(id).painted==scene.object(id).painted&&before.object(id).volume==scene.object(id).volume,"Foreign instance stays readonly with exact mutable wrapper identities");
 check(report.work<=500000000&&report.workspace<=256u*1024u*1024u&&report.total_admission<=512u*1024u*1024u,"Mixed-project retained ownership obeys unchanged owning/work/workspace caps");
 scene.save(argv[3]);std::cout<<"Native anatomical current-domain checks "<<checks<<" work="<<report.work<<" total="<<report.total_admission<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
