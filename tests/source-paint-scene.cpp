// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/source-paint.hpp"
#include "dome/scene.hpp"
#include "dome/source-maps.hpp"
#include "dome/material-bindings.hpp"
#include "dome/polygon-scene.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
using namespace dome;
namespace {
size_t checks=0;
void check(bool ok,const char *message){if(!ok)throw std::runtime_error(message);++checks;std::cout<<"PASS "<<message<<std::endl;}
template<class F>bool refuses(F f){try{f();return false;}catch(const std::exception &e){std::cout<<"EXPECTED "<<e.what()<<std::endl;return true;}}
std::shared_ptr<PaintedMesh> source(bool painted) {
  Mesh raw;raw.positions={{0,0,0},{1,0,0},{1,1,0},{0,1,0}};raw.normals.assign(4,{0,0,1});
  raw.uv={{-1.25f,-.5f},{1.75f,-.5f},{1.75f,2.5f},{-1.25f,2.5f}};raw.indices={0,1,2,0,2,3};
  auto maps=std::make_shared<PaintSourceMaps>();maps->color.image=std::make_shared<const Image>(Image{2,2,
    {255,0,0,255,0,255,0,255,0,0,255,255,255,255,255,255}});
  maps->color.min_filter=9986;maps->color.mag_filter=9728;maps->color.wrap_s=33648;
  PaintSurfaceOptions policy;policy.policy=PaintUvPolicy::SourceAuthored;
  auto before=PaintedMesh::from_maps(raw,maps,64,policy);
  const std::vector<uint32_t> vertices{0,1,2,0,2,3};Mesh target;
  for(auto v:vertices){target.positions.push_back(raw.positions[v]);target.normals.push_back(raw.normals[v]);}
  target.indices={0,1,2,3,4,5};target.uv={{.05f,.05f},{.45f,.05f},{.45f,.45f},{.55f,.55f},{.95f,.95f},{.55f,.95f}};
  auto result=prepare_source_paint(before,target,vertices);
  if(painted){Brush brush;brush.from=brush.to={.5f,.5f,0};brush.normal={0,0,1};brush.radius=.35f;
    PaintSettings ink;ink.color={.1f,.8f,.3f,.6f};ink.front_only=false;
    result->begin_stroke();result->stamp(brush,ink);if(!result->end_stroke())throw std::runtime_error("Fixture must contain actual paint");}
  return result;
}
}
int main(int argc,char **argv)try {
  const auto directory=argc==2?std::filesystem::path(argv[1]):std::filesystem::temp_directory_path()/"source-paint-scene";
  std::filesystem::create_directories(directory);
  for(bool painted:{false,true}) {
    auto owned=source(painted);const auto before=owned->source_sampling_original()->encode();
    for(int root=0;root<4;++root) {
      Scene scene;scene.active().mesh_active=true;
      scene.active().painted=root==0?owned:std::make_shared<PaintedMesh>(owned->mesh(),64);
      if(root==1) {
        auto origin=std::make_shared<SourceUvOrigin>();origin->paint=owned;
        auto binding=std::make_shared<SourceMaterialBindings>();binding->has_mtoon=false;binding->uv_origin=origin;
        scene.active().material_bindings=std::move(binding);
      }
      if(root>=2) {
        auto origin=std::make_shared<PolygonOrigin>();origin->paint=owned;origin->mesh_active=true;
        origin->volume=scene.active().volume;origin->material=scene.active().material;origin->name="Actual retained source-paint capsule";
        if(root==2)scene.active().shape_source=origin;
        else scene.active().rig_source=origin;
      }
      const auto encoded=scene.encode();check(encoded.at("version")==28,
        "Actual current/material/shape/rig source paint determines Scene28 even with empty layers");
      auto reopened=Scene::decode(encoded);check(reopened.encode()==encoded,
        "Complete owning Scene28 current and retained source states restart exactly");
      check(reopened.snapshot().encode()==encoded,"Immutable Scene snapshot preserves the owning source-paint graph");
      for(int old:{5,18,26,27}){auto downgraded=encoded;downgraded["version"]=old;
        check(refuses([&]{Scene::decode(downgraded);}),"Changing only the Scene header cannot grant source-paint capability");}
      const auto path=directory/(std::to_string(root)+(painted?"-painted":"-empty")+".dsculpt");scene.save(path);
      check(Scene::load(path).encode()==encoded,"Actual native CBOR Save/load preserves source ownership and all current bytes");
      check(owned->source_sampling_original()->encode()==before,"Serialization does not mutate the original source authority");
    }
  }
  Scene old;const auto old_bytes=old.encode();auto fake=old_bytes;fake["version"]=28;
  check(refuses([&]{Scene::decode(fake);}),"Scene28 without actual owning feature refuses");
  check(Scene::decode(old_bytes).encode()==old_bytes,"Ordinary legacy native Scene bytes remain unchanged");
  std::cout<<"PASS "<<checks<<" source-paint Scene data checks; renderer/UI/export adapters remain unfinished\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
