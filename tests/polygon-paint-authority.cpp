// SPDX-License-Identifier: GPL-3.0-or-later
#include "polygon-fixture.hpp"
#include "polygon-paint.hpp"
#include "dome/material-bindings.hpp"
#include <bit>
#include <iostream>
using namespace dome;
namespace {unsigned checks=0;void check(bool v,const char*m){if(!v)throw std::runtime_error(m);++checks;std::cout<<"PASS "<<m<<'\n';}template<class F>bool refuses(F f,std::string_view diagnostic={}){try{f();return false;}catch(const std::exception&e){if(!diagnostic.empty()&&std::string_view(e.what()).find(diagnostic)==std::string_view::npos)throw std::runtime_error(std::string("Wrong refusal: ")+e.what());return true;}}
void changed_float(nlohmann::json &buffer){auto &bytes=buffer.get_binary();uint32_t word=uint32_t(bytes[0])|uint32_t(bytes[1])<<8|uint32_t(bytes[2])<<16|uint32_t(bytes[3])<<24;float value=std::bit_cast<float>(word)+.0001f;word=std::bit_cast<uint32_t>(value);for(unsigned i=0;i<4;++i)bytes[i]=uint8_t(word>>(8*i));}}
int main()try{
 auto scene=polygon_test::fixture();PaintSurfaceOptions options;options.policy=PaintUvPolicy::PolygonAuthored;
 scene.active().painted=polygon_paint_geometry(*scene.active().painted,scene.active().painted->mesh(),options);scene.validate();auto encoded=scene.encode();
 check(encoded["version"]==12&&Scene::decode(encoded).encode()==encoded,"Valid Scene12 authenticates current polygon paint against exact active REST/index/UV authority");
 auto candidate=scene.snapshot();candidate.active().polygon.reset();check(refuses([&]{candidate.validate();})&&refuses([&]{candidate.encode();}),"Programmatic unbound paint is refused before encoding or publication");
 candidate=scene.snapshot();candidate.active().mesh_active=false;check(refuses([&]{candidate.validate(false);}),"Cheap publication admission also rejects dormant current polygon paint");
 auto bad=encoded;bad["objects"][0]["polygon"]=nullptr;bad["polygon_sources"]["authorities"]=nlohmann::json::array();check(refuses([&]{Scene::decode(bad);},"authenticated Scene12"),"Serialized unbound current paint is refused before SurfaceMap exposure");
 bad=encoded;bad["polygon_sources"]["authorities"][0]["active"]=false;check(refuses([&]{Scene::decode(bad);},"inactive current authority"),"Inactive authority cannot authenticate current editable paint");
 bad=encoded;bad["objects"][0]["mesh_active"]=false;check(refuses([&]{Scene::decode(bad);},"authenticated Scene12"),"Serialized dormant paint cannot borrow an otherwise active witness");
 for(unsigned version=1;version<12;++version){bad=encoded;bad["version"]=version;bad.erase("polygon_sources");for(auto &o:bad["objects"])o.erase("polygon");if(!refuses([&]{Scene::decode(bad);}))throw std::runtime_error("Older Scene silently admitted polygon paint policy");}
 check(true,"All older Scene versions refuse polygon paint even when polygon fields are stripped");
 bad=encoded;changed_float(bad["objects"][0]["painted"]["mesh"]["positions"]);check(refuses([&]{Scene::decode(bad);},"editable topology"),"Same-count changed REST coordinates fail exact authority authentication");
 bad=encoded;changed_float(bad["objects"][0]["painted"]["mesh"]["uv"]);check(refuses([&]{Scene::decode(bad);},"editable topology"),"Sub-tolerance changed UV bytes fail exact authority authentication");
 bad=encoded;auto &indices=bad["objects"][0]["painted"]["mesh"]["indices"].get_binary();for(unsigned k=0;k<4;++k)std::swap(indices[k],indices[k+4]);check(refuses([&]{Scene::decode(bad);},"editable topology"),"Same-size reordered triangle indices cannot impersonate N-gon corner authority");
 auto origin=std::make_shared<PolygonOrigin>();origin->volume=std::make_shared<const Volume>(scene.active().volume->geometry_snapshot());origin->paint=scene.active().painted->snapshot();origin->polygon=scene.active().polygon;origin->mesh_active=true;origin->name="Retained prior N-gon source";
 auto empty=std::make_shared<PolygonAuthority>();empty->topology=PolygonMesh::from_faces({},{});empty->origin=origin;
 candidate=scene.snapshot();candidate.active().polygon=empty;candidate.active().painted.reset();candidate.active().mesh_active=false;candidate.validate();auto archived=candidate.encode();
 check(Scene::decode(archived).encode()==archived&&candidate.surfaces().empty(),"Active empty authority suppresses Volume while a prior active witness retains full polygon paint in the bounded origin chain");
 bad=archived;bad["polygon_sources"]["origins"][0]["polygon"]=nullptr;check(refuses([&]{Scene::decode(bad);},"Retained polygon-authored"),"Retained origin paint requires its own direct active authority witness");
 bad=archived;bad["polygon_sources"]["origins"][0]["mesh_active"]=false;check(refuses([&]{Scene::decode(bad);},"Retained polygon-authored"),"Dormant retained paint cannot borrow another owner's active witness");
 bad=archived;changed_float(bad["polygon_sources"]["paints"][0]["mesh"]["uv"]);check(refuses([&]{Scene::decode(bad);},"editable topology"),"Retained origin exact UV authentication applies independently of current object paint");
 auto invalid_origin=std::make_shared<PolygonOrigin>(*origin);invalid_origin->polygon.reset();auto invalid_empty=std::make_shared<PolygonAuthority>(*empty);invalid_empty->origin=invalid_origin;candidate.active().polygon=invalid_empty;check(refuses([&]{candidate.validate(false);}),"Programmatic retained unbound paint is refused in cheap Scene admission");
 auto binding=std::make_shared<SourceMaterialBindings>();binding->has_mtoon=false;auto uv=std::make_shared<SourceUvOrigin>();uv->paint=polygon_test::fixture().active().painted;binding->uv_origin=uv;binding->validate(false);uv->paint=scene.active().painted;check(refuses([&]{binding->validate(false);},"Source UV origin paint"),"Source UV origins cannot infer polygon authority from an aliased current paint");
 bad=encoded;bad["source_materials"]["paints"].push_back(encoded["objects"][0]["painted"]);check(refuses([&]{Scene::decode(bad);},"Source UV origins"),"Serialized material-origin paint refuses polygon policy before source archive decoding");
 check(refuses([&]{PaintedMesh::decode(encoded["objects"][0]["painted"]);}),"Existing standalone two-argument paint decoder remains an authority boundary");
 std::cout<<"PASS "<<checks<<" polygon paint authority checks\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
