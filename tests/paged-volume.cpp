// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/volume.hpp"
#include <cstdlib>
#include <iostream>
#include <nlohmann/json.hpp>
namespace {thread_local int64_t fail=-1;thread_local size_t allocations=0;}
void*operator new(size_t n){++allocations;if(fail==0)throw std::bad_alloc();if(fail>0)--fail;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void*operator new[](size_t n){return ::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}
using namespace dome;
namespace {
size_t checks=0;
void check(bool b,const char*s){if(!b)throw std::runtime_error(s);++checks;std::cout<<"PASS "<<s<<'\n';}
Brush brush(Vec3 p,float r,Tool t=Tool::Clay){Brush b;b.from=b.to=p;b.radius=r;b.blend=0;b.tool=t;return b;}
void stroke(Volume&v,Brush b){v.begin_stroke();v.stamp(b);v.end_stroke();}
void faults(const Volume&source,const auto&operation,const char*label){
  auto probe=source;allocations=0;operation(probe);size_t count=allocations;
  check(count>0,"Operation exposes real allocation boundaries");
  for(size_t i=0;i<count;++i){
    auto v=source;auto owner=v.geometry_owner();auto revision=v.revision();
    auto history=v.undo_count();auto mask=v.mask_values().owner();
    std::vector<std::shared_ptr<const void>> layers;
    for(auto&l:v.layers())layers.push_back(l.delta.owner());
    bool refused=false;fail=int64_t(i);
    try{operation(v);}catch(const std::bad_alloc&){refused=true;}
    fail=-1;
    bool same=v.geometry_owner()==owner&&v.revision()==revision&&v.undo_count()==history
      &&v.in_stroke()==source.in_stroke()&&v.mask_values().owner()==mask
      &&v.layers().size()==layers.size();
    for(size_t j=0;same&&j<layers.size();++j)same=v.layers()[j].delta.owner()==layers[j];
    if(!refused||!same)throw std::runtime_error(std::string(label)+" boundary "+std::to_string(i)+"/"+std::to_string(count));
    if(v.in_stroke()){fail=0;v.cancel_stroke();fail=-1;}
  }
  std::cout<<"PASS "<<label<<" ("<<count<<" exact allocation boundaries)\n";++checks;
}
}
int main()try{
  Volume v(.04f);stroke(v,brush({},.32f));v.add_layer("Paged details");
  auto original=v.geometry_owner();auto expected_samples=v.voxels().size();
  v.begin_stroke();v.stamp(brush({.28f,0,0},.09f,Tool::Carve));
  faults(v,[](Volume&w){w.stamp(brush({-.28f,0,0},.09f,Tool::Carve));},"Paged continued stamp fails atomically at every allocation");
  faults(v,[](Volume&w){w.end_stroke();},"Paged End fails atomically at every allocation");
  v.end_stroke();faults(v,[](Volume&w){w.undo();},"Paged Undo fails atomically at every allocation");
  v.undo();faults(v,[](Volume&w){w.redo();},"Paged Redo fails atomically at every allocation");
  v.redo();
  faults(v,[&](Volume&w){w.layer_properties(w.active_layer(),"Paged details",true,false,.5f);},"Paged recomposition fails atomically at every allocation");
  auto owner=v.geometry_owner();auto settings=v.sculpt_settings();settings.symmetry_axes=7;
  v.sculpt_settings(settings);v.fill_mask(1);auto stat=sculpt_storage_stats();
  auto history=v.undo_count();auto revision=v.revision();
  v.begin_stroke();v.stamp(brush({0,0,0},.2f));bool changed=v.end_stroke();
  check(!changed&&v.geometry_owner()==owner&&v.undo_count()==history&&v.revision()==revision,"Global protection performs no geometry replacement or history edit");
  v.begin_stroke();allocations=0;fail=0;
  auto zero=brush({0,0,0},.2f);zero.pressure=0;v.stamp(zero);
  zero.pressure=1;zero.strength=0;v.stamp(zero);changed=v.end_stroke();fail=-1;
  check(!changed&&v.geometry_owner()==owner&&v.undo_count()==history&&v.revision()==revision&&allocations==0,"Zero pressure and strength perform no allocation, geometry replacement or history edit");
  v.fill_mask(0);
  check(v.mask_values().empty()&&v.geometry_owner()==owner,"Global mask clear creates no sample pages and preserves strong SDF identity");
  v.begin_stroke();auto saved=v.geometry_owner();v.stamp(brush({0,.28f,0},.09f,Tool::Carve));
  fail=0;v.cancel_stroke();fail=-1;
  check(v.geometry_owner()==saved,"Cancel restores exact paged root without allocation");
  auto cap=sculpt_storage_budget();auto before=v.encode();v.begin_stroke();
  set_sculpt_storage_budget(sculpt_storage_stats().live_bytes+256);bool denied=false;
  try{v.stamp(brush({0,-.28f,0},.09f,Tool::Carve));}catch(const std::length_error&){denied=true;}
  set_sculpt_storage_budget(cap);v.cancel_stroke();
  check(denied&&v.encode()==before,"Retained-page budget refusal leaves full native state unchanged");
  check(v.stored_samples()>=expected_samples,"Aggregate sample quota counts all paged displacement channels");
  auto quota_owner=v.geometry_owner();auto quota_revision=v.revision();
  v.begin_stroke();bool quota=false;
  try{v.stamp(brush({3,3,3},.1f),v.stored_samples());}catch(const std::length_error&){quota=true;}
  check(quota&&v.geometry_owner()==quota_owner&&v.revision()==quota_revision&&!v.pending_changes(),"Aggregate sample quota refuses a far sparse edit before publication");
  v.cancel_stroke();
  // A rooted ancestor and far branch exercise weak control-block ancestry
  // while intermediate roots expire. No raw-address identity is used.
  auto ancestor=v.geometry_owner();auto branch=v.geometry_snapshot();
  for(int i=0;i<70;++i)stroke(v,brush({float(i)*.2f-7.f,1,0},.06f));
  check(v.geometry_change(ancestor).kind==Volume::GeometryChange::Kind::Rebuild,"Expired bounded history falls back safely even with a rooted old paged owner");
  stroke(branch,brush({0,1,1},.06f));
  check(v.geometry_change(branch.geometry_owner()).kind==Volume::GeometryChange::Kind::Rebuild,"Unrelated paged ancestry cannot authenticate a dirty region");
  check(ancestor!=v.geometry_owner()&&original->size()==expected_samples,"Old SDF pages remain immutable across branches and history expiry");
  (void)stat;
  std::cout<<checks<<" paged Volume checks passed\n";
}catch(const std::exception&e){fail=-1;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
