// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "baked-normal-field.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
using namespace dome;
namespace {
void require(bool value,const char *message){if(!value)throw std::runtime_error(message);}
void settled(Document &document){require(document.wait_asset(std::chrono::seconds(120))&&document.wait_mesh(std::chrono::seconds(120))&&document.frame().state->error.empty(),("Canonical paint fixture: "+document.frame().state->error).c_str());}
}
int main(int argc,char **argv)try{
 if(argc!=3)return 2;
 const std::filesystem::path sources=argv[1],out=argv[2];std::filesystem::create_directories(out);
 nlohmann::json manifest=nlohmann::json::array();
 for(int variant:{0,7})for(bool keyed:{false,true}){
  const auto input=sources/("original-"+std::to_string(variant)+".dsculpt");auto scene=Scene::load(input);const auto before=scene.encode();scene.active().transform.scale=5;
  Document document;document.replace(scene);document.shading(false);settled(document);document.convert_baked_normals();settled(document);
  if(keyed){
   ShapeKeyEdit key;key.action=ShapeKeyAction::Create;key.name="Canonical fixture current key";document.author_shape(key);settled(document);
   key.action=ShapeKeyAction::Replace;key.target=document.frame().state->morph_target;key.positions=document.copy_scene().active().painted->mesh().positions;key.positions.at(0).z+=.01f;document.author_shape(key);settled(document);
  }
  document.edit_scene("Fixture physical material",[](Scene &current,const std::function<bool()> &cancelled){if(cancelled&&cancelled())throw std::runtime_error("Fixture cancelled");current.active().material.emissive={8,.016f,.16f};});settled(document);
  const auto surface=document.frame().surface;require(surface&&!surface->ranges.empty(),"Actual converted fixture publishes a visible native stroke target");const auto &range=surface->ranges.front();const auto a=surface->mesh.positions.at(surface->mesh.indices.at(range.first)),b=surface->mesh.positions.at(surface->mesh.indices.at(range.first+1)),c=surface->mesh.positions.at(surface->mesh.indices.at(range.first+2));
  const auto point=a*.3f+b*.3f+c*.4f,normal=normalized(cross(b-a,c-a));PaintSettings ink;ink.front_only=false;ink.channel=PaintChannel::Emission;ink.emission_units=PaintEmissionUnits::Physical;ink.emission={12,.002f,3};document.paint_settings(ink);
  NativeStrokeSample sample;sample.origin=point+normal;sample.direction=normal*-1;sample.brush.radius=.2f;sample.brush.strength=.73f;sample.brush.blend=.2f;
  require(document.begin_native_stroke()&&document.sample_native_stroke(sample),"Actual native controller/pointer stroke reaches the converted current-key source");document.end_native_stroke();require(document.wait_native_stroke(std::chrono::seconds(120)),"Actual native physical gesture completes");settled(document);
  const auto result=document.copy_scene();const auto &object=result.active();require(object.material_bindings&&object.material_bindings->canonical_attributes&&object.painted->baked()&&object.painted->baked()->normal_field&&object.painted->has_physical_emission(),"Fixture retains separate genuine canonical normal and editable FLOAT paint");require(!object.material_bindings->canonical_paint_sampling,"Fixture is genuine pre-UV data, not a fabricated owning37 node");if(keyed)require(bool(object.material_bindings->key_derivation),"Current key comes from the actual native authoring transaction");
  const std::string stem=std::to_string(variant)+"-"+std::to_string(keyed?(variant?4:3):0);const auto file=out/(stem+".dsculpt");document.save_async(file).get();require(Scene::load(file).encode()==result.encode(),"Physical fixture native Save reopens the exact current source/normal/paint graph");require(Scene::load(input).encode()==before,"Fixture generation leaves original source file exact");manifest.push_back({{"file",file.filename().string()},{"input",input.filename().string()},{"curved_skin_morph",variant==7},{"new_key_before_uv",keyed},{"native_physical_stroke",true}});
 }
 std::ofstream(out/"fixtures.json")<<manifest.dump(2)<<'\n';std::cout<<"PASS 4 actual native converted-key FLOAT paint fixtures\n";return 0;
}catch(const std::exception &error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
