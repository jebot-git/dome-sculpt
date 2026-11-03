// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/source-paint.hpp"
#include "dome/source-maps.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <numeric>
#include <string_view>
using namespace dome;
namespace {
size_t checks=0;
void check(bool ok,const char *message){if(!ok)throw std::runtime_error(message);++checks;}
template<class F>bool refuses(F f){try{f();return false;}catch(const std::length_error &){return true;}}
Mesh quad(){Mesh m;m.positions={{0,0,0},{1,0,0},{1,1,0},{0,1,0}};m.normals.assign(4,{0,0,1});
  m.uv={{0,0},{1,0},{1,1},{0,1}};m.indices={0,1,2,0,2,3};return m;}
std::shared_ptr<PaintSourceMaps> source(){auto m=std::make_shared<PaintSourceMaps>();
  m->color.image=std::make_shared<const Image>(Image{1,1,{200,100,50,255}});return m;}
std::shared_ptr<PaintedMesh> read(const nlohmann::json &j){
  return PaintedMesh::decode(j,nullptr,false,nullptr,nullptr,SourcePaintDecodeContext{true});}
}
int main(int argc,char **argv)try {
  const std::string_view mode=argc>1?argv[1]:"all";
  if(mode!="storage") {
    // The actual old producer accepted this explicit, supported budget, then
    // its reader refused the same small record because of a different cap.
    auto original=quad();PaintSurfaceOptions policy;policy.policy=PaintUvPolicy::SourceAuthored;
    auto before=PaintedMesh::from_maps(original,source(),2048,policy);auto target=original;
    target.uv={{.1f,.1f},{.9f,.1f},{.9f,.9f},{.1f,.9f}};
    const std::array<uint32_t,4> vertices{0,1,2,3};
    check(refuses([&]{prepare_source_paint(before,target,vertices);}),"Default smaller caller budget remains enforced");
    SourcePaintOptions options;options.max_working_bytes=SourcePaintOptions::maximum_working_bytes;
    auto paint=prepare_source_paint(before,target,vertices,options);auto encoded=paint->encode();
    check(read(encoded)->encode()==encoded,"Actual 2048 original/current grids save and reopen under the existing maximum budget");
    for(int n=1;n<16;++n)paint->add_layer("Empty reserved layer");
    encoded=paint->encode();check(read(encoded)->encode()==encoded,"All sixteen metadata layers remain saveable");
    std::cout<<"PASS real2048 save/reopen and smaller caller budget\n"<<std::flush;
  }
  if(mode!="grid") {
    // Real retained coordinates and original geometry must share the legacy
    // 128 MiB paint-storage ceiling with prospective sparse tiles. Undrawn
    // vertices are legitimate source accessor rows, retained without loss.
    auto raw=quad();raw.uv={{-1,-1},{2,-1},{2,2},{-1,2}};
    raw.positions.resize(100'000,{0,0,0});raw.normals.resize(raw.positions.size(),{0,0,1});
    raw.uv.resize(raw.positions.size(),{-1,-1});auto maps=source();
    maps->uv1=std::make_shared<const std::vector<std::array<float,2>>>(raw.uv);
    maps->normal.image=std::make_shared<const Image>(Image{1,1,{128,128,255,255}});maps->normal.texcoord=1;
    PaintSurfaceOptions policy;policy.policy=PaintUvPolicy::SourceAuthored;
    auto before=PaintedMesh::from_maps(raw,maps,1024,policy);auto target=raw;
    target.uv.assign(target.positions.size(),{0,0});auto corners=quad().uv;
    std::copy(corners.begin(),corners.end(),target.uv.begin());
    std::vector<uint32_t> vertices(raw.positions.size());std::iota(vertices.begin(),vertices.end(),0);
    auto paint=prepare_source_paint(before,std::move(target),vertices);
    Brush brush;brush.from=brush.to={.5f,.5f,0};brush.normal={0,0,1};brush.radius=2;
    PaintSettings ink;ink.color={.2f,.7f,.3f,1};ink.front_only=false;
    size_t accepted=0;bool denied=false;
    for(size_t n=0;n<16;++n) {
      if(n)paint->add_layer("Actual full-coverage paint");
      const auto baseline=paint->encode();paint->begin_stroke();
      try{paint->stamp(brush,ink);}catch(const std::length_error &){
        denied=true;paint->cancel_stroke();
        check(paint->encode()==baseline,"Failed prospective stroke restores the complete owner and all earlier paint");
        check(read(baseline)->encode()==baseline,"Last accepted multi-layer owner actually reopens after refusal");
        break;
      }
      check(paint->end_stroke(),"Full-coverage source stroke changes real tiles");++accepted;
      check(paint->storage_bytes()<=128*1024*1024,"Accepted source paint includes retained storage in its ceiling");
    }
    check(denied&&accepted>1,"Real layered paint reaches the retained-storage boundary");
    check(paint->undo()&&paint->redo(),"Accepted history remains usable after quota refusal");
    const auto last=paint->encode();check(read(last)->encode()==last,"History-restored owner remains saveable");
    std::cout<<"PASS retained-source quota, whole-stamp rollback and restart after "<<accepted<<" accepted layers\n";
  }
  std::cout<<"PASS "<<checks<<" source-paint resource checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
