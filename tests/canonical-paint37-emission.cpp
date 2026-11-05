// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/emission-native.hpp"
#include "canonical-paint-sampling.hpp"
#include <fstream>
#include <iostream>
using namespace dome;
namespace {size_t checks=0;void check(bool b,const char*s){++checks;if(!b)throw std::runtime_error(s);std::cout<<"PASS "<<s<<std::endl;}std::vector<uint8_t> bytes(const std::filesystem::path&p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}void settle(Document&d){if(!d.wait_asset()||!d.wait_mesh()||!d.frame().state->error.empty())throw std::runtime_error(d.frame().state->error);}}
int main(int argc,char**argv)try{
 if(argc!=4)return 2;const auto source=Scene::load(argv[1]);const auto original=source.encode();const std::filesystem::path out=argv[2];const bool before=std::string_view(argv[3])=="--expect-old-refusal";std::filesystem::create_directories(out);
 const auto &o=source.active();check(o.painted&&o.painted->has_physical_emission()&&o.material_bindings&&o.material_bindings->canonical_paint_sampling&&o.material_bindings->canonical_attributes,"Actual native37 input owns physical paint and independent canonical normal/current-key sampling");
 Document d;d.replace(source);settle(d);d.edit_scene("Package4 live history",[](Scene&s,const auto&){s.active().name="Canonical physical current endpoint";});settle(d);const auto live=d.copy_scene().encode();const auto state=d.frame().state;const auto path=out/"current.glb",saved=out/"queued.dsculpt";{std::ofstream f(path,std::ios::binary);f<<"canonical-paint37-HDR-sentinel";}const auto sentinel=bytes(path);
 auto export_job=d.export_asset(path,AssetExportFormat::Glb);auto save=d.save_async(saved);bool refused=false;std::string message;try{export_job.get();}catch(const std::exception&e){refused=true;message=e.what();}save.get();
 if(before){std::cout<<"ORIGINAL_PUBLIC_REFUSAL "<<message<<std::endl;check(refused&&message.find("Retained HDR paint export")!=std::string::npos,"Original public physical export genuinely refuses its unsupported retained domain");check(bytes(path)==sentinel,"Original refusal preserves prior output file exactly");}
 else{check(!refused,"Genuine package4 public fileworker export succeeds");auto restored=extract_native_emission(bytes(path));check(restored&&restored->encode()==live,"Physical owning package restores exact Scene/float/control/normal/key sources");}
 settle(d);check(Scene::load(saved).encode()==live&&d.copy_scene().encode()==live&&d.frame().state->can_undo==state->can_undo&&d.frame().state->can_redo==state->can_redo,"Following queued Save and complete live/history state remain exact");d.undo();settle(d);check(d.copy_scene().encode()==original,"Export preserves exact existing Undo endpoint");d.redo();settle(d);check(d.copy_scene().encode()==live,"Export preserves exact existing Redo endpoint");check(source.encode()==original,"Public physical worker never mutates caller's complete native source");std::cout<<"PASS "<<checks<<" canonical paint37 physical worker controls\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
