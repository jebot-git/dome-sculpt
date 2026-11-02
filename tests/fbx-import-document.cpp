// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/import-authoring.hpp"
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <unistd.h>
using namespace dome;
namespace {
size_t checks=0;
std::atomic_bool armed=false,entered=false,released=false;
std::mutex gate_mutex; std::condition_variable gate_changed;
std::string gate_path;
void check(bool ok,const char *message){if(!ok)throw std::runtime_error(message);++checks;}
struct Gate {
  explicit Gate(const std::filesystem::path &path){gate_path=std::filesystem::canonical(path).string();entered=false;released=false;armed=true;}
  void wait(){std::unique_lock lock(gate_mutex);check(gate_changed.wait_for(lock,std::chrono::seconds(15),[]{return entered.load();}),"Real native FBX source read enters the ordered worker barrier");}
  void release(){armed=false;released=true;gate_changed.notify_all();}
  ~Gate(){release();}
};
std::string bytes(const std::filesystem::path &path){std::ifstream stream(path,std::ios::binary);return {std::istreambuf_iterator<char>(stream),{}};}
void ready(Document &document){check(document.wait_asset()&&document.wait_mesh(),"Actual import and immutable render workers settle");if(!document.frame().state->error.empty())throw std::runtime_error(document.frame().state->error);}
void rejected(std::future<void> &saving){bool refused=false;try{saving.get();}catch(const std::exception&){refused=true;}check(refused,"Dependent Save propagates failed or cancelled actual import rather than capturing an old Scene");}
}
extern "C" ssize_t __real_read(int,void *,size_t);
extern "C" ssize_t __wrap_read(int fd,void *buffer,size_t size){
  if(armed.load()){
    char descriptor[64],path[4096];snprintf(descriptor,sizeof(descriptor),"/proc/self/fd/%d",fd);
    const auto count=readlink(descriptor,path,sizeof(path)-1);
    if(count>0&&std::string_view(path,size_t(count))==gate_path){
      std::unique_lock lock(gate_mutex);entered=true;gate_changed.notify_all();
      gate_changed.wait_for(lock,std::chrono::seconds(20),[]{return released.load();});
    }
  }
  return __real_read(fd,buffer,size);
}
int main(int argc,char **argv)try{
  check(argc==2,"Provide a private output directory");const std::filesystem::path out=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(out);
  const auto home=out/"home";std::filesystem::create_directories(home);
  setenv("HOME",home.c_str(),1);setenv("XDG_CONFIG_HOME",home.c_str(),1);setenv("XDG_STATE_HOME",home.c_str(),1);
  const std::filesystem::path inputs=DOME_FBX_FIXTURE_DIRECTORY;
  ImportSettings settings;settings.paint_resolution=64;ImportAuthoringOptions options;options.paint_resolution=64;
  const std::vector<std::string> files={"maya_game_sausage_7500_ascii.fbx","maya_game_sausage_7500_ascii_combined.fbx",
    "blender_293_half_skinned_7400_binary.fbx","maya_blend_shape_cube_7700_ascii.fbx","blender_279_shape_weights_7400_binary.fbx"};
  for(const auto &file:files){
    const auto path=inputs/file; const auto original_bytes=bytes(path);
    const auto expected=prepare_authoring_import(Scene{},path,options).scene.encode();
    Document document;document.replace(Scene{});ready(document);
    const auto before=document.copy_scene().encode();const auto target=out/(file+".dsculpt");
    document.save_async(target).get();const auto old=bytes(target);
    Gate gate(path);document.import_asset(path,settings);gate.wait();
    check(document.frame().state->asset_busy&&document.copy_scene().encode()==before,
          "Pending real FBX skin/morph import retains the authoritative source");
    auto saving=document.save_async(target);
    check(saving.wait_for(std::chrono::seconds(0))!=std::future_status::ready&&bytes(target)==old,
          "Accepted native FBX Save queues without blocking or replacing the previous file");
    gate.release();saving.get();ready(document);
    check(document.copy_scene().encode()==expected&&Scene::load(target).encode()==expected,
          "Actual worker and dependent Save preserve the complete converted FBX source/deformation graph");
    check(document.undo()&&document.copy_scene().encode()==before&&document.redo()&&document.copy_scene().encode()==expected,
          "Actual FBX import retains exactly one complete global Undo/Redo endpoint");
    check(bytes(path)==original_bytes,"Native FBX authoring never modifies its original source file");
  }
  const auto input=inputs/files.front(),target=out/"retained.dsculpt";
  Document document;document.replace(Scene{});ready(document);document.save_async(target).get();
  const auto before=document.copy_scene().encode();const auto old=bytes(target);
  {
    Gate gate(input);document.import_asset(input,settings);gate.wait();auto saving=document.save_async(target);
    document.cancel_asset_work();gate.release();rejected(saving);
    check(document.wait_asset()&&document.wait_mesh()&&document.copy_scene().encode()==before&&bytes(target)==old&&!document.frame().state->can_undo,
          "Cancel at the real source read retains exact old model, file and history");
  }
  {
    Gate gate(input);document.import_asset(input,settings);gate.wait();auto saving=document.save_async(target);
    Scene replacement;replacement.active().name="Replacement source";const auto expected=replacement.encode();
    document.replace(replacement.snapshot());gate.release();rejected(saving);
    check(document.wait_asset()&&document.wait_mesh()&&document.copy_scene().encode()==expected&&bytes(target)==old,
          "Old FBX completion and Save cannot overwrite a replacement-generation document or prior file");
  }
  document.replace(Scene{});ready(document);document.import_asset(input,settings);
  document.save_async(target).get();ready(document);
  check(Scene::load(target).encode()==prepare_authoring_import(Scene{},input,options).scene.encode(),
        "The actual same worker retries complete FBX import and Save after Cancel and replacement");
  std::cout<<checks<<" real FBX worker/accepted Save/cancel/generation/history checks\n";return 0;
}catch(const std::exception &e){armed=false;released=true;gate_changed.notify_all();std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
