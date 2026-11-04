// SPDX-License-Identifier: GPL-3.0-or-later
#define main original_mtoon_suite
#include "mtoon-render.cpp"
#undef main
#include "dome/document.hpp"
#include "dome/vrm-expression.hpp"
#include "dome/material-bindings.hpp"
#include "vrm1-expression-layer-consumer.hpp"
#include <fstream>
int main(int argc,char **argv)try{
 if(argc<3||argc>4)return 2;const std::filesystem::path fixture=argv[1],out=argv[2];std::filesystem::create_directories(out);NativeGraphics graphics;
 const std::string vendor=reinterpret_cast<const char*>(glGetString(GL_VENDOR)),device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"EXPRESSION_LAYER_DEVICE vendor="<<vendor<<" renderer="<<device<<std::endl;
 if(argc==4)check(std::string_view(argv[3])=="--hardware"&&vendor.find("Intel")!=std::string::npos&&device.find("Arc")!=std::string::npos&&device.find("A770")!=std::string::npos&&device.find("llvmpipe")==std::string::npos&&device.find("softpipe")==std::string::npos,"Actual selected Intel Arc A770 is required");
 const auto baseline=Scene::load(fixture/"nonuniform-source.dsculpt");const auto exact=baseline.encode();const auto id=baseline.active().id;Fbo fbo;SurfaceRenderer renderer;renderer.set_output_encoding(SurfaceOutputEncoding::Linear);renderer.set_frame_time(0);V1ExpressionLayerConsumer consumer;
 for(const char *asset:{"dynamic.vrm","dynamic-512.vrm","dynamic-1024.vrm"}){role_consumer::Model model(fixture/asset);consumer.selected_node=-1;
 const auto &remaps=model.json.at("extras").at("domeSculptAuthoring").at("objects");for(const auto &entry:remaps)if(entry.at("object").get<std::string>()==std::to_string(id))consumer.selected_node=entry.at("node").get<int>();check(consumer.selected_node>=0,"Independent consumer selects actual authenticated emitted source object ID");
 const Mat4 camera{5.27903413772583f,0,0,0,0,5.27903413772583f,0,0,0,0,5.27903413772583f,0,-2.639517307281494f,-.6387631297111511f,0,1};
 auto image=[&](const std::string &name,const Pixels &pixels){const auto bytes=Image{96,96,pixels}.png();std::ofstream f(out/(std::string(asset)+"-"+name+".png"),std::ios::binary);f.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());};
 for(int pose:{0,2}){
  auto posed=baseline.snapshot();if(pose){
   const auto raw=*baseline.active().appearance->original_bytes;const uint32_t length=uint32_t(raw[12])|uint32_t(raw[13])<<8|uint32_t(raw[14])<<16|uint32_t(raw[15])<<24;const auto json=nlohmann::json::parse(raw.begin()+20,raw.begin()+20+length);const auto head=json.at("extensions").at("VRMC_vrm").at("humanoid").at("humanBones").at("head").at("node").get<uint32_t>();
   for(const auto &part:baseline.objects())if(part.deformation&&part.deformation->skeleton&&part.appearance){auto deformation=std::make_shared<DeformationState>(*part.deformation);const auto at=std::find(part.appearance->source_joint_nodes.begin(),part.appearance->source_joint_nodes.end(),head);check(at!=part.appearance->source_joint_nodes.end(),"Native pose uses actual per-part original head identity");const size_t joint=size_t(at-part.appearance->source_joint_nodes.begin());deformation->pose.clear();for(const auto &bone:deformation->skeleton->bones)deformation->pose.push_back(bone.rest);const float angle=-.12f;deformation->pose[joint].rotation=Quat{std::cos(angle*.5f),0,std::sin(angle*.5f),0}*deformation->pose[joint].rotation;deformation->pose[joint].translation+=Vec3{-.02f,.01f,0};posed.object(part.id).deformation=deformation;}
  }
  // Focused material/part view; the actual file and native package above retain
  // every emitted part. No spring/constraint simulation is claimed here.
  for(const auto &part:posed.objects())if(part.id!=id)posed.object(part.id).visible=false;
  const auto posed_exact=posed.encode();Document doc;doc.replace(posed.snapshot());doc.authored_shading();check(doc.wait_mesh()&&doc.frame().state->error.empty(),"Actual original typed native frame publishes");doc.begin_expression_preview();check(doc.wait_mesh()&&doc.frame().state->expressions&&doc.frame().state->expressions->ready,"Actual native expression preview owns source/paint frame");auto session=doc.frame().state->expressions;size_t selected=0;while(selected<session->entries.size()&&(session->entries[selected].custom||session->entries[selected].name!="happy"))++selected;check(selected<session->entries.size(),"Native preview selects actual source expression");
  for(float weight:{0.f,.35f,1.f}){
   doc.configure_expression_preview(selected,weight,session->session);check(doc.wait_mesh()&&doc.frame().state->error.empty(),"Native nonuniform paint plus expression request completes");consumer.expression=weight;consumer.pose=pose;
   for(int eye=0;eye<2;++eye){auto view=camera;if(eye)view[12]+=.008f;fbo.bind();renderer.draw(doc.frame().surface,{},view,{0,0,2},96,96,true);const auto native=read();fbo.bind();const auto independent=consumer.render(model,0,view);const auto name="p"+std::to_string(pose)+"-w"+std::to_string(weight)+"-e"+std::to_string(eye);image(name+"-native",native);image(name+"-parsed",independent);const auto maximum=delta(native,independent);size_t coverage=0;for(size_t i=3;i<native.size();i+=4)coverage+=native[i]!=0;std::cout<<"DYNAMIC_LAYER_IMAGE "<<asset<<" "<<name<<" max="<<maximum<<" coverage="<<coverage<<std::endl;check(coverage>300&&maximum<=4,"Actual native nonuniform painted expression matches independently parsed standard layer at unchanged four-level bound");
    if(!pose&&weight==1&&!eye){std::vector<bool*> sensitivity{&consumer.wrong_paint_uv,&consumer.wrong_base_tint};if(baseline.active().material_bindings->roles[0].image==baseline.active().material_bindings->roles[1].image)sensitivity.push_back(&consumer.wrong_shared_shade);for(auto flag:sensitivity){*flag=true;fbo.bind();const auto wrong=consumer.render(model,0,view);*flag=false;const auto effect=delta(independent,wrong);std::cout<<"DYNAMIC_LAYER_SENSITIVITY "<<effect<<std::endl;check(effect>2,"Independent emitted-layer consumer detects moving or tinting fixed paint");}}
   }
   check(doc.copy_scene().encode()==posed_exact,"Rendered expression leaves complete focused source/paint Scene exact");
  }
  doc.end_expression_preview(false,session->session);check(doc.copy_scene().encode()==posed_exact,"Actual expression Cancel preserves complete native history and paint");
 }
 }
 check(baseline.encode()==exact,"Dynamic layer render leaves original input immutable");std::cout<<"PASS "<<checks<<" actual dynamic expression-layer render checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
