// SPDX-License-Identifier: GPL-3.0-or-later
#include "canonical-paint-sampling.hpp"
#include "canonical-paint-archive.hpp"
#include "baked-normal-field.hpp"
#include "dome/scene.hpp"
#include <filesystem>
#include <iostream>
#include <numeric>
using namespace dome;
namespace {
size_t checks=0;
void check(bool b,const char *why){++checks;if(!b)throw std::runtime_error(why);}
template<class F>bool refuses(F f){try{f();return false;}catch(const std::exception &){return true;}}
std::pair<Mesh,std::vector<uint32_t>> target(const Mesh &before,bool split){
 Mesh out;std::vector<uint32_t> map;
 if(split)for(auto v:before.indices){map.push_back(v);out.positions.push_back(before.positions[v]);out.normals.push_back(before.normals[v]);out.uv.push_back(before.uv[v]);out.indices.push_back(uint32_t(out.indices.size()));}
 else{out=before;map.resize(out.positions.size());std::iota(map.begin(),map.end(),0);}
 for(auto &uv:out.uv){const auto v=uv;uv={.5f-.7f*(v[1]-.5f),.5f+.7f*(v[0]-.5f)};}
 return {out,map};
}
bool layers(const PaintedMesh&a,const PaintedMesh&b){
 if(a.resolution()!=b.resolution()||a.layers().size()!=b.layers().size()||a.active_layer()!=b.active_layer())return false;
 for(size_t i=0;i<a.layers().size();++i){const auto &x=a.layers()[i],&y=b.layers()[i];if(x.id!=y.id||x.name!=y.name||x.visible!=y.visible||x.opacity!=y.opacity||x.tiles!=y.tiles||x.pbr_tiles!=y.pbr_tiles||x.emission_tiles!=y.emission_tiles)return false;}
 return true;
}
Scene retain(const Scene &source,bool split){
 auto [mesh,map]=target(source.active().painted->mesh(),split);
 auto result=detail::CanonicalPaintSampling::create(source,source.active().id,mesh,map);
 check(layers(*source.active().painted,*result.paint)&&source.active().painted->baked()==result.paint->baked(),"Factory preserves all actual sparse values and baked field");
 Scene next=source;next.active().painted=result.paint;next.active().material_bindings=result.binding;next.active().deformation=result.deformation;
 next.validate();return next;
}
void check_owners(const Scene &scene){
 const auto &o=scene.active();const auto p=o.material_bindings->canonical_paint_sampling;
 check(p&&p->valid_owner(p)&&p->matches_current(*o.painted),"Decoded current Paint must bind its own replayed attachment");
 check(p->before()->paint&&p->before()->material_bindings&&p->before()->volume,"Owning node retains complete actual before control state");
 check(p->normal_uv()==o.material_bindings->canonical_uv&&p->normal_uv()->origin()==o.material_bindings->canonical_attributes,"Independent normal role retains genuine current/root owners");
 check(o.painted->paint_sampling_domain()==p->attachment()->domain(),"Current grid uses genuine decoded writable domain");
 p->validate_current(*o.painted);check(true,"Complete decoded replay validates");
}
}
int main(int argc,char **argv)try{
 if(argc!=3)return 2;const auto source=Scene::load(argv[1]);const auto original=source.encode();const std::filesystem::path out=argv[2];std::filesystem::create_directories(out);
 for(bool split:{false,true}){
  auto retained=retain(source,split);check_owners(retained);auto encoded=retained.encode();
  check(encoded.at("version")==37&&encoded.at("source_materials").at("version")==19,"Only real owning node writes Scene37/material19");
  check(encoded.contains("canonical_paint")&&!encoded.at("canonical_paint").at("nodes").empty(),"Owning archive emits actual before/normal/paint references");
  const auto path=out/(split?"split.dsculpt":"same.dsculpt");retained.save(path);auto decoded=Scene::load(path);check_owners(decoded);
  check(decoded.encode()==encoded,"Native save/reopen preserves exact owning graph/bytes/aliases");
  auto second=retain(decoded,false);check_owners(second);const auto twice=second.encode();auto replay=Scene::decode(twice);check_owners(replay);check(replay.encode()==twice,"Repeated UV retains complete acyclic before roots and exact decoded aliases");
  const auto &a=*decoded.active().material_bindings->canonical_paint_sampling->attachment()->domain()->basis();
  const auto &b=*replay.active().material_bindings->canonical_paint_sampling->attachment()->domain()->basis();
  check(a.uv==b.uv&&a.indices==b.indices,"Repeated current UV never rerasterizes original writable coordinates");
  auto wrong=encoded;wrong["version"]=35;check(refuses([&]{Scene::decode(wrong);}),"Old outer context refuses new owning pool");
  wrong=encoded;wrong["source_materials"]["version"]=17;check(refuses([&]{Scene::decode(wrong);}),"Old material context cannot infer new owner");
  wrong=encoded;wrong.erase("canonical_paint");check(refuses([&]{Scene::decode(wrong);}),"Bare version37 without real node fails closed");
  wrong=encoded;wrong["canonical_paint"]["nodes"][0]["before_origin"]=9999;check(refuses([&]{Scene::decode(wrong);}),"Foreign before origin cannot borrow a structural domain");
  wrong=encoded;wrong["canonical_paint"]["nodes"].push_back(wrong["canonical_paint"]["nodes"][0]);check(refuses([&]{Scene::decode(wrong);}),"Unreachable owning nodes refuse");
 }
 check(source.encode()==original,"All codec operations retain exact caller source state");std::cout<<"PASS "<<checks<<" canonical paint37 owning codec checks\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
