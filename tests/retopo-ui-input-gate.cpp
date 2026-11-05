// SPDX-License-Identifier: GPL-3.0-or-later
// Linked ONLY into the separately named Release test app. Observe the actual
// independent input worker before its real EndGesture/finalization/unwrap.
#include "dome/retopo-authoring.hpp"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <thread>
using namespace dome;
extern "C" void real_edit(RetopoWorkspace &,RetopoAction,Vec3,float,const std::function<bool()> &)
 asm("__real__ZN4dome15RetopoWorkspace4editENS_12RetopoActionENS_4Vec3EfRKSt8functionIFbvEE");
extern "C" void wrapped_edit(RetopoWorkspace &,RetopoAction,Vec3,float,const std::function<bool()> &)
 asm("__wrap__ZN4dome15RetopoWorkspace4editENS_12RetopoActionENS_4Vec3EfRKSt8functionIFbvEE");
extern "C" void wrapped_edit(RetopoWorkspace &w,RetopoAction action,Vec3 p,float distance,const std::function<bool()> &cancelled){
 if(action==RetopoAction::EndGesture)if(auto directory=std::getenv("DOME_RETOPO_UI_INPUT_GATE")){
   const std::filesystem::path root(directory);
   if(std::filesystem::exists(root/"arm")){
     std::filesystem::rename(root/"arm",root/"entered");
     const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(120);
     while(!std::filesystem::exists(root/"release")){
       if(std::chrono::steady_clock::now()>=deadline)throw std::runtime_error("Test-only real retopo input gate timeout");
       std::this_thread::sleep_for(std::chrono::milliseconds(5));
     }
   }
 }
 real_edit(w,action,p,distance,cancelled);
}
