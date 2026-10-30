// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_material_renderer_suite
#include "mtoon-render.cpp"
#undef main
#include "../src/source-frame-skin.hpp"
#include "dome/material-bindings.hpp"
#include "normal-frame-fixtures.hpp"
namespace {
void frames(MeshSnapshot &s,Vec3 N,Vec3 T,float sign=1) {
  s.source_normals.assign(s.mesh.positions.size(),N);s.source_tangents.assign(s.mesh.positions.size(),{T.x,T.y,T.z,sign});s.ranges[0].source_frame=true;
}
Vec3 source_expected(bool legacy,Vec3 N,Vec3 T,Vec3 encoded,float strength=1) {
  N=normalized(N);T=normalized(T);auto B=cross(N,T);if(!legacy)B=normalized(B);
  auto detail=mtoon_normal_map(encoded,strength);return normalized(T*detail.x+B*detail.y+N*detail.z);
}
Image flat64(std::array<uint8_t,4> p){Image out{64,64,std::vector<uint8_t>(64*64*4)};for(size_t i=0;i<out.rgba.size();++i)out.rgba[i]=p[i%4];return out;}
bool capture_buffers=false;size_t array_bytes=0,index_bytes=0;
}
extern "C" void __real_glBufferData(GLenum,GLsizeiptr,const void*,GLenum);
extern "C" void __wrap_glBufferData(GLenum target,GLsizeiptr bytes,const void *data,GLenum usage){if(capture_buffers){if(target==GL_ARRAY_BUFFER)array_bytes+=bytes;if(target==GL_ELEMENT_ARRAY_BUFFER)index_bytes+=bytes;}__real_glBufferData(target,bytes,data,usage);}
extern "C" void __real_glBufferSubData(GLenum,GLintptr,GLsizeiptr,const void*);
extern "C" void __wrap_glBufferSubData(GLenum target,GLintptr offset,GLsizeiptr bytes,const void *data){if(capture_buffers){if(target==GL_ARRAY_BUFFER)array_bytes+=bytes;if(target==GL_ELEMENT_ARRAY_BUFFER)index_bytes+=bytes;}__real_glBufferSubData(target,offset,bytes,data);}
int main(int argc,char **argv)try {
 NativeGraphics graphics;std::string device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GPU "<<device<<'\n';
 check(argc<2||std::string(argv[1])!="--hardware"||(device.find("llvmpipe")==std::string::npos&&device.find("softpipe")==std::string::npos),"Original-frame rendering runs on requested hardware");
 Fbo framebuffer;framebuffer.bind();SurfaceRenderer renderer;renderer.set_frame_time(0);
 // Independent analytic morph: +X/+Z basis rotates120degrees about(1,1,1),
 // mixed at.5; then an anisotropic joint scales(2,1,.5). Raw T/N are nonorthogonal.
 const Vec3 T{1,.5f,0},N{.25f,0,1},encoded{1,128/255.f,128/255.f};
 for(bool legacy:{false,true}) {
  auto m=typed(legacy);m.textures[2].index=0;m.parameters.normal_enabled=true;m.parameters.normal_scale=.8f;
  m.parameters.base_color={.8f,.5f,.2f,1};m.parameters.shade_color={.05f,.1f,.2f};m.parameters.toony=0;m.parameters.shift=legacy?-.5f:.2f;
  auto scene=snapshot();scene->ranges[0].appearance=appearance(m,{image({255,128,128,255})});frames(*scene,N,T);
  auto wanted=source_expected(legacy,N,T,encoded,.8f);
  check(close(at(draw(renderer,scene)),expected(m,{},wanted),3),"Authored frame survives simultaneous nonorthogonal morph and anisotropic skin without posed-atlas GramSchmidt");
  auto uv=std::make_shared<MeshSnapshot>(*scene);for(auto &q:uv->mesh.uv)q={1-q[1],q[0]};
  check(delta(draw(renderer,scene),draw(renderer,uv))<=1,"Raw normal samples keep original frame when sampling atlas rotates");
  auto repeat=std::make_shared<MeshSnapshot>(*uv);for(auto &q:repeat->mesh.uv)q={1-q[1],q[0]};
  check(delta(draw(renderer,scene),draw(renderer,repeat))<=1,"Repeated atlas rotation cannot rotate retained original normal vectors");
  if(legacy) {
    auto collapse=MToonMaterial::from_vrm0({{"shader","VRM/MToon"},{"keywordMap",{{"_NORMALMAP",true}}},{"textureProperties",{{"_BumpMap",0}}},{"vectorProperties",{{"_MainTex",{.3f,.2f,0,0}}}}});
    collapse.parameters.base_color=m.parameters.base_color;collapse.parameters.shade_color=m.parameters.shade_color;collapse.parameters.shift=m.parameters.shift;collapse.parameters.toony=0;collapse.parameters.normal_scale=.8f;
    auto constant=std::make_shared<MeshSnapshot>(*scene);constant->ranges[0].appearance=appearance(collapse,{image({255,128,128,255})});
    check(close(at(draw(renderer,constant)),expected(collapse,{},wanted),3),"Legacy fixed vertex frame survives degenerate MainTex sampling scale without derivative fallback");
  }
  auto invalid=std::make_shared<MeshSnapshot>(*scene);invalid->source_normals.clear();check(rejects([&]{draw(renderer,invalid);}),"Explicit source frame flags require complete immutable attribute arrays");
  invalid=std::make_shared<MeshSnapshot>(*scene);invalid->source_tangents[0][3]=-1;check(rejects([&]{draw(renderer,invalid);}),"Retained source frames refuse mixed handedness within a triangle");

  // Actual per-vertex interpolation from three independently normalized frames;
  // legacy interpolates vertex B, modern reconstructs B from interpolated T/N.
  auto curved=std::make_shared<MeshSnapshot>(*scene);curved->source_normals={{0,0,1},{.2f,.6f,.8f},{.6f,0,.8f},{0,.4f,.9f}};
  curved->source_tangents={{{1,0,0,1}},{{1,.2f,-.1f,1}},{{.5f,.8f,.2f,1}},{{1,0,.1f,1}}};
  constexpr int px=52,py=44;float x=(px+.5f)/48-1,y=(py+.5f)/48-1,u=(x+.75f)/1.5f,v=(y+.75f)/1.5f;
  std::array<float,3> bary{1-u,u-v,v};Vec3 ni{},ti{},bi{};
  for(size_t c=0;c<3;++c){auto tn=curved->source_tangents[c];auto vn=normalized(curved->source_normals[c]),vt=normalized(Vec3{tn[0],tn[1],tn[2]});ni+=vn*bary[c];ti+=vt*bary[c];bi+=cross(vn,vt)*bary[c];}
  auto ts=mtoon_normal_map(encoded,.8f);auto curvedN=legacy?normalized(ti*ts.x+bi*ts.y+ni*ts.z):normalized(normalized(ti)*ts.x+normalized(cross(normalized(ni),normalized(ti)))*ts.y+normalized(ni)*ts.z);
  check(close(at(draw(renderer,curved),px,py),expected(m,{},curvedN),3),"Versioned original-frame interpolation agrees with independent legacy vertexTBN or modern fragmentTN evaluation");

  // Canonical baked detail has a distinct N/T basis; protect and blend in world space.
  auto raw=std::make_shared<PaintSourceMaps>();raw->color.image=image({255,255,255,255});raw->normal.image=image({255,128,128,255});raw->normal_scale=.8f;
  auto painter=PaintedMesh::from_maps(quad(),raw,64);BakedTextures base;base.maps.color=flat64({255,255,255,255});base.maps.metal_rough=flat64({255,204,0,255});base.maps.emission=flat64({0,0,0,255});base.maps.normal=flat64({128,204,230,255});base.coverage.resize(64*64);
  for(size_t j=0;j<base.coverage.size();++j)base.coverage[j]=j%64<22?0:j%64<43?128:255;
  painter->install_bake(base);auto baked=std::make_shared<MeshSnapshot>(*scene);baked->ranges[0].textures=painter->composite(white());baked->ranges[0].canonical_frame=true;
  auto bake_material=m;bake_material.parameters.base_color={1,1,1,1};baked->ranges[0].appearance=appearance(bake_material,{image({255,128,128,255})});
  baked->canonical_normals.assign(4,{0,0,1});baked->tangents.assign(4,{1,0,0,1});auto pixels=draw(renderer,baked);
  auto canonical=normalized(Vec3{128/255.f*2-1,204/255.f*2-1,230/255.f*2-1});
  for(auto [column,cov]:std::array<std::pair<int,float>,3>{{{24,0},{48,128/255.f},{72,1}}}) {
    auto mapped=normalized(wanted*(1-cov)+canonical*cov);
    auto got=at(pixels,column,48),want=expected(bake_material,{},mapped);
    if(!close(got,want,3))std::cerr<<"BLEND legacy="<<legacy<<" cov="<<cov<<" got="<<got[0]<<","<<got[1]<<","<<got[2]<<" want="<<want[0]<<","<<want[1]<<","<<want[2]<<" normal="<<mapped.x<<","<<mapped.y<<","<<mapped.z<<'\n';
    check(close(got,want,3),"Protected/fractional/full canonical detail blends independent source and baked frame normals in world space");
  }
  auto fullbase=base;fullbase.maps.color=flat64({170,149,124,255});fullbase.coverage.assign(64*64,255);auto fullpaint=painter->snapshot();fullpaint->install_bake(fullbase);
  auto curvedCanonical=std::make_shared<MeshSnapshot>(*baked);curvedCanonical->ranges[0].textures=fullpaint->composite(white());
  auto curvedMaterial=bake_material;curvedMaterial.parameters.base_color={1,1,1,1};curvedMaterial.parameters.shade_color={.01f,.02f,.04f};curvedMaterial.parameters.toony=.95f;curvedMaterial.parameters.shift=legacy?.6f:-.6f;
  curvedCanonical->ranges[0].appearance=appearance(curvedMaterial,{image({255,128,128,255})});
  curvedCanonical->canonical_normals={{0,0,1},normalized(Vec3{0,-.99f,.1f}),normalized(Vec3{.99f,0,.1f}),{0,0,1}};
  curvedCanonical->tangents.clear();Vec3 rawN{},rawT{},rawB{};
  for(size_t vertex=0;vertex<4;++vertex){auto vn=curvedCanonical->canonical_normals[vertex],vt=normalized(cross(Vec3{0,1,0},vn));curvedCanonical->tangents.push_back({vt.x,vt.y,vt.z,1});if(vertex<3){rawN+=vn*bary[vertex];rawT+=vt*bary[vertex];rawB+=cross(vn,vt)*bary[vertex];}}
  auto detail=Vec3{128/255.f*2-1,204/255.f*2-1,230/255.f*2-1};auto rawCanonical=normalized(rawT*detail.x+rawB*detail.y+rawN*detail.z);
  auto normalizedColumns=normalized(normalized(rawT)*detail.x+normalized(rawB)*detail.y+normalized(rawN)*detail.z);
  MToonSamples curvedSamples;curvedSamples.base={srgb_to_linear(170/255.f),srgb_to_linear(149/255.f),srgb_to_linear(124/255.f),1};curvedSamples.shade={curvedSamples.base[0],curvedSamples.base[1],curvedSamples.base[2]};
  check(length(rawCanonical-normalizedColumns)>.1f,"Curved canonical fixture independently distinguishes native raw-column bake evaluation from fragment-normalized columns");
  check(!close(expected(curvedMaterial,curvedSamples,rawCanonical),expected(curvedMaterial,curvedSamples,normalizedColumns),4),"Curved canonical fixture produces observably different lighting under the incorrect normalized-column policy");
  check(close(at(draw(renderer,curvedCanonical),px,py),expected(curvedMaterial,curvedSamples,rawCanonical),2),"Retained canonical normal detail preserves the native raw interpolated TBN bake convention on curved frames");
 }
 // Generic PBR uses independently interpolated original vertexB, matching the
 // core authored-attribute reference path rather than MToon1's cross-at-fragment.
 auto native=white();native.unlit=false;auto original=std::make_shared<PaintSourceMaps>();original->color.image=image({255,255,255,255});original->normal.image=image({255,128,128,255});original->normal_scale=.8f;
 auto paint=PaintedMesh::from_maps(quad(),original,64);auto pbr=snapshot(native);pbr->ranges[0].textures=paint->preview(native);frames(*pbr,N,T);
 auto coreN=source_expected(false,N,T,encoded,.8f);auto reference=snapshot(native);reference->mesh.normals.assign(4,coreN);
 check(delta(draw(renderer,pbr),draw(renderer,reference))<=3,"PBR original frame agrees with independent world-normal geometry under anisotropic skin");
 BakedTextures bake;bake.maps.color=flat64({255,255,255,255});bake.maps.metal_rough=flat64({255,204,0,255});bake.maps.emission=flat64({0,0,0,255});bake.maps.normal=flat64({128,204,230,255});bake.coverage.assign(64*64,128);paint->install_bake(bake);
 pbr=std::make_shared<MeshSnapshot>(*pbr);pbr->ranges[0].textures=paint->composite(native);pbr->ranges[0].canonical_frame=true;pbr->canonical_normals.assign(4,{0,0,1});pbr->tangents.assign(4,{1,0,0,1});
 auto canonical=normalized(Vec3{128/255.f*2-1,204/255.f*2-1,230/255.f*2-1}),combined=normalized(coreN*(1-128/255.f)+canonical*(128/255.f));reference=std::make_shared<MeshSnapshot>(*reference);reference->mesh.normals.assign(4,combined);
 check(delta(draw(renderer,pbr),draw(renderer,reference))<=3,"PBR raw source and canonical baked normal coverage blend in separate retained frames on GPU");
 // N-only imported lighting remains authoritative even with no normal map.
 for(bool legacy:{false,true}) {
  auto m=typed(legacy);auto only=snapshot();only->ranges[0].appearance=appearance(m);only->ranges[0].source_frame=true;only->ranges[0].source_normal_only=true;
  only->source_normals.assign(4,{.4f,.1f,1});
  check(close(at(draw(renderer,only)),expected(m,{},normalized(Vec3{.4f,.1f,1})),3),"Untextured V0/V1 lighting retains imported N-only authority without a tangent attribute");
 }
 {
  auto model=quad();auto raw=std::make_shared<PaintSourceMaps>();raw->color.image=image({255,255,255,255});raw->normal.image=image({255,128,128,255});raw->normal_scale=.8f;
  auto paint=PaintedMesh::from_maps(model,raw,64);auto origin=std::make_shared<SourceUvOrigin>();origin->paint=paint->snapshot();
  auto authority=std::make_shared<DeformationState>(model);authority->morphs=MorphSet(model);authority->morphs->set_target(model,"direction only",model.positions);origin->deformation=authority;
  auto native_frame=std::make_shared<NormalFrames>();native_frame->origin_vertex_count=4;native_frame->origin_vertices=model.indices;native_frame->normals.assign(6,{0,0,1});native_frame->tangents.assign(6,{1,0,0,1});
  native_frame->targets.push_back({1,std::vector<Vec3>(6,Vec3{1,0,0}),std::vector<Vec3>(6,Vec3{0,1,0})});
  auto registry=std::make_shared<NormalFrameSet>();registry->pbr=native_frame;registry->targets={{1,0,std::nullopt,{}}};registry->current_targets={1};registry->next_target=2;
  auto binding=std::make_shared<SourceMaterialBindings>();binding->has_mtoon=false;binding->uv_origin=origin;binding->normal_frames=registry;binding->normal_frame=SourceMaterialNormalFrame::OriginalAttributes;
  Scene scene;scene.active().painted=paint;scene.active().mesh_active=true;scene.active().material=native;scene.active().material_bindings=binding;
  auto current=*authority;current.morph_weights={.5f};current.skeleton=Skeleton{{{"root",-1,{},1}}};current.skin=SkinWeights{};current.skin->vertices.assign(4,{{0,1}});current.pose={{{},{},{2,1,.5f}}};scene.set_deformation(scene.active().id,current);
  Document document;document.replace(scene);check(document.wait_mesh(),"Actual asynchronous native worker publishes dynamic morph/anisotropic skin frames for the GPU");
  auto worker=document.frame().surface;auto baseline=draw(renderer,worker);auto uploads=renderer.buffer_stats();
  array_bytes=index_bytes=0;capture_buffers=true;auto stereo=draw(renderer,worker);capture_buffers=false;
  check(stereo==baseline && array_bytes==0 && index_bytes==0,"Original source direction attributes share the immutable geometry stream across stereo views without an upload");
  auto masked=std::make_shared<MeshSnapshot>(*worker);masked->sculpt_masks.assign(masked->mesh.positions.size(),.7f);array_bytes=index_bytes=0;capture_buffers=true;auto protected_pixels=draw(renderer,masked);capture_buffers=false;
  check(protected_pixels!=baseline && array_bytes==masked->mesh.positions.size()*sizeof(float) && index_bytes==0 && renderer.buffer_stats().geometry_uploads==uploads.geometry_uploads,"Mask-only publication changes feedback while uploading only the scalar buffer, preserving source and canonical direction streams");
  check(draw(renderer,worker)==baseline && renderer.buffer_stats().geometry_uploads==uploads.geometry_uploads,"Releasing protection feedback restores exact retained-frame pixels without rebuilding static geometry");
  auto independent=std::make_shared<MeshSnapshot>(*worker);independent->geometry_identity.reset();independent->ranges[0].source_frame=false;independent->ranges[0].material_bindings.reset();independent->ranges[0].textures.reset();
  auto expectedNormal=source_expected(false,{.25f,0,2},{2,.5f,0},encoded,.8f);independent->mesh.normals.assign(independent->mesh.positions.size(),expectedNormal);
  check(delta(draw(renderer,worker),draw(renderer,independent))<=3,"Actual worker-to-GPU normal transport agrees with independent posed-geometry normal consumer");
  auto restarted=Scene::decode(nlohmann::json::from_cbor(nlohmann::json::to_cbor(scene.encode())));
  Document reopen;reopen.replace(restarted);check(reopen.wait_mesh() && delta(draw(renderer,worker),draw(renderer,reopen.frame().surface))<=1,"Scene9 restart regenerates identical dynamic original-frame GPU output");
 }
 {
  auto ordinary=snapshot();draw(renderer,ordinary);check(renderer.buffer_stats().vertex_bytes<=ordinary->mesh.positions.size()*64,"Ordinary sculpt geometry retains its compact base vertex without allocating unused retained-frame directions");
  auto scene=normal_frame_fixture::cube();Document document;document.replace(scene);check(document.wait_mesh(),"Real retained-frame cube reaches the worker before measured clip and cap transitions");auto base=document.frame().surface;
  auto measured=[&](std::shared_ptr<const MeshSnapshot> snapshot){array_bytes=index_bytes=0;capture_buffers=true;auto pixels=draw(renderer,std::move(snapshot));capture_buffers=false;return pixels;};
  measured(base);const auto base_bytes=renderer.buffer_stats().vertex_bytes;check(array_bytes>base_bytes && index_bytes>0,"Initial frame geometry allocates and uploads both base and optional direction blocks through actual GL writes");
  document.begin_cut();check(document.wait_cut(),"Real retained-frame cap preview completes for traffic measurement");auto capped=document.frame().surface;auto cap_pixels=measured(capped);check(array_bytes>0 && index_bytes>0 && capped->mesh.positions.size()>base->mesh.positions.size() && !capped->ranges.back().source_frame,"Appending a generic cap rebuilds the owned static block and does not assign original frame authority to cap vertices");
  check(measured(capped)==cap_pixels && array_bytes==0 && index_bytes==0,"Second-eye capped preview reuses base, direction, index and scalar buffers without Data or SubData writes");
  auto settings=document.frame().state->cut->settings;settings.cap=false;document.edit_cut(settings);check(document.wait_cut(),"Retained-frame source returns to a clip-only preview");auto clipped=document.frame().surface;auto before=measured(clipped);check(array_bytes>0 && index_bytes>0 && renderer.buffer_stats().vertex_bytes==base_bytes,"Removing the cap restores precisely the original base and direction allocation");
  settings.offset_mm+=50;document.edit_cut(settings);check(document.wait_cut(),"Clip-only plane movement completes without modifying frame geometry");auto moved=measured(document.frame().surface);check(moved!=before && array_bytes==0 && index_bytes==0,"Changing the clip plane changes actual pixels using GPU uniforms with no base, frame or scalar buffer writes");
  document.end_cut(false);measured(document.frame().surface);check(array_bytes==0 && index_bytes==0 && document.copy_scene().encode()==scene.encode(),"Cancelling the clip-only draft restores the exact native frame source without uploading unchanged geometry");
 }
 std::cout<<"Original frame renderer checks "<<checks<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
