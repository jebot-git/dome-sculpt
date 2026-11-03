// SPDX-License-Identifier: GPL-3.0-or-later
#include "source-node-extras.hpp"
#include "dome/avatar.hpp"
#include <iostream>
#include <cstdlib>
#include <new>
using Json=nlohmann::json;
namespace {thread_local long cut=-1;thread_local bool active=false;thread_local size_t allocations=0,hits=0;size_t checks=0;}
void*operator new(size_t n){if(active){++allocations;if(cut==0){++hits;throw std::bad_alloc();}if(cut>0)--cut;}if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}void*operator new[](size_t n){return ::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}
namespace {
void check(bool value,const char*why){if(!value)throw std::runtime_error(why);++checks;}
Json initial(){return {{"name","Source"},{"extras",{{"artist",{{"unicode","腕"},{"array",{1,2,3}}}},{"domeSculptHumanoid",{{"version",1},{"humanBones",{{"hips",{{"node",0}}},{"head",{{"node",2}}}}}}}}}};}
template<class F>void rejected(F f,const char*why){try{f();}catch(const std::exception&){++checks;return;}throw std::runtime_error(why);}
}
int main()try{
 (void)dome::human_roles();const std::array<int,3>map{9,11,13};const auto old=initial();auto node=old;dome::detail::remap_source_node_extras(node,map);check(node.at("extras").at("artist")==old.at("extras").at("artist"),"Unrelated source extras remain exact");check(node.at("extras").at("domeSculptHumanoid").at("humanBones").at("head").at("node").get<int>()==13&&node.at("extras").at("domeSculptHumanoid").at("humanBones").at("hips").at("node").get<int>()==9,"Known native role bindings use actual original-to-emitted node IDs");
 for(auto extras:std::vector<Json>{nullptr,Json::array({1,2}),"opaque",Json{{"artist",7}}}){Json opaque={{"extras",extras}};const auto before=opaque;dome::detail::remap_source_node_extras(opaque,map);check(opaque==before,"Unknown/non-object extras are not rewritten or discarded");}
 std::vector<Json>bad{nullptr,Json::array(),Json{{"version",2},{"humanBones",Json::object()}},Json{{"version",1.0},{"humanBones",Json::object()}},Json{{"version",1},{"humanBones",Json::object()},{"future",1}},Json{{"version",1},{"humanBones",{{"head",{{"node",3}}}}}},Json{{"version",1},{"humanBones",{{"head",{{"node",-1}}}}}},Json{{"version",1},{"humanBones",{{"head",{{"node",1.5}}}}}},Json{{"version",1},{"humanBones",{{"head",{{"node",1},{"future",1}}}}}},Json{{"version",1},{"humanBones",{{"futureRole",{{"node",0}}}}}}};
 for(auto value:bad){node=old;node["extras"]["domeSculptHumanoid"]=value;const auto before=node;rejected([&]{dome::detail::remap_source_node_extras(node,map);},"Unknown or malformed typed hint must refuse");check(node==before,"Typed-hint refusal preserves exact node and opaque extras");}
 node=old;rejected([&]{dome::detail::remap_source_node_extras(node,std::array<int,3>{9,11,-1});},"A source reference without an emitted node must refuse");check(node==old,"Missing emitted target preserves old node");
 size_t visits=0;node=old;dome::detail::remap_source_node_extras(node,map,[&]{++visits;return false;});for(size_t boundary=1;boundary<=visits;++boundary){size_t now=0;node=old;rejected([&]{dome::detail::remap_source_node_extras(node,map,[&]{return ++now>=boundary;});},"Every measured Cancel boundary must refuse");check(node==old,"Cancel preserves source node exactly before commit");}
 node=old;cut=-1;allocations=0;active=true;dome::detail::remap_source_node_extras(node,map);active=false;const size_t measured=allocations;
 for(size_t boundary=0;boundary<measured;++boundary){node=old;cut=long(boundary);allocations=hits=0;active=true;bool refused=false;try{dome::detail::remap_source_node_extras(node,map);}catch(const std::bad_alloc&){refused=true;}catch(...){active=false;throw;}active=false;cut=-1;check(refused&&hits,"Every measured persistent denial produces actual bad_alloc");check(node==old,"Persistent denial preserves typed hints and all source fields exactly");}
 std::cout<<checks<<" source-node extras checks; cancellation="<<visits<<" persistent="<<measured<<'\n';return 0;
}catch(const std::exception&e){active=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
