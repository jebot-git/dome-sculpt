// SPDX-License-Identifier: GPL-3.0-or-later
#include "native-surface-heat.hpp"
#include <fstream>
#include <iostream>
using namespace dome;using namespace dome::detail;
int main(int argc,char**argv)try{
 if(argc!=3)return 2;const std::filesystem::path out=argv[2];std::filesystem::create_directories(out);auto scene=Scene::load(argv[1]);auto before=scene.encode();uint64_t reference=0;std::vector<uint64_t>ids;
 for(const auto&o:scene.objects())if(o.mesh_active&&o.painted&&o.appearance){ids.push_back(o.id);if(o.deformation&&o.deformation->skeleton){if(reference)throw std::runtime_error("One actual fitted unweighted native reference required");reference=o.id;}}
 if(ids.size()!=19||!reference||scene.object(reference).deformation->skin)throw std::runtime_error("Actual genuine unskinned nineteen-part repository fixture required");
 auto request=capture_native_surface_heat(scene,ids,reference);auto prepared=prepare_native_surface_heat(request);const auto report=prepared.report();if(!request.matches(scene)||scene.encode()!=before||report.changed!=19||report.work>500'000'000||report.workspace>256u*1024u*1024u||report.total_admission>512u*1024u*1024u)throw std::runtime_error("Owning heat source/workspace/work/total proof");
 if(!adopt_native_surface_heat(scene,request,std::move(prepared)))throw std::runtime_error("Genuine owning new-bind must change all nineteen recipients");
 for(auto id:ids){const auto&o=scene.object(id);if(!o.deformation||!o.deformation->skin||!o.rig_source)throw std::runtime_error("Actual complete new-bind rig/skin/first origin required");}
 scene.save(out/"bound.dsculpt");auto loaded=Scene::load(out/"bound.dsculpt");if(loaded.encode()!=scene.encode())throw std::runtime_error("Actual owning output Save/reopen exact");
 nlohmann::json parts=nlohmann::json::array();for(const auto&p:report.parts)parts.push_back({{"object",p.object},{"work",p.work},{"workspace",p.workspace},{"closed",p.closed},{"nonembedded",p.nonembedded},{"residual",p.residual},{"partition",p.partition}});
 std::ofstream(out/"report.json")<<nlohmann::json{{"parts",ids.size()},{"reference",reference},{"work",report.work},{"workspace",report.workspace},{"retained_input",report.retained_input},{"total_admission",report.total_admission},{"changed",report.changed},{"records",parts},{"original_skin_values_in_energy",false},{"scope","Actual owning new-bind factory/native Scene and persistence; no Document/public method/UI admission"}}.dump(2);std::cout<<"PASS actual owning unskinned nineteen-part bind work="<<report.work<<" workspace="<<report.workspace<<" total="<<report.total_admission<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
