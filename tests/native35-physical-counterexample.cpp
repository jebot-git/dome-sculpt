// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/emission-native.hpp"
#include "dome/gltf-export.hpp"
#include "baked-normal-field.hpp"
#include <fstream>
#include <iostream>
using namespace dome;
namespace {
void settle(Document &d){if(!d.wait_asset()||!d.wait_mesh()||!d.frame().state->error.empty())throw std::runtime_error(d.frame().state->error.empty()?"Document worker did not settle":d.frame().state->error);}
std::vector<uint8_t> bytes(const std::filesystem::path&p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
void paint(Document &doc){doc.edit_scene("Actual canonical physical brush",[](Scene &s,const std::function<bool()> &cancelled){if(cancelled&&cancelled())throw std::runtime_error("Canonical physical brush cancelled");auto &o=s.active();o.material.unlit=false;o.material.emissive={8,.016f,.16f};o.painted=o.painted->snapshot();auto &p=*o.painted;p.add_layer("Actual canonical float detail");PaintSettings ink;ink.front_only=false;ink.channel=PaintChannel::Emission;ink.emission_units=PaintEmissionUnits::Physical;ink.emission={12,.002f,3};Brush b;b.normal={0,0,1};const auto &m=p.mesh();b.from=b.to=(m.positions.at(m.indices.at(0))+m.positions.at(m.indices.at(1))+m.positions.at(m.indices.at(2)))/3;b.radius=.2f;b.strength=.73f;p.begin_stroke();p.stamp(b,ink);if(!p.end_stroke())throw std::runtime_error("Actual canonical physical brush was empty");p.layer_properties("Actual canonical float detail",true,.63f);ink.channel=PaintChannel::Mask;ink.mask=.4f;b.radius=.1f;p.begin_stroke();p.stamp(b,ink);if(!p.end_stroke())throw std::runtime_error("Actual canonical physical mask was empty");});settle(doc);}
void uv(Document &doc){doc.begin_uv();if(!doc.wait_uv())throw std::runtime_error("UV workspace did not settle");doc.edit_uv(UvAction::Scale,.8f);if(!doc.wait_uv()||!doc.frame().state->uv->valid)throw std::runtime_error("Actual UV draft invalid");doc.end_uv(true);settle(doc);}
}
int main(int argc,char **argv)try{
 if(argc!=3)return 2;std::filesystem::path source=argv[1],out=argv[2];std::filesystem::create_directories(out);nlohmann::json report=nlohmann::json::array();bool failed=false;
 for(int variant:{0,7})for(int operation:{0,1,2,3,4}){nlohmann::json row={{"variant",variant},{"operation",operation}};std::string stage="load";try{
  auto s=Scene::load(source/("original-"+std::to_string(variant)+".dsculpt"));s.active().transform.scale=5;Document doc;doc.replace(s);doc.shading(false);settle(doc);stage="real-normal-conversion";doc.convert_baked_normals();settle(doc);auto converted=doc.copy_scene();if(!converted.active().material_bindings->canonical_attributes||!converted.active().painted->baked()->normal_field)throw std::runtime_error("Missing genuine normal conversion owner");
  // Establish a same-input ordinary finite-view positive before float edits.
  stage="before-physical-export";const auto initial=converted.encode();const auto control=out/(std::to_string(variant)+"-"+std::to_string(operation)+"-control.glb");doc.export_asset(control,AssetExportFormat::EditableGlb256).get();settle(doc);if(doc.copy_scene().encode()!=initial)throw std::runtime_error("Control export changed source");row["control_export"]=true;
  if(operation==1||operation==4){stage="uv-before-physical";uv(doc);}
  stage="actual-physical-brush";paint(doc);
  if(operation==3||operation==4){stage="actual-key-create";ShapeKeyEdit key;key.name="Canonical physical key";doc.author_shape(key);settle(doc);key.action=ShapeKeyAction::Replace;key.target=doc.frame().state->morph_target;key.positions=doc.copy_scene().active().painted->mesh().positions;key.positions.at(0).z+=.01f;stage="actual-key-replace";doc.author_shape(key);settle(doc);const auto keyed=doc.copy_scene();if(!keyed.active().material_bindings->key_derivation)throw std::runtime_error("Key command omitted genuine derivation owner");row["key_owner"]=true;}
  if(operation==2){stage="physical-before-uv";uv(doc);}
  stage="owning-save-restart";const auto scene=doc.copy_scene();const auto exact=scene.encode();const auto native=out/(std::to_string(variant)+"-"+std::to_string(operation)+".dsculpt");doc.save_async(native).get();if(Scene::load(native).encode()!=exact)throw std::runtime_error("Physical native restart differs");row["native_roundtrip"]=true;row["scene_version"]=exact.at("version");
  stage="actual-physical-export";const auto emitted=out/(std::to_string(variant)+"-"+std::to_string(operation)+".glb");doc.export_asset(emitted,AssetExportFormat::Glb).get();settle(doc);auto recovered=extract_native_emission(bytes(emitted));if(!recovered||recovered->encode()!=exact||doc.copy_scene().encode()!=exact)throw std::runtime_error("Actual HDR export did not preserve full native owner graph");row["physical_export"]=true;
 }catch(const std::exception &e){row["failure_stage"]=stage;row["diagnostic"]=e.what();failed=true;std::cout<<"COUNTEREXAMPLE "<<row.dump()<<std::endl;}report.push_back(row);std::ofstream f(out/"report.json");f<<report.dump(2)<<'\n';}
 std::cout<<report.dump(2)<<std::endl;return failed?1:0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
