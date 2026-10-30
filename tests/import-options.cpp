// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/humanoid-creator.hpp"
#include "dome/import-source.hpp"
#include "import-controller-driver.hpp"
#include "import-authoring-fixtures.hpp"
#include "dome/material-bindings.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <unistd.h>
using namespace dome;
namespace fs=std::filesystem;
namespace {
size_t checks=0;std::atomic_bool gate=false,entered=false,release_read=false;
std::mutex gate_mutex;std::condition_variable gate_changed;
void check(bool value,const char *message){if(!value)throw std::runtime_error(message);++checks;std::cout<<"PASS "<<message<<'\n';}
template<class F>void refuses(F f,const char *message){bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed,message);}
template<class F>void wait(F f){auto until=std::chrono::steady_clock::now()+std::chrono::seconds(8);while(!f()){if(std::chrono::steady_clock::now()>until)throw std::runtime_error("Import state timed out");std::this_thread::sleep_for(std::chrono::milliseconds(2));}}
void write(const fs::path &p,std::string_view text){std::ofstream file(p,std::ios::binary);file.write(text.data(),text.size());if(!file)throw std::runtime_error("Fixture write failed");}
const char *triangle="o triangle\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
}
extern "C" ssize_t __real_read(int,void*,size_t);
extern "C" ssize_t __wrap_read(int fd,void *buffer,size_t count){
  if(gate.load()){
    char path[64],name[4096];snprintf(path,sizeof(path),"/proc/self/fd/%d",fd);auto n=readlink(path,name,sizeof(name)-1);
    if(n>0){name[n]=0;if(std::string_view(name,size_t(n)).ends_with("/cancel.obj")){
      entered=true;gate_changed.notify_all();std::unique_lock lock(gate_mutex);gate_changed.wait_for(lock,std::chrono::seconds(3),[]{return release_read.load();});
    }}
  }return __real_read(fd,buffer,count);
}
int main(int argc,char **argv){try{
  fs::path base=fs::absolute(argc>1?argv[1]:"import-options");fs::create_directories(base);
  auto pattern=(base/"case-XXXXXX").string();std::vector<char> name(pattern.begin(),pattern.end());name.push_back(0);
  auto created=mkdtemp(name.data());if(!created)throw std::runtime_error("Cannot create isolated import fixture");
  fs::path root=created;std::cout<<"Fixture "<<root<<'\n';fs::create_directories(root/"nested");
  write(root/"model.obj",triangle);write(root/"ignored.txt","not an asset");
  fs::create_symlink("missing.obj",root/"dangling.obj");
  auto listed=scan_import_directory(root);check(listed.entries.size()==2&&listed.entries[0].directory&&listed.entries[1].name=="model.obj","bounded native browser filters assets and sorts folders first");
  check(!listed.error.empty(),"unreadable directory entries diagnosed without hiding readable models");
  refuses([&]{scan_import_directory(root,[]{return true;});},"browser cancellation is independent of source authoring");
  ImportSettings settings;settings.paint_resolution=64;settings.validate();settings.paint_resolution=63;
  refuses([&]{settings.validate();},"shared native/XR paint resolution validation");settings.paint_resolution=64;
  setenv("HOME",root.c_str(),1);Document doc;Scene original;doc.replace(original);check(doc.wait_mesh(),"empty document worker ready");
  auto baseline=doc.copy_scene().encode();auto revision=doc.frame().state->revision;
  doc.configure_import(settings);ImportDriver hand(doc);hand.open();
  std::cout<<"Navigation open "<<doc.frame().state->menu_page<<"/"<<doc.frame().state->menu_index<<'\n';
  hand.sector(8);std::cout<<"Navigation objects "<<doc.frame().state->menu_page<<"/"<<doc.frame().state->menu_index<<'\n';
  hand.sector(9);std::cout<<"Navigation mesh "<<doc.frame().state->menu_page<<"/"<<doc.frame().state->menu_index<<'\n';
  hand.sector(10);std::cout<<"Navigation import "<<doc.frame().state->menu_page<<"/"<<doc.frame().state->menu_index<<" error="<<doc.frame().state->error<<'\n';
  check(doc.frame().state->menu_page==21,"actual controller Mesh10 cell10 enters Import21");
  wait([&]{auto v=doc.frame().state->importing;return v&&!v->busy;});auto initial_view=doc.frame().state->importing;
  check(initial_view->entries.size()==2 && initial_view->settings.paint_resolution==64,"existing file worker publishes immutable browser listing and shared defaults");
  hand.sector(2);check(doc.frame().state->importing->selected==1 && initial_view->selected==0,"controller selection publishes a new immutable view");
  hand.sector(5);check(doc.frame().state->menu_page==23,"Import21 options opens coordinated page23");
  for(int i=0;i<3;++i)hand.sector(1);hand.sector(2);hand.sector(3);hand.sector(4);
  auto configured=doc.frame().state->importing;
  check(configured->settings.choices.units==ImportUnits::Millimetres && configured->settings.choices.axes==ImportAxes::YUp &&
      configured->settings.choices.normals==ImportNormals::GenerateFlat && configured->settings.paint_resolution==128,"controller shares real units/axes/normals/paint-size options");
  hand.sector(5);check(!doc.frame().state->importing->settings.choices.materials,"controller exposes explicit material import choice");hand.sector(5);
  for(int i=0;i<5;++i)hand.sector(0);check(doc.frame().state->importing->settings.choices.format==ImportSourceFormat::Automatic,"controller format choices cycle back to filename inference");
  auto before_custom=doc.frame().state->importing;
  hand.sector(6);check(doc.frame().state->controls.parameter==VrParameter::ImportUnitScale&&doc.frame().state->controls.logarithmic,"custom unit scale uses logarithmic spatial control");
  hand.ray(.4f);hand.panel_button(false);
  check(doc.frame().state->importing->settings==before_custom->settings,"Cancel discards unconfirmed custom unit scale exactly");
  hand.sector(6);hand.ray(float(std::log(.02/1e-12)/std::log(1e24)));hand.panel_button(true);
  auto accepted_options=doc.frame().state->importing->settings;
  check(accepted_options.choices.units==ImportUnits::Custom&&std::abs(accepted_options.choices.custom_metres_per_unit-.02)<1e-7,"pressure confirmation stages a custom metres-per-unit value");
  check(doc.copy_scene().encode()==baseline && doc.frame().state->revision==revision && !doc.frame().state->can_undo,"browsing and staged options perform no geometry/history mutation");
  hand.sector(8);check(doc.frame().state->menu_page==21,"ImportOptions23 Back returns to browser21");
  hand.sector(6);check(doc.wait_asset()&&doc.wait_mesh(),"actual controller Import applies on ordered background asset worker");
  auto imported=doc.copy_scene();check(imported.objects().size()==2&&doc.frame().state->error.empty(),"native OBJ options publish complete editable object");
  const auto &object=imported.objects().back();
  const auto &position=object.painted->mesh().positions[1];auto physical=ob::transform(imported.world_transform(object.id),{position.x,position.y,position.z});
  check(std::abs(physical.x-.02)<1e-7&&object.painted->resolution()==128&&object.appearance->source_receipt->options==accepted_options.choices,
      "applied custom units and paint size match retained exact options receipt");
  auto accepted=imported.encode();check(Scene::decode(accepted).encode()==accepted,"controller chosen options survive source-preserving Scene11 restart");
  check(doc.undo()&&doc.copy_scene().encode()==baseline&&!doc.frame().state->can_undo,"one Undo restores exact scene before controller import");
  check(doc.redo()&&doc.copy_scene().encode()==accepted,"one Redo restores exact imported source and options");
  auto view_before_cancel=doc.frame().state->importing;doc.import_action(SpatialCommand::ImportUnits);doc.import_action(SpatialCommand::ImportCancel);
  check(!doc.frame().state->importing && doc.import_settings()==accepted_options,"browser Cancel discards staged options and retains last accepted defaults");
  check(doc.copy_scene().encode()==accepted,"browser Cancel has no source/history side effects");
  doc.cancel_spatial_menu();doc.open_import_browser(root);doc.import_action(SpatialCommand::ImportCancel);
  std::this_thread::sleep_for(std::chrono::milliseconds(25));check(!doc.frame().state->importing,"stale directory completion cannot reopen a canceled import browser");
  doc.open_import_browser(root/"missing");wait([&]{auto v=doc.frame().state->importing;return v&&!v->busy;});
  check(!doc.frame().state->importing->error.empty()&&doc.copy_scene().encode()==accepted,"directory failure is actionable and leaves the source scene intact");
  doc.import_action(SpatialCommand::ImportCancel);doc.replace(original);check(doc.wait_mesh(),"cancellation test source ready");baseline=doc.copy_scene().encode();
  std::string large=triangle;for(int i=0;i<20000;++i)large+="v 0 0 1\n";write(root/"cancel.obj",large);
  entered=false;release_read=false;gate=true;doc.import_asset(root/"cancel.obj",settings);wait([]{return entered.load();});doc.cancel_asset_work();
  release_read=true;gate=false;gate_changed.notify_all();check(doc.wait_asset()&&doc.copy_scene().encode()==baseline&&!doc.frame().state->can_undo,"Cancel during captured source read publishes no partial objects or history");
  doc.import_asset(root/"cancel.obj",settings);check(doc.wait_asset()&&doc.frame().state->error.empty()&&doc.copy_scene().objects().size()==2,"same source and options retry successfully after cancellation");
  check(doc.undo()&&doc.copy_scene().encode()==baseline,"retried import remains exactly one Undo");
  std::string japanese;for(int i=0;i<25;++i)japanese+="日本語";
  write(root/"unicode.mtl","newmtl "+japanese+"\nKd 1 0 0\n");
  write(root/"unicode.obj","mtllib unicode.mtl\no status\nv 0 0 0\nv 1 0 0\nv 0 1 0\nusemtl "+japanese+"\nf 1 2 3\n");
  doc.import_asset(root/"unicode.obj",settings);check(doc.wait_asset()&&doc.frame().state->error.empty(),"native Unicode material source imports on the same asset worker");
  auto status=nlohmann::json(doc.frame().state->message).dump();
  check(!status.empty()&&doc.frame().state->message.find("日本語")!=std::string::npos&&doc.undo()&&doc.copy_scene().encode()==baseline,"bounded Unicode import diagnostics retain complete UTF-8 scalars and exact Undo");
  const auto flat_directory=root/"flat-source";fs::create_directories(flat_directory);
  auto flat_fixture=import_fixture::Fixture{};
  auto &flat_primitive=flat_fixture.j["meshes"][0]["primitives"][0];
  flat_primitive["attributes"].erase("NORMAL");flat_primitive["targets"][0].erase("NORMAL");
  flat_fixture.j["materials"][0]["normalTexture"]={{"index",0},{"scale",.8f}};
  flat_fixture.save(flat_directory/"flat.glb");
  doc.import_action(SpatialCommand::ImportCancel);doc.cancel_spatial_menu();
  hand.open();hand.sector(8);hand.sector(9);hand.sector(10);
  ImportSettings flat_settings;flat_settings.paint_resolution=64;
  doc.open_import_browser(flat_directory);wait([&]{auto v=doc.frame().state->importing;return v&&!v->busy;});
  doc.configure_import(flat_settings);
  check(doc.frame().state->menu && doc.frame().state->menu_page==21 &&
      doc.frame().state->importing->entries.size()==1 && doc.frame().state->importing->settings==flat_settings,
      "Actual controller navigation opens browser21 with the captured missing-normal glTF source and shared defaults");
  hand.sector(6);check(doc.wait_asset()&&doc.wait_mesh()&&doc.frame().state->error.empty(),"Actual controller Import applies flat-frame preparation on the shared asset worker");
  auto flat_scene=doc.copy_scene();const auto &flat_object=flat_scene.objects().back();
  check(flat_scene.objects().size()==3 && !flat_object.appearance->asset->primitives[0].normals_authored &&
      flat_object.painted->mesh().positions.size()==6 && flat_object.material_bindings->normal_frames->pbr->generated_tangents,
      "Controller import preserves original normal absence and publishes genuine corner-split flat/Mikk frame authority");
  auto flat_encoded=flat_scene.encode();check(doc.undo()&&doc.copy_scene().encode()==baseline &&
      doc.redo()&&doc.copy_scene().encode()==flat_encoded && doc.undo()&&doc.copy_scene().encode()==baseline,
      "Controller flat import has exact single-operation Undo/Redo with no partial source adoption");
  doc.import_action(SpatialCommand::ImportCancel);
  auto limit=root/"limit";fs::create_directories(limit);for(int i=0;i<513;++i)write(limit/(std::to_string(i)+".obj"),triangle);
  refuses([&]{scan_import_directory(limit);},"directory result quota refuses instead of silently omitting eligible assets");
  check(spatial_page_count==25 && spatial_max_page==24 && !spatial_action(spatial_page_count,5) && spatial_action(21,6)==SpatialCommand::ImportApply&&spatial_action(23,0)==SpatialCommand::ImportFormat&&spatial_parameter(23,6)==VrParameter::ImportUnitScale&&
      spatial_action(0,4)==SpatialCommand::Undo&&spatial_parameter(17,1)==VrParameter::CutOffset,"Import21/23 preserve root Undo and existing cut controls");
  std::cout<<checks<<" import options/controller checks passed\n";return 0;
}catch(const std::exception&e){release_read=true;gate_changed.notify_all();std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
