// SPDX-License-Identifier: GPL-3.0-or-later
#define main original_mtoon_suite
#include "mtoon-render.cpp"
#undef main
#include "dome/document.hpp"
#include "dome/vrm-expression.hpp"
#include "dome/material-bindings.hpp"
#include "dome/material-visibility.hpp"
#include "vrm1-expression-layer-consumer.hpp"
#include <fstream>
int main(int argc,char **argv)try{
 if(argc<3||argc>4)return 2;
 const std::filesystem::path fixture=argv[1],out=argv[2];std::filesystem::create_directories(out);NativeGraphics graphics;
 const std::string vendor=reinterpret_cast<const char*>(glGetString(GL_VENDOR)),device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));
 std::cout<<"UV_TARGET_DEVICE vendor="<<vendor<<" renderer="<<device<<std::endl;
 if(argc==4)check(std::string_view(argv[3])=="--hardware"&&vendor.find("Intel")!=std::string::npos&&device.find("Arc")!=std::string::npos&&device.find("A770")!=std::string::npos&&device.find("llvmpipe")==std::string::npos&&device.find("softpipe")==std::string::npos,"Actual selected Intel Arc A770 is required");
 const auto baseline=Scene::load(fixture/"pristine-source.dsculpt");const auto exact=baseline.encode();const auto id=baseline.active().id;
 const auto &binding=*baseline.active().material_bindings;const auto &a=binding.roles[0],&b=binding.roles[1];const auto &x=binding.material.textures[0],&y=binding.material.textures[1];
 const bool shared=a.image&&a.image==b.image&&a.wrap_s==b.wrap_s&&a.wrap_t==b.wrap_t&&a.min_filter==b.min_filter&&a.mag_filter==b.mag_filter&&mtoon_coordinate(x)==mtoon_coordinate(y)&&x.offset==y.offset&&x.scale==y.scale&&x.rotation==y.rotation;
 Fbo fbo;SurfaceRenderer renderer;renderer.set_output_encoding(SurfaceOutputEncoding::Linear);renderer.set_frame_time(0);V1ExpressionLayerConsumer consumer;
 auto stable=[](const SurfaceBufferStats&a,const SurfaceBufferStats&b,const SurfaceTextureStats&c,const SurfaceTextureStats&d){return a.geometry_uploads==b.geometry_uploads&&a.weight_uploads==b.weight_uploads&&a.mask_uploads==b.mask_uploads&&c.uploads==d.uploads&&c.pack_builds==d.pack_builds;};
 for(const char *asset:{"source-mask-matcap.vrm"}){
  role_consumer::Model model(fixture/asset);consumer.selected_node=-1;
  consumer.selected_node=baseline.active().appearance->source_node;
  check(consumer.selected_node>=0,"Independent target consumer selects actual authenticated emitted object identity");
  const Mat4 camera{5.27903413772583f,0,0,0,0,5.27903413772583f,0,0,0,0,5.27903413772583f,0,-2.639517307281494f,-.6387631297111511f,0,1};
  auto image=[&](const std::string &name,const Pixels &pixels){const auto encoded=Image{96,96,pixels}.png();std::ofstream f(out/(std::string(asset)+"-"+name+".png"),std::ios::binary);f.write(reinterpret_cast<const char*>(encoded.data()),encoded.size());};
  for(int pose:{0,2}){
   auto posed=baseline.snapshot();
   if(pose){
    const auto raw=*baseline.active().appearance->original_bytes;const uint32_t length=uint32_t(raw[12])|uint32_t(raw[13])<<8|uint32_t(raw[14])<<16|uint32_t(raw[15])<<24;const auto j=nlohmann::json::parse(raw.begin()+20,raw.begin()+20+length);const auto head=j.at("extensions").at("VRMC_vrm").at("humanoid").at("humanBones").at("head").at("node").get<uint32_t>();
    for(const auto &part:baseline.objects())if(part.deformation&&part.deformation->skeleton&&part.appearance){auto d=std::make_shared<DeformationState>(*part.deformation);const auto at=std::find(part.appearance->source_joint_nodes.begin(),part.appearance->source_joint_nodes.end(),head);check(at!=part.appearance->source_joint_nodes.end(),"Native pose uses each part's actual original head identity");const auto joint=size_t(at-part.appearance->source_joint_nodes.begin());d->pose.clear();for(const auto &bone:d->skeleton->bones)d->pose.push_back(bone.rest);const float angle=-.12f;d->pose[joint].rotation=Quat{std::cos(angle*.5f),0,std::sin(angle*.5f),0}*d->pose[joint].rotation;d->pose[joint].translation+=Vec3{-.02f,.01f,0};posed.object(part.id).deformation=d;}
   }
   // Only graphics focuses one part. The file retains every emitted source part.
   for(const auto &part:posed.objects())if(part.id!=id)posed.object(part.id).visible=false;
   const auto posed_exact=posed.encode();Document doc;doc.replace(posed.snapshot());doc.authored_shading();check(doc.wait_mesh()&&doc.frame().state->error.empty(),"Actual native typed target frame publishes");doc.begin_expression_preview();check(doc.wait_mesh()&&doc.frame().state->expressions&&doc.frame().state->expressions->ready,"Actual immutable native expression session owns the target frame");auto session=doc.frame().state->expressions;
   size_t happy=session->entries.size(),cross=happy;
   for(size_t i=0;i<session->entries.size();++i){const auto &e=session->entries[i];if(!e.custom&&e.name=="happy")happy=i;if(e.custom&&e.name=="cross")cross=i;}
   check(happy<session->entries.size(),"Native target preview resolves genuine preset expression identity");
   std::vector<std::array<float,2>> weights{{0,0},{.35f,0},{1,0}};if(cross<session->entries.size()){weights.push_back({0,.35f});weights.push_back({0,1});weights.push_back({.35f,.6f});}
   for(const auto &w:weights){
    doc.configure_expression_preview(happy,w[0],session->session);check(doc.wait_mesh()&&doc.frame().state->error.empty(),"Actual preset weight worker publishes");
    if(cross<session->entries.size()){doc.configure_expression_preview(cross,w[1],session->session);check(doc.wait_mesh()&&doc.frame().state->error.empty(),"Actual custom weight worker publishes independently of the preset");}
    const auto frame=doc.frame();check(frame.state->expressions->entries[happy].weight==w[0]&&(cross==session->entries.size()||frame.state->expressions->entries[cross].weight==w[1]),"Published immutable view retains both requested source weights exactly");
    consumer.weights={{"preset/happy",w[0]}};if(cross<session->entries.size())consumer.weights["custom/cross"]=w[1];consumer.expression=w[0]+w[1];consumer.pose=pose;
    SurfaceBufferStats first_buffer{};SurfaceTextureStats first_texture{};
    for(int eye=0;eye<2;++eye){
     auto view=camera;if(eye)view[12]+=.008f;
     fbo.bind();renderer.draw(frame.surface,{},view,{0,0,2},96,96,true);const auto native=read();
     if(!eye){first_buffer=renderer.buffer_stats();first_texture=renderer.texture_stats();}
     else check(stable(first_buffer,renderer.buffer_stats(),first_texture,renderer.texture_stats()),"Actual second eye reuses all immutable geometry and image uploads");
     fbo.bind();const auto independent=consumer.render(model,0,view);const auto name="p"+std::to_string(pose)+"-h"+std::to_string(w[0])+"-c"+std::to_string(w[1])+"-e"+std::to_string(eye);image(name+"-native",native);image(name+"-parsed",independent);const auto maximum=delta(native,independent);size_t coverage=0;for(size_t i=3;i<native.size();i+=4)coverage+=native[i]!=0;
     std::cout<<"UV_TARGET_IMAGE "<<asset<<" "<<name<<" max="<<maximum<<" coverage="<<coverage<<std::endl;
     check(coverage>300&&maximum<=4,"Actual painted native source matches independent varying/zero/mixed target semantics at unchanged4");
     if(!pose&&!eye&&((w[0]==1&&w[1]==0)||(w[0]==0&&w[1]==1))){
      bool layered=false;for(const auto &mesh:model.json.at("meshes"))layered|=mesh.at("primitives").size()>1;
      std::vector<std::pair<bool*,const char*>> controls{{&consumer.wrong_source_uv,"frozen-source"},{&consumer.wrong_matcap_view,"matcap-mesh-UV"},{&consumer.wrong_matcap_uv_target,"matcap-UV-binding"},{&consumer.wrong_matcap_color,"missing-matcap"}};if(layered)controls.push_back({&consumer.wrong_paint_uv,"moving-paint"});if(layered&&w[0]==1)controls.push_back({&consumer.wrong_base_tint,"tinted-paint"});if(binding.material.parameters.alpha==MToonAlpha::Mask)controls.push_back({&consumer.wrong_mask_cutoff,"wrong-mask-cutoff"});if(shared&&layered)controls.push_back({&consumer.wrong_shared_shade,"missing-shade-paint"});
      for(const auto &[flag,label]:controls){*flag=true;fbo.bind();const auto wrong=consumer.render(model,0,view);*flag=false;const auto effect=delta(independent,wrong);std::cout<<"UV_TARGET_SENSITIVITY "<<label<<" "<<effect<<std::endl;check(effect>2,"Independent target consumer detects the genuine source/paint counterfactual");}
     }
     if(eye){fbo.bind();renderer.draw(frame.surface,{},view,{0,0,2},96,96,true);check(read()==native&&stable(first_buffer,renderer.buffer_stats(),first_texture,renderer.texture_stats()),"Actual no-op view preserves every native pixel and performs no geometry or image reupload");}
    }
    check(doc.copy_scene().encode()==posed_exact,"Target rendering preserves complete focused source/paint/native history");
   }
   doc.end_expression_preview(false,session->session);check(doc.copy_scene().encode()==posed_exact&&!doc.frame().state->expressions,"Actual Cancel retires the original session and preserves exact source/paint Scene");
  }
 }
 check(baseline.encode()==exact,"Independent target proof leaves original complete input immutable");std::cout<<"PASS "<<checks<<" actual independent UV target render checks\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
