// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/vrm-authoring.hpp"
#include "dome/import-authoring.hpp"
#include "dome/humanoid-creator.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
using namespace dome;
namespace {
size_t checks=0;void check(bool v,const char*t){if(!v)throw std::runtime_error(t);++checks;std::cout<<"PASS "<<t<<std::endl;}
std::vector<uint8_t> bytes(const std::filesystem::path&p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
void ready(Document &doc){check(doc.wait_mesh(std::chrono::seconds(120))&&doc.frame().state->error.empty(),("Actual source mesh: "+doc.frame().state->error).c_str());}
}
int main(int argc,char**argv)try{
 if(argc!=4||std::string_view(argv[3])!="--old-refusal")return 2;const std::filesystem::path source=argv[1],out=argv[2];std::filesystem::create_directories(out);Scene empty;empty.active().visible=false;ImportAuthoringOptions options;options.prepare_texture_atlas=true;options.paint_resolution=64;auto imported=prepare_authoring_import(empty,source,options);check(imported.objects.size()==3,"Actual original source retains both skins and rigid part");imported.scene.select(imported.objects.front());Document doc;doc.replace(imported.scene.snapshot());doc.authored_shading();ready(doc);
 for(int stroke=0;stroke<2;++stroke){const auto frame=doc.frame().surface;const auto &range=frame->ranges.front();check(range.object==doc.copy_scene().active().id&&range.count>=6,"Actual visible active source part supplies the native paint ray");const size_t face=range.first+(stroke?3:0);const auto a=frame->mesh.positions.at(frame->mesh.indices.at(face)),b=frame->mesh.positions.at(frame->mesh.indices.at(face+1)),c=frame->mesh.positions.at(frame->mesh.indices.at(face+2));const auto p=a*.3f+b*.3f+c*.4f,n=normalized(cross(b-a,c-a));PaintSettings ink;ink.channel=PaintChannel::Color;ink.color=stroke?std::array<float,4>{.8f,.15f,.7f,.7f}:std::array<float,4>{.15f,.85f,.3f,.8f};ink.front_only=false;doc.paint_settings(ink);NativeStrokeSample sample;sample.origin=p+n;sample.direction=n*-1;sample.brush.radius=stroke?.055f:.085f;sample.brush.strength=.7f;sample.brush.blend=.4f;check(doc.begin_native_stroke()&&doc.sample_native_stroke(sample),"Genuine independent colour/coverage stroke enters actual native worker");doc.end_native_stroke();check(doc.wait_native_stroke(std::chrono::seconds(120)),"Genuine nonuniform paint worker retires");ready(doc);}
 const auto native=out/"nonuniform-source.dsculpt";doc.save_async(native).get();const auto scene=doc.copy_scene();const auto exact=scene.encode();check(Scene::load(native).encode()==exact&&scene.active().painted->source_sampling_prepared(),"Real source/paint owner persists before export counterfactual");const auto path=out/"dynamic.vrm";const std::vector<uint8_t> prior{7,3,9,1};{std::ofstream f(path,std::ios::binary);f.write(reinterpret_cast<const char*>(prior.data()),prior.size());}
 bool refused=false;std::string reason;try{doc.export_asset(path,AssetExportFormat::EditableAvatar256).get();}catch(const std::exception&e){refused=true;reason=e.what();}check(doc.wait_asset(std::chrono::seconds(120)),"Actual EditableAvatar file worker reaches its failure endpoint");std::cout<<"OLD_DYNAMIC_EXPRESSION_REFUSAL "<<reason<<std::endl;check(refused&&reason.find("UV expressions")!=std::string::npos,"Actual old native painted worker refuses unresolved dynamic UV-expression transport");check(bytes(path)==prior&&doc.copy_scene().encode()==exact&&Scene::load(native).encode()==exact,"Old dynamic refusal preserves prior output and complete original source/paint Scene");std::cout<<"PASS "<<checks<<" actual old dynamic painted-expression counterfactual checks\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
