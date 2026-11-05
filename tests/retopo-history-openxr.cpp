// SPDX-License-Identifier: GPL-3.0-or-later
#define main unused_affine_main
#include "retopo-affine.cpp"
#undef main
#include "dome/xr.hpp"
#include "../third_party/desktop/bdf_font.hpp"
#include <fstream>
#include <cstdlib>
namespace {
Document *observed=nullptr;
std::string mode;
nlohmann::json original;
nlohmann::json samples=nlohmann::json::array();
bool undo_level(unsigned n){return(n>=4&&n<=16)||(n>=20&&n<=23)||(n>=48&&n<=54);}
bool redo_level(unsigned n){return(n>=26&&n<=29)||(n>=33&&n<=36)||(n>=48&&n<=54);}
}
extern "C" void actual_submit(Document*,SpatialFrame) asm("__real__ZN4dome8Document6submitENS_12SpatialFrameE");
extern "C" void observed_submit(Document*,SpatialFrame) asm("__wrap__ZN4dome8Document6submitENS_12SpatialFrameE");
extern "C" void observed_submit(Document*d,SpatialFrame f){
  const auto step=unsigned(std::llround((f.time-1.)/.018));
  if(d==observed&&f.focused){
    check(f.hands[1].undo==undo_level(step)&&f.hands[1].redo==redo_level(step),"Actual xrSyncActions/Boolean adapter forwards current levels, including unchanged held states");
  }
  actual_submit(d,f);
  if(d!=observed)return;
  check(d->wait_input()&&d->wait_retopology(),"Real loader frame reaches actual native input and retopology workers");
  const auto s=d->frame().state;const auto vertices=s->retopology?s->retopology->topology->vertices().size():0;
  samples.push_back({{"frame",step},{"focused",f.focused},{"raw_undo",f.hands[1].undo},{"raw_redo",f.hands[1].redo},{"vertices",vertices},{"epoch",s->input_epoch},{"retopology",bool(s->retopology)},{"error",s->error}});
  if(step==6||step==11||step==16)check(vertices==2,"Held Undo does not repeat through real focus loss/regain and reference-space rebase");
  if(step==22)check(vertices==1,"Real release and second boolean press performs a second draft Undo");
  if(step==28)check(vertices==2,"Real first Redo restores one draft point");
  if(step==35)check(vertices==3,"Held Redo is suppressed; release/second press restores the third point");
  if(step>=52&&step<=54){
    if(mode=="history-apply")check(d->wait_asset(),"Terminal real Apply worker completes before stable-state observation");
    check(!s->retopology,"Actual terminal menu command consumes simultaneous raw history levels");
    if(mode=="history-cancel")check(d->copy_scene().encode()==original,"Terminal Cancel plus held history cannot target Scene history");
    else check(d->copy_scene().objects().size()==2,"Terminal Apply plus held history retains the genuine patch");
  }
}
int main(int argc,char**argv)try{
  if(argc!=4)return 2;const std::filesystem::path out=argv[2];std::filesystem::create_directories(out);mode=argv[3];
  check(mode=="history-apply"||mode=="history-cancel","Known actual loader history case");
  const auto manifest=out/"runtime.json";{std::ofstream f(manifest);f<<nlohmann::json{{"file_format_version","1.0.0"},{"runtime",{{"library_path",std::filesystem::absolute(argv[1]).string()}}}};}
  setenv("XR_RUNTIME_JSON",manifest.c_str(),1);setenv("DOME_XR_FIXTURE_RETOPO",mode.c_str(),1);
  const auto report=out/"runtime-report.json";setenv("DOME_XR_FIXTURE_REPORT",report.c_str(),1);
  constexpr Matrix m{-1.3,.1,0,0,.35,.8,.05,0,.1,.15,1.2,0,.15,-.1,.2,1};const auto s=source(m,true);original=s.encode();
  const auto fontconfig=bdf_fonts::export_config();setenv("FONTCONFIG_FILE",fontconfig.c_str(),1);
  const auto face=bdf_fonts::resolve("Unifont","Sans-Serif",16);
  Document d;d.appearance(face.file,face.pixels,false,0xff3399ff,255);d.replace(s.snapshot());check(d.wait_mesh(),"Genuine affine skinned/morphed/painted source is current");d.begin_retopology();ready(d);
  for(auto p:std::array<Vec3,3>{{{-.2f,-.15f,-.055f},{.2f,-.15f,.025f},{0,.15f,.015f}}})place(d,point(m,evaluated(p,true)));
  const auto epoch=d.frame().state->input_epoch;observed=&d;
  {XrSculpt xr(d);xr.run(65);}observed=nullptr;
  check(d.wait_asset(),"Actual loader final asset work drains");const auto final=d.copy_scene().encode();
  if(mode=="history-cancel")check(final==original,"Real Cancel leaves the full original Scene byte-exact");
  else{
    check(d.wait_asset()&&d.copy_scene().objects().size()==2,"Real loader Apply completes the owning asset worker");
    check(d.undo()&&d.copy_scene().encode()==original,"One global Undo restores original source after actual XR Apply");
    check(d.redo()&&d.copy_scene().encode()==final,"Redo restores the entire real XR patch exactly");
  }
  const auto target=out/"result.dsculpt";d.save_async(target).get();check(Scene::load(target).encode()==d.copy_scene().encode(),"Real loader result saves and reopens exactly");
  check(d.frame().state->input_epoch>epoch+1,"Actual session startup and runtime reference change advance real input epochs");
  {std::ifstream f(report);nlohmann::json j;f>>j;
    check(j.value("focus_losses",0)==1&&j.value("focus_regains",0)==1&&j.value("reference_changes",0)==1,"Real runtime emits focus loss/regain and reference-space change");
    check(j.value("history_held_queries",0)>10,"Actual Boolean provider exposes unchanged true states");
    check(j["frames_waited"]==j["frames_begun"]&&j["frames_begun"]==j["frames_ended"]&&j["acquires"]==j["releases"]&&j["swapchains_destroyed"]==2,"Real loader balances stereo frame/image/session cleanup");
  }
  {std::ofstream f(out/"observations.json");f<<samples.dump(2)<<'\n';}
  std::cout<<"PASS "<<checks<<" actual XR level/focus/rebase/terminal/history/Save checks\n";return 0;
}catch(const std::exception&e){observed=nullptr;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
