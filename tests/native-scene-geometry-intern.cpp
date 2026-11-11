// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/scene.hpp"
#include "native-scene-geometry-intern.hpp"
#include <nlohmann/json.hpp>
#include <array>
#include <barrier>
#include <exception>
#include <iostream>
#include <optional>
#include <thread>
using namespace dome;
namespace {
size_t checks=0;
void check(bool value,const char*message){if(!value)throw std::runtime_error(message);++checks;}
bool same(const Volume&a,const Volume&b){const auto x=a.geometry_owner(),y=b.geometry_owner();return x==y&&!x.owner_before(y)&&!y.owner_before(x);}
Scene fixture(){
 Volume::Voxels values;values.emplace(Int3{-1,0,0},-.03f);values.emplace(Int3{0,0,0},.05f);values.emplace(Int3{1,0,0},.12f);
 const auto field=Volume::from_samples(.1f,std::move(values));Scene scene;scene.active().volume=std::make_shared<Volume>(field);scene.add("Native duplicate",field);scene.select(1);return scene;
}
bool rejects_after_first_volume(const nlohmann::json&wire){
 auto invalid=wire;invalid["objects"][1]["material"]["alpha"]=99;
 try{Scene::decode(invalid);}catch(const std::runtime_error&e){return std::string(e.what()).find("material alpha")!=std::string::npos;}
 return false;
}
void native_scene_boundaries(const nlohmann::json&wire,const nlohmann::json&volume){
 const auto first=Scene::decode(wire),second=Scene::decode(wire);
 check(first.encode()==wire&&second.encode()==wire,"Independent native scene reads retain every encoded field");
 check(same(*first.object(1).volume,*first.object(2).volume)&&same(*second.object(1).volume,*second.object(2).volume),"Each actual native Scene read shares its own verified duplicate fields");
 check(!same(*first.object(1).volume,*second.object(1).volume),"Independent native Scene reads do not borrow a global pool owner");
 check(rejects_after_first_volume(wire),"Malformed second object rejects after a valid first Volume entered its scope");
 const auto a=Volume::decode(volume),b=Volume::decode(volume);
 check(!same(a,b),"Exception restores the absent Scene scope before standalone Volume reads");
 check(a.encode()==volume&&b.encode()==volume,"Standalone native Volume reads retain complete authoring after the exception");
}
void nested_scopes(const nlohmann::json&wire,const nlohmann::json&volume){
 {
  detail::NativeSceneGeometryIntern outer;
  const auto before=Volume::decode(volume);
  {
   detail::NativeSceneGeometryIntern inner;
   const auto inside=Volume::decode(volume),again=Volume::decode(volume);
   check(same(inside,again)&&!same(before,inside),"Nested actual Volume reads use their own current scoped pool");
   const auto scene=Scene::decode(wire);
   check(!same(inside,*scene.object(1).volume),"An actual nested Scene read owns a further independent pool");
   const auto after_scene=Volume::decode(volume);
   check(same(inside,after_scene),"Nested Scene destruction restores the enclosing Volume-read pool");
  }
  const auto restored=Volume::decode(volume);
  check(same(before,restored),"Nested scope destruction restores the original pool owner");
  check(rejects_after_first_volume(wire),"Malformed actual nested Scene rejects after its first valid Volume");
  const auto after_error=Volume::decode(volume);
  check(same(before,after_error),"Nested Scene exception restores the live outer pool rather than dropping it");
 }
 const auto a=Volume::decode(volume),b=Volume::decode(volume);
 check(!same(a,b),"Final scope destruction restores independent standalone reads");
}
void threads(const nlohmann::json&volume){
 std::array<std::optional<Volume>,2>a,b;std::array<std::exception_ptr,2>errors;
 std::barrier together(2);
 auto worker=[&](size_t index){try{
  detail::NativeSceneGeometryIntern scope;
  together.arrive_and_wait();a[index]=Volume::decode(volume);
  together.arrive_and_wait();b[index]=Volume::decode(volume);
 }catch(...){errors[index]=std::current_exception();together.arrive_and_drop();}};
 std::thread first(worker,0),second(worker,1);first.join();second.join();
 for(const auto&e:errors)if(e)std::rethrow_exception(e);
 check(a[0]&&a[1]&&b[0]&&b[1],"Both concurrent native read scopes complete");
 check(same(*a[0],*b[0])&&same(*a[1],*b[1]),"Each simultaneous thread retains its own genuine repeated Volume owner");
 check(!same(*a[0],*a[1]),"Simultaneous thread-local scopes never join each other's native owners");
 check(a[0]->encode()==volume&&a[1]->encode()==volume,"Concurrent pooling preserves all native Volume fields");
}
}
int main()try{
 const auto scene=fixture();const auto wire=scene.encode(),volume=scene.object(1).volume->encode();
 native_scene_boundaries(wire,volume);nested_scopes(wire,volume);threads(volume);
 std::cout<<"PASS "<<checks<<" native Scene geometry scope lifetime controls\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
