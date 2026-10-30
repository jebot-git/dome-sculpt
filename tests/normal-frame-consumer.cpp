// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_material_renderer_suite
#include "mtoon-render.cpp"
#undef main
#include "dome/material-bindings.hpp"
#include "dome/source-uv-bake.hpp"
#include "normal-frame-consumer.hpp"
namespace {
Scene model(){
 auto mesh=quad();mesh.normals={{0,0,1},normalized(Vec3{.2f,.6f,.8f}),normalized(Vec3{.6f,0,.8f}),normalized(Vec3{0,.4f,.9f})};auto maps=std::make_shared<PaintSourceMaps>();maps->normal.image=image({230,153,204,255});maps->normal_scale=.8f;auto paint=PaintedMesh::from_maps(mesh,maps,64);
 auto rest=std::make_shared<DeformationState>(mesh);rest->morphs=MorphSet(mesh);auto target=mesh.positions;for(auto &p:target)p.z+=.05f;rest->morphs->set_target(mesh,"artist",target);
 auto origin=std::make_shared<SourceUvOrigin>();origin->paint=paint->snapshot();origin->deformation=rest;
 auto frame=std::make_shared<NormalFrames>();frame->origin_vertex_count=4;frame->origin_vertices=mesh.indices;for(auto v:mesh.indices){auto n=mesh.normals[v],t=normalized(cross(Vec3{0,1,0},n));frame->normals.push_back(n);frame->tangents.push_back({t.x,t.y,t.z,1});}frame->targets.push_back({1,std::vector<Vec3>(6,Vec3{.1f,0,-.01f}),std::vector<Vec3>(6,Vec3{0,.3f,0})});frame->targets[0].tangents[3].y=.5f;
 auto set=std::make_shared<NormalFrameSet>();set->pbr=frame;set->targets={{1,0,std::nullopt,{}}};set->current_targets={1};set->next_target=2;auto binding=std::make_shared<SourceMaterialBindings>();binding->has_mtoon=false;binding->normal_frame=SourceMaterialNormalFrame::OriginalAttributes;binding->normal_frames=set;binding->uv_origin=origin;
 Scene scene;auto &o=scene.active();o.mesh_active=true;o.painted=paint;o.material.color={.6f,.4f,.2f,1};o.material_bindings=binding;auto current=*rest;current.skeleton=Skeleton{{{"root",-1,{},1}}};current.skin=SkinWeights{};current.skin->vertices.assign(4,{{0,1}});current.pose={{{},{},{2,1,.5f}}};scene.set_deformation(o.id,current);return scene;
}
Scene canonical_model(bool rigid) {
 auto scene=model();auto &o=scene.active();auto binding=std::make_shared<SourceMaterialBindings>(*o.material_bindings);auto set=std::make_shared<NormalFrameSet>(*binding->normal_frames);auto frame=std::make_shared<NormalFrames>(*set->pbr);frame->normals.assign(6,{0,0,1});frame->tangents.assign(6,{1,0,0,1});frame->targets[0].normals.clear();frame->targets[0].tangents.clear();set->pbr=frame;auto canonical=std::make_shared<NormalFrames>(*frame);canonical->transport=NormalFrameTransport::CanonicalMikk;canonical->generated_tangents=true;canonical->generated_uv=0;set->canonical=canonical;binding->normal_frames=set;o.material_bindings=binding;
 BakedTextures bake;auto solid=[](std::array<uint8_t,4> pixel){Image out{64,64,std::vector<uint8_t>(64*64*4)};for(size_t n=0;n<out.rgba.size();++n)out.rgba[n]=pixel[n%4];return out;};bake.maps.color=solid({255,255,255,255});bake.maps.metal_rough=solid({255,255,255,255});bake.maps.emission=solid({0,0,0,255});bake.maps.normal=solid({128,204,230,255});bake.coverage.assign(64*64,255);o.painted=o.painted->snapshot();o.painted->install_bake(std::move(bake));o.material.color={1,1,1,1};
 auto state=*o.deformation;if(!rigid){state.skeleton.reset();state.skin.reset();state.pose.clear();}scene.set_deformation(o.id,state);scene.validate();return scene;
}
}
int main(int argc,char**argv)try {
 if(argc<2)return 2;std::filesystem::path prefix=argv[1];NativeGraphics graphics;std::string device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GPU "<<device<<'\n';check(argc<3||std::string(argv[2])!="--hardware"||device.find("llvmpipe")==std::string::npos,"Independent exported-frame consumer runs on requested graphics hardware");
 Fbo framebuffer;framebuffer.bind();SurfaceRenderer native;frame_consumer::Renderer independent;
 auto original=model();
 for(int stage=0;stage<2;++stage){auto scene=original;if(stage){const auto &o=scene.active();SourceUvBakeInput in;in.paint=o.painted;in.current_material=o.material_bindings;in.current_deformation=o.deformation;const auto &m=o.painted->mesh();for(auto v:m.indices){in.source_vertices.push_back(v);in.target.positions.push_back(m.positions[v]);in.target.normals.push_back(m.normals[v]);auto q=m.uv[v];in.target.uv.push_back({1-q[1],q[0]});in.target.indices.push_back(uint32_t(in.target.positions.size()-1));}SourceUvBakeOptions options;options.resolution=64;auto adopt=adopt_source_uv_bake(bake_source_uv(in,options));scene.active().painted=adopt.paint;scene.active().material_bindings=adopt.material_bindings;scene.active().deformation=adopt.deformation;}
  for(float w:{0.f,.35f,1.f}){auto state=*scene.active().deformation;state.morph_weights={w};scene.set_deformation(scene.active().id,state);Document document;document.replace(scene);check(document.wait_mesh(),"Actual worker publishes curved authored-frame morph and anisotropic pose");auto path=prefix;path+="-"+std::to_string(stage)+"-"+std::to_string(int(w*100))+".glb";export_glb(scene.preview_surfaces(),path);frame_consumer::Model file(path);auto expected=draw(native,document.frame().surface);framebuffer.bind();independent.render(file.data);auto actual=read();int error=delta(expected,actual);std::cout<<"CONSUMER stage="<<stage<<" morph="<<w<<" maxerror="<<error<<" vertices="<<independent.vertices<<'\n';check(error<=3,"Independent cgltf/PNG/morph/skin/GLSL consumer matches production pixels from the exported curved normal model");}
 }
 for(bool rigid:{false,true})for(float w:{0.f,.35f,1.f}) {
  auto scene=canonical_model(rigid);auto state=*scene.active().deformation;state.morph_weights={w};scene.set_deformation(scene.active().id,state);Document document;document.replace(scene);check(document.wait_mesh(),"Exact flat canonical frame reaches the real worker with current position morph and optional rigid skin");auto path=prefix;path+="-canonical-"+std::to_string(rigid)+"-"+std::to_string(int(w*100))+".glb";export_glb(scene.preview_surfaces(),path);frame_consumer::Model file(path);auto wanted=draw(native,document.frame().surface);framebuffer.bind();independent.render(file.data);int error=delta(wanted,read());std::cout<<"CANONICAL_CONSUMER rigid="<<rigid<<" morph="<<w<<" maxerror="<<error<<'\n';check(error<=3,"Independent standard PBR consumer preserves the proven full-coverage static or rigid canonical normal evaluation");
 }
 std::cout<<"Frame consumer checks "<<checks<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
