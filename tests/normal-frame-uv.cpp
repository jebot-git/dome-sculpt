// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_material_renderer_suite
#include "mtoon-render.cpp"
#undef main
#include "dome/material-bindings.hpp"
#include "dome/source-uv-bake.hpp"
namespace {
Image solid64(std::array<uint8_t,4> p){Image i{64,64,std::vector<uint8_t>(64*64*4)};for(size_t n=0;n<i.rgba.size();++n)i.rgba[n]=p[n%4];return i;}
SourceUvBakeInput rotated(const Scene &s,bool split) {
 const auto &o=s.active();SourceUvBakeInput i;i.paint=o.painted;i.current_material=o.material_bindings;i.current_deformation=o.deformation;i.appearance=o.appearance;
 const auto &m=o.painted->mesh();
 if(split)for(auto v:m.indices){i.source_vertices.push_back(v);i.target.positions.push_back(m.positions[v]);i.target.normals.push_back(m.normals[v]);auto q=m.uv[v];i.target.uv.push_back({1-q[1],q[0]});i.target.indices.push_back(uint32_t(i.target.positions.size()-1));}
 else {i.target=m;for(auto &q:i.target.uv)q={1-q[1],q[0]};for(uint32_t v=0;v<m.positions.size();++v)i.source_vertices.push_back(v);}
 return i;
}
void install(Scene &s,const SourceUvAdoption &a){auto &o=s.active();o.painted=a.paint;o.material_bindings=a.material_bindings;o.appearance=a.appearance;o.deformation=a.deformation;s.validate();}
}
int main(int argc,char**argv)try {
 NativeGraphics graphics;std::string device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GPU "<<device<<'\n';
 check(argc<2||std::string(argv[1])!="--hardware"||device.find("llvmpipe")==std::string::npos,"Retained-frame UV transfer runs on the requested GPU");
 Fbo framebuffer;framebuffer.bind();SurfaceRenderer renderer;renderer.set_frame_time(0);
 for(int variant=0;variant<3;++variant) {
  auto model=quad();auto maps=std::make_shared<PaintSourceMaps>();maps->color.image=image({255,255,255,255});maps->normal.image=image({255,128,128,255});maps->normal_scale=.8f;
  auto paint=PaintedMesh::from_maps(model,maps,64);BakedTextures bake;bake.maps.color=solid64({255,255,255,255});bake.maps.metal_rough=solid64({255,204,0,255});bake.maps.emission=solid64({0,0,0,255});bake.maps.normal=solid64({128,204,230,255});bake.coverage.resize(64*64);
  for(size_t p=0;p<bake.coverage.size();++p)bake.coverage[p]=p%64<22?0:p%64<43?128:255;paint->install_bake(bake);
  auto source=std::make_shared<NormalFrames>();source->origin_vertex_count=4;source->origin_vertices=model.indices;source->normals.assign(6,{0,0,1});source->tangents.assign(6,{1,0,0,1});source->targets.push_back({1,std::vector<Vec3>(6,Vec3{1,0,0}),std::vector<Vec3>(6,Vec3{0,1,0})});
  auto canonical=std::make_shared<NormalFrames>(*source);canonical->transport=NormalFrameTransport::CanonicalMikk;canonical->generated_tangents=true;canonical->generated_uv=0;canonical->targets[0].normals.clear();canonical->targets[0].tangents.clear();
  auto frames=std::make_shared<NormalFrameSet>();frames->pbr=source;if(variant)frames->mtoon=source;frames->canonical=canonical;frames->targets={{1,0,std::nullopt,{}}};frames->current_targets={1};frames->next_target=2;
  auto origin=std::make_shared<SourceUvOrigin>();origin->paint=paint->snapshot();auto state=std::make_shared<DeformationState>(model);state->morphs=MorphSet(model);state->morphs->set_target(model,"direction",model.positions);origin->deformation=state;
  auto binding=std::make_shared<SourceMaterialBindings>();binding->has_mtoon=variant!=0;binding->normal_frames=frames;binding->normal_frame=SourceMaterialNormalFrame::OriginalAttributes;binding->uv_origin=origin;binding->native_reference.color={1,1,1,1};
  if(variant){auto material=typed(variant==1);material.textures[2].index=0;material.parameters.normal_enabled=true;material.parameters.normal_scale=.8f;material.parameters.base_color={.8f,.5f,.2f,1};material.parameters.shade_color={.05f,.1f,.2f};material.parameters.toony=0;material.parameters.shift=variant==1?-.5f:.2f;binding->material=material;binding->roles[2].image=maps->normal.image;origin->material=material;origin->roles=binding->roles;}
  Scene scene;scene.active().painted=paint;scene.active().mesh_active=true;scene.active().material=white();scene.active().material.unlit=false;scene.active().material_bindings=binding;
  auto current=*state;current.morph_weights={.5f};current.skeleton=Skeleton{{{"root",-1,{},1}}};current.skin=SkinWeights{};current.skin->vertices.assign(4,{{0,1}});current.pose={{{},{},{2,1,.5f}}};scene.set_deformation(scene.active().id,current);
  Document original;original.replace(scene);check(original.wait_mesh(),"Original source/canonical normal frames reach the actual worker before UV transfer");auto expected=draw(renderer,original.frame().surface);
  SourceUvBakeOptions options;options.resolution=64;auto result=bake_source_uv(rotated(scene,true),options);auto adopted=adopt_source_uv_bake(result);
  check(adopted.material_bindings->normal_frames==frames && adopted.material_bindings->uv_origin==origin && adopted.deformation->skin->vertices.size()==6 && adopted.deformation->morphs->targets[0].delta.size()==6,"UV adoption retains immutable frames/origin while exactly remapping current skin and morph POSITION authority");
  check(result.maps->normal.image->rgba[0]==255 && result.maps->normal.image->rgba[1]==128,"Normal texels remain in their original encoded frame instead of undergoing static atlas conversion");
  install(scene,adopted);Document once;once.replace(scene);check(once.wait_mesh() && delta(expected,draw(renderer,once.frame().surface))<=3,"Protected/fractional/full baked coverage remains pose-correct after actual vertex-splitting atlas rotation");
  auto repeated=adopt_source_uv_bake(bake_source_uv(rotated(scene,false),options));install(scene,repeated);Document twice;twice.replace(scene);
  check(twice.wait_mesh() && delta(expected,draw(renderer,twice.frame().surface))<=3,"Repeated UV baking preserves original and canonical posed normal authority without repeated conversion");
  auto saved=scene.encode();auto reopened=Scene::decode(nlohmann::json::from_cbor(nlohmann::json::to_cbor(saved)));Document restart;restart.replace(reopened);
  check(saved["version"]==9 && restart.wait_mesh() && delta(expected,draw(renderer,restart.frame().surface))<=3,"Scene9 native restart retains repeat-baked frames, current rig mapping and identical GPU output");
  auto malformed=result;malformed.normal_frames.reset();check(rejects([&]{adopt_source_uv_bake(malformed);}),"UV adoption cannot silently drop an original frame payload");
 }
 std::cout<<"Normal frame UV checks "<<checks<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
