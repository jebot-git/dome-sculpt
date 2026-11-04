// SPDX-License-Identifier: GPL-3.0-or-later
#include "native-anatomical-weights.hpp"
#include <fstream>
#include <iostream>
using namespace dome;
using namespace dome::detail;
static void check(bool x,const char*message){if(!x)throw std::runtime_error(message);}
int main(int argc,char**argv)try{
 if(argc!=3)throw std::invalid_argument("scene output directory");
 const std::filesystem::path out=argv[2];std::filesystem::create_directories(out);
 auto scene=Scene::load(argv[1]);std::vector<uint64_t>targets;uint64_t reference=0;
 for(auto&o:scene.objects()){if(o.mesh_active&&o.painted&&o.appearance)targets.push_back(o.id);if(o.deformation&&o.deformation->skeleton)reference=o.id;}
 check(reference&& !targets.empty(),"Authentic source/reference input");
 auto input=native_weight_retention(scene,256u*1024u*1024u);
 auto low=NativeAnatomicalOptions{};low.maximum_total=low.maximum_workspace+input.bytes-1;
 bool refused=false;try{auto r=capture_native_anatomical(scene,targets,reference,low);(void)r;}catch(const std::length_error&){refused=true;}
 check(refused,"Exact owning input total boundary must refuse before capture");
 auto request=capture_native_anatomical(scene,targets,reference);
 check(request.matches(scene),"Complete original lease after capture");
 bool cancelled=false;size_t calls=0;
 try{auto r=prepare_native_anatomical(request,[&]{return ++calls==3;});(void)r;}catch(const std::runtime_error&){cancelled=true;}
 check(cancelled&&calls==3&&request.matches(scene),"Real bounded preparation cancellation preserves actual input");
 const auto start=std::chrono::steady_clock::now();
 auto prepared=prepare_native_anatomical(request);
 const auto report=prepared.report();
 check(report.work<=500'000'000&&report.workspace<=256u*1024u*1024u&&report.total_admission<=512u*1024u*1024u,"Original resources and separate owning total");
 check(request.matches(scene),"Real preparation leaves complete input owner/generation/geometry lease exact");
 auto stale=scene;stale.object(targets.back()).material.roughness=.127f;
 bool stale_refused=false;try{auto copy=prepared;adopt_native_anatomical(stale,request,std::move(copy));}catch(const std::runtime_error&){stale_refused=true;}
 check(stale_refused&&stale.object(targets.back()).material.roughness==.127f,"Stale material refusal, no cached wholeScene overwrite");
 auto other_request=capture_native_anatomical(scene,targets,reference);
 bool cross=false;try{auto copy=prepared;adopt_native_anatomical(scene,other_request,std::move(copy));}catch(const std::invalid_argument&){cross=true;}
 check(cross&&request.matches(scene),"Prepared output cannot cross authenticated request identity");
 const Scene before=scene;
 check(adopt_native_anatomical(scene,request,std::move(prepared)),"Actual bounded native adoption");
 for(auto&o:scene.objects()){
   const auto&old=before.object(o.id);
   check(o.painted==old.painted&&o.volume==old.volume&&o.material_bindings==old.material_bindings&&o.appearance==old.appearance&&o.shape_source==old.shape_source&&o.polygon==old.polygon&&same_material(o.material,old.material),"Adoption must retain recipient paint/source/material/topology identity");
   if(std::find(targets.begin(),targets.end(),o.id)==targets.end())check(same_object_authority(old,o),"Readonly non-output exact");
 }
 scene.save(out/"bound.dsculpt");
 std::ofstream(out/"report.json")<<nlohmann::json{{"work",report.work},{"workspace",report.workspace},{"retained_input",report.retained_input},{"total_admission",report.total_admission},{"owners",input.owners},{"retention_work",input.work},{"changed",report.changed},{"seconds",std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()},{"cancel_callbacks",calls}}.dump(2);
 std::cout<<"PASS bounded owning factory, real Cancel/stale/cross-request/adoption checks work="<<report.work<<" retained="<<report.retained_input<<" total="<<report.total_admission<<std::endl;
 return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
