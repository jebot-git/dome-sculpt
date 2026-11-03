// SPDX-License-Identifier: GPL-3.0-or-later
// Native sparse paint data only; linear GPU preview and Scene27 graph adoption
// are separate unfinished adapters and are explicitly refused in this phase.
#include "dome/scene.hpp"
#include <bit>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
namespace {
thread_local int64_t deny=-1;
thread_local bool observe=false,hit=false;
thread_local size_t allocations=0;
void allocating(){if(observe)++allocations;if(deny==0){hit=true;throw std::bad_alloc();}if(deny>0)--deny;}
}
void*operator new(size_t n){allocating();if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void*operator new[](size_t n){return ::operator new(n);}
void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}
void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}
using namespace dome;
namespace {
size_t checks=0;
void check(bool value,const char *message){++checks;if(!value)throw std::runtime_error(message);}
template<class F>bool rejects(F f){try{f();}catch(const std::exception&){return true;}return false;}
Mesh quad(){Mesh m;m.positions={{-.5f,-.5f,0},{.5f,-.5f,0},{.5f,.5f,0},{-.5f,.5f,0}};m.normals.assign(4,{0,0,1});m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.indices={0,1,2,0,2,3};return m;}
Brush brush(){Brush b;b.from=b.to={0,0,0};b.normal={0,0,1};b.radius=2;b.strength=.9995f;return b;}
PaintSettings physical(std::array<float,3> rgb){PaintSettings p;p.channel=PaintChannel::Emission;p.emission_units=PaintEmissionUnits::Physical;p.emission=rgb;return p;}
auto decode(const nlohmann::json &j){return PaintedMesh::decode(j,nullptr,false,nullptr,nullptr,true);}
void write(const std::filesystem::path &p,const nlohmann::json &j){auto bytes=nlohmann::json::to_cbor(j);std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());if(!f)throw std::runtime_error("Paint fixture write failed");}
}
int main(int argc,char **argv)try {
  if(argc!=2)return 2;std::filesystem::path output=argv[1];std::filesystem::create_directories(output);
  static_assert(sizeof(PaintPixel)==14);static_assert(sizeof(PaintTile)==32*32*14);
  auto seed=std::make_shared<PaintedMesh>(quad(),64);const auto legacy=seed->encode();write(output/"legacy.cbor",legacy);
  check(!legacy.contains("version")&&!seed->has_physical_emission(),"Ordinary paint retains its legacy root layout");
  auto paint=seed->snapshot();auto settings=physical({12,.0003f,9999});
  paint->begin_stroke();paint->stamp(brush(),settings);check(paint->end_stroke(),"Physical paint commits actual sparse linear data");
  auto value=paint->emission_pixel(paint->active_layer(),32,32);
  check(value.physical==settings.emission&&value.legacy==std::array<float,3>{},"HDR and tiny linear values are not encoded into clipped sRGB bytes");
  check(value.coverage>0&&value.coverage<1&&paint->pixel(paint->active_layer(),32,32).data[12]==255,"Physical coverage retains a real residual erased by UNORM8");
  const double x=(32.5/64)-.5,edge=1-(x*x+x*x)/4;
  const double expected=edge*edge*brush().strength;
  check(std::abs(value.coverage-expected)<2e-7,"Physical coverage matches an independent world brush footprint");
  const auto encoded=paint->encode();check(encoded.at("version")==3&&encoded.at("domain").is_null(),"Genuine physical data owns a conditional Paint3 envelope");
  write(output/"physical.cbor",encoded);
  check(rejects([&]{PaintedMesh::decode(encoded);}),"Legacy standalone decode refuses physical Paint3");
  check(decode(nlohmann::json::from_cbor(nlohmann::json::to_cbor(encoded)))->encode()==encoded,"Physical floats and source mesh survive exact CBOR restart");
  check(paint->undo()&&paint->encode()==legacy,"Physical stroke has one exact Undo");
  check(paint->redo()&&paint->encode()==encoded,"Physical stroke has exact Redo");
  auto relative=PaintSettings{};relative.channel=PaintChannel::Emission;relative.emission={.2f,.6f,.8f};
  auto weaker=brush();weaker.strength=.25f;paint->begin_stroke();paint->stamp(weaker,relative);check(paint->end_stroke(),"Legacy-relative edits remain distinct on an existing physical tile");
  auto mixed=paint->emission_pixel(paint->active_layer(),32,32);
  check(mixed.legacy[1]>0&&mixed.physical[2]>1000,"Mixed relative and physical coefficients both remain owned");
  auto mixed_json=paint->encode();paint->begin_stroke();auto erase=settings;erase.erase=true;paint->stamp(weaker,erase);check(paint->pending_changes(),"Fractional physical erase changes actual owned coverage");paint->cancel_stroke();
  check(paint->encode()==mixed_json,"Cancel restores exact physical, common and source data");
  paint->layer_properties("Hidden HDR",false,0);const auto hidden=paint->encode();
  check(hidden.at("version")==3&&decode(hidden)->encode()==hidden,"Hidden zero-opacity HDR remains reachable and exact");
  auto zero=seed->snapshot();zero->begin_stroke();zero->stamp(brush(),physical({0,0,0}));check(zero->end_stroke()&&zero->has_physical_emission(),"Authored absolute zero retains genuine units and float coverage");
  check(zero->encode().at("version")==3,"Authored zero cannot be downgraded by a nonzero-value heuristic");
  // Real evaluated-space stamping shares the exact original REST/UV domain.
  auto posed=std::make_shared<Mesh>(quad());for(auto &p:posed->positions){p.x=2*p.x+.7f;p.z+=.4f;}
  auto evaluated=seed->snapshot();auto owner=evaluated->mesh_owner();auto evaluation=evaluated->evaluate(posed);
  auto positioned=brush();positioned.from=positioned.to={.7f,0,.4f};evaluated->begin_stroke();evaluated->stamp_evaluated(positioned,settings,*evaluation);check(evaluated->end_stroke(),"Physical stroke uses actual evaluated-world positions");
  check(evaluated->mesh_owner()==owner&&evaluated->emission_pixel(evaluated->active_layer(),32,32).physical==settings.emission,"Evaluated physical painting preserves immutable REST/source ownership");
  // Measured cancellation points include the final post-computation check.
  auto baseline=seed->snapshot();baseline->begin_stroke();size_t polls=0;baseline->stamp(brush(),settings,[&]{++polls;return false;});baseline->cancel_stroke();check(polls>=4,"Physical stamp exposes measured cancellation boundaries");
  for(size_t boundary:{size_t(0),polls/2,polls-1}){auto q=seed->snapshot();q->begin_stroke();size_t at=0;bool cancelled=false;
    try{q->stamp(brush(),settings,[&]{return at++==boundary;});}catch(const std::runtime_error&){cancelled=true;}q->cancel_stroke();
    check(cancelled&&q->encode()==legacy&&q->mesh_owner()==seed->mesh_owner(),"Cancellation preserves exact paint and source at measured boundaries");}
  // Host allocation denial applies only to real detached stamp work. It stays
  // persistent through unwind and is disabled before diagnostics/retry.
  auto probe=seed->snapshot();probe->begin_stroke();allocations=0;observe=true;probe->stamp(brush(),settings);observe=false;const auto count=allocations;probe->cancel_stroke();check(count>0,"Physical stamp makes genuine measured allocations");
  for(size_t boundary=0;boundary<count;++boundary){auto q=seed->snapshot();q->begin_stroke();hit=false;bool failed=false;deny=int64_t(boundary);
    try{q->stamp(brush(),settings);}catch(const std::bad_alloc&){failed=true;}deny=-1;q->cancel_stroke();
    check(hit&&failed&&q->encode()==legacy&&q->mesh_owner()==seed->mesh_owner(),"Persistent stamp allocation failure preserves exact data/source");
    q->begin_stroke();q->stamp(brush(),settings);check(q->end_stroke()&&q->encode()==encoded,"Same-source retry is exact after allocation resumes");}
  auto malformed=encoded;malformed["payload"]["layers"][0]["physical_emission_tiles"]=nlohmann::json::array();check(rejects([&]{decode(malformed);}),"An empty physical declaration does not create HDR authority");
  malformed=encoded;malformed["payload"]["version"]=3;check(rejects([&]{decode(malformed);}),"Nested paint envelopes refuse instead of recursing");
  malformed=encoded;auto &data=malformed["payload"]["layers"][0]["physical_emission_tiles"][0]["data"].get_binary();data[24]=data[25]=data[26]=data[27]=0;check(rejects([&]{decode(malformed);}),"Tampered float coverage/common fallback pair refuses");
  check(rejects([]{PaintedMesh::decode({{"version",3},{"payload",nlohmann::json::object()},{"domain",nullptr}});}),"Old capability refuses even empty Paint3 before geometry");
  check(rejects([&]{zero->composite(Material{});}),"Unimplemented RGBA8 preview is explicitly refused rather than clipping physical data");
  Scene scene;scene.active().mesh_active=true;scene.active().painted=zero;
  check(rejects([&]{scene.encode();}),"Scene adoption remains refused until the complete Scene27 closure exists");
  std::cout<<"Physical paint data: "<<checks<<" checks; "<<polls<<" cancellation polls; "<<count<<" persistent allocation boundaries\n";return 0;
}catch(const std::exception&e){deny=-1;observe=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
