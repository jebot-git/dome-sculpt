// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_material_reference_main
#include "mtoon-render.cpp"
#undef main
#include <unistd.h>
namespace {
size_t vertex_bytes=0,index_calls=0;
bool capture=false;
struct Home {
  std::filesystem::path path;
  Home() {
    std::string pattern=(std::filesystem::temp_directory_path()/"dome-doc-gpu-XXXXXX").string();
    std::vector<char> text(pattern.begin(),pattern.end());text.push_back(0);
    auto result=mkdtemp(text.data());if(!result)throw std::runtime_error("Cannot isolate document GPU test");path=result;
    setenv("HOME",path.c_str(),1);
    for(auto [key,folder]:std::initializer_list<std::pair<const char*,const char*>>{{"XDG_STATE_HOME","state"},{"XDG_CONFIG_HOME","config"},{"XDG_CACHE_HOME","cache"},{"XDG_DATA_HOME","data"}}) {
      auto value=path/folder;std::filesystem::create_directory(value);setenv(key,value.c_str(),1);
    }
    setenv("XDG_RUNTIME_DIR",path.c_str(),1);
  }
  ~Home(){std::error_code ignored;std::filesystem::remove_all(path,ignored);}
};
}
extern "C" void __real_glBufferData(GLenum,GLsizeiptr,const void*,GLenum);
extern "C" void __wrap_glBufferData(GLenum target,GLsizeiptr size,const void *data,GLenum usage) {
  if(capture&&target==GL_ARRAY_BUFFER)vertex_bytes+=size;
  if(capture&&target==GL_ELEMENT_ARRAY_BUFFER)index_calls++;
  __real_glBufferData(target,size,data,usage);
}
int main(int argc,char **argv)try {
  Home home;NativeGraphics graphics;
  std::string device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GPU "<<device<<'\n';
  check(argc<2||std::string(argv[1])!="--hardware"||(device.find("llvmpipe")==std::string::npos&&device.find("softpipe")==std::string::npos),"Document buffer fixture executes on requested GPU hardware");
  Fbo fbo;fbo.bind();SurfaceRenderer renderer;Document document;
  Volume volume(.05f);Brush seed;seed.radius=.3f;seed.blend=0;
  volume.begin_stroke();volume.stamp(seed);volume.end_stroke();
  Scene source;source.active().volume=std::make_shared<Volume>(volume.geometry_snapshot());source.active().material=white();document.replace(std::move(source));
  auto ready=[&] {
    if(!document.wait_mesh()||!document.frame().state->error.empty())throw std::runtime_error("Document did not publish a valid current mesh: "+document.frame().state->error);
    return document.frame().surface;
  };
  auto render=[&](std::shared_ptr<const MeshSnapshot> scene) {
    vertex_bytes=index_calls=0;capture=true;
    try {renderer.draw(std::move(scene),{},identity,{0,0,2},Fbo::size,Fbo::size,true);}catch(...){capture=false;throw;}
    capture=false;return read();
  };
  auto original=ready();auto original_key=original->geometry_identity;
  check(bool(original_key),"Actual Document publishes an owned geometry identity");
  auto baseline=render(original);auto uploads=renderer.buffer_stats().geometry_uploads;
  auto settings=document.frame().state->sculpt_settings;settings.mask_edit=true;document.sculpt_settings(settings);
  auto controls=ready();render(controls);
  check(controls->geometry_identity==original_key&&controls->sculpt_meshing.cells_evaluated==0&&controls->sculpt_meshing.materialize_ms==0,
        "Mask controls retain actual field geometry without tile extraction or full assembly");
  check(index_calls==0&&vertex_bytes==controls->mesh.positions.size()*sizeof(float)&&renderer.buffer_stats().geometry_uploads==uploads,
        "Document mask control publication uploads only its scalar stream");
  document.sculpt_mask_fill(1);if(!document.wait_asset())throw std::runtime_error("Global protection worker timeout");
  auto filled=ready();auto dark=render(filled);
  check(filled->geometry_identity==original_key&&index_calls==0&&vertex_bytes==filled->mesh.positions.size()*sizeof(float),
        "Real background Fill retains geometry and transfers one mask scalar per vertex");
  check(at(dark)[0]<at(baseline)[0]-40&&at(dark)[3]==at(baseline)[3],"Actual persisted global protection darkens the rendered model");
  check(document.undo(),"Worker Fill is undoable through the actual Document");
  auto restored=ready();
  check(restored->geometry_identity==original_key&&render(restored)==baseline&&index_calls==0,
        "Document Undo restores exact pixels without reuploading geometry");
  settings=document.frame().state->sculpt_settings;settings.mask_visible=false;settings.mask_edit=false;document.sculpt_settings(settings);render(ready());
  auto info=document.frame().state->objects.front();auto material=info.material;material.color={1,0,0,1};
  document.object_properties(info.id,info.name,true,false,info.transform,material);
  auto recolored=ready();auto red=render(recolored);
  check(recolored->geometry_identity==original_key&&vertex_bytes==0&&index_calls==0&&close(at(red),{255,0,0,255}),
        "Actual material-only authoring changes pixels with no vertex or index transfer");
  Brush carve;carve.from=carve.to={.25f,0,0};carve.radius=.25f;carve.blend=0;carve.tool=Tool::Carve;
  if(!document.begin_desktop())throw std::runtime_error("Cannot start real geometry gesture");
  document.stamp_desktop(carve);document.end_desktop();
  auto sculpted=ready();auto sculpted_pixels=render(sculpted);
  check(sculpted->geometry_identity!=original_key&&sculpted->sculpt_meshing.local_updates==1&&sculpted->sculpt_meshing.full_rebuilds==0,
        "Actual sculpt gesture authenticates a local tile update and invalidates GPU geometry");
  check(vertex_bytes>0&&index_calls==1&&sculpted_pixels!=red,"Changed field geometry actually refreshes GPU buffers and visible coverage");
  auto sculpted_key=sculpted->geometry_identity;auto transform=info.transform;transform.position.x=.12f;
  document.object_properties(info.id,info.name,true,false,transform,material);
  auto moved=ready();render(moved);
  check(moved->geometry_identity!=sculpted_key&&moved->sculpt_meshing.cells_evaluated==0&&vertex_bytes>0&&index_calls==1,
        "Object motion reuses field extraction while refreshing packed world-space geometry");
  Scene painted;painted.active().painted=std::make_shared<PaintedMesh>(quad(),64);painted.active().mesh_active=true;painted.active().material=white();document.replace(std::move(painted));
  auto before_paint=ready();auto paint_key=before_paint->geometry_identity;auto white_pixels=render(before_paint);auto texture_uploads=renderer.texture_stats().uploads;
  auto ink=document.frame().state->paint;ink.color={0,0,1,1};ink.channel=PaintChannel::Color;document.paint_settings(ink);
  Brush paint;paint.radius=.4f;paint.normal={0,0,1};paint.blend=0;
  if(!document.begin_desktop())throw std::runtime_error("Cannot start real texture gesture");
  document.stamp_desktop(paint);document.end_desktop();
  auto after_paint=ready();auto blue=render(after_paint);
  check(after_paint->geometry_identity==paint_key&&vertex_bytes==0&&index_calls==0,"Real layered painting retains packed mesh geometry and its GPU buffers");
  check(blue!=white_pixels&&at(blue)[2]>at(blue)[0]+40&&renderer.texture_stats().uploads>texture_uploads,
        "Actual texture gesture uploads changed material pixels and renders the new ink");
  check(glGetError()==GL_NO_ERROR,"Actual Document-to-GPU authoring leaves the GL error queue clean");
  std::cout<<"Document GPU reuse complete: "<<checks<<" checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
