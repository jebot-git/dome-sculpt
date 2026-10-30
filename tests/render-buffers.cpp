// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_material_reference_main
#include "mtoon-render.cpp"
#undef main
#include <limits>
namespace {
struct Traffic {size_t vertex_bytes=0,index_bytes=0;unsigned vertex_calls=0,index_calls=0;};
Traffic traffic;
bool capture=false;
}
extern "C" void __real_glBufferData(GLenum,GLsizeiptr,const void*,GLenum);
extern "C" void __wrap_glBufferData(GLenum target,GLsizeiptr size,const void *data,GLenum usage) {
  if(capture && target==GL_ARRAY_BUFFER){traffic.vertex_bytes+=size;traffic.vertex_calls++;}
  if(capture && target==GL_ELEMENT_ARRAY_BUFFER){traffic.index_bytes+=size;traffic.index_calls++;}
  __real_glBufferData(target,size,data,usage);
}
int main(int argc,char **argv)try {
  NativeGraphics graphics;std::string device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));
  std::cout<<"GPU "<<device<<'\n';
  check(argc<2||std::string(argv[1])!="--hardware"||
        (device.find("llvmpipe")==std::string::npos&&device.find("softpipe")==std::string::npos),
        "Buffer reuse executes on requested real hardware");
  Fbo framebuffer;framebuffer.bind();SurfaceRenderer renderer;
  auto render=[&](std::shared_ptr<const MeshSnapshot> scene,CanvasTransform canvas=CanvasTransform{}) {
    traffic={};capture=true;
    try {renderer.draw(std::move(scene),canvas,identity,{0,0,2},Fbo::size,Fbo::size,true);}
    catch(...) {capture=false;throw;}
    capture=false;return read();
  };
  auto scene=snapshot();scene->generation=7;scene->geometry_identity=std::make_shared<const int>(1);
  const auto baseline=render(scene);auto first=renderer.buffer_stats();
  check(traffic.vertex_bytes>0&&traffic.index_bytes==6*sizeof(uint32_t)&&first.geometry_uploads==1,
        "First immutable geometry actually uploads vertex and index buffers");
  check(render(scene)==baseline&&traffic.vertex_calls==0&&traffic.index_calls==0,
        "Second stereo view issues no geometry or scalar uploads");
  auto masks=std::make_shared<MeshSnapshot>(*scene);masks->sculpt_masks.assign(4,1);
  auto protected_pixels=render(masks);
  check(traffic.vertex_bytes==4*sizeof(float)&&traffic.index_calls==0&&renderer.buffer_stats().geometry_uploads==1,
        "Protection-only publication uploads exactly one scalar per vertex and no geometry");
  check(at(protected_pixels)[0]<at(baseline)[0]-40&&at(protected_pixels)[3]==255,
        "Separate mask buffer drives actual shader feedback without changing opacity");
  auto weights=std::make_shared<MeshSnapshot>(*masks);weights->selected_weights.assign(4,.5f);
  auto heatmap=render(weights);
  check(traffic.vertex_bytes==8*sizeof(float)&&traffic.index_calls==0&&renderer.buffer_stats().geometry_uploads==1,
        "Weight and mask scalars remain separate from retained geometry");
  check(close(at(heatmap),{0,255,0,255}),"Weight buffer retains its exact green midpoint and priority");
  auto recolored=std::make_shared<MeshSnapshot>(*scene);recolored->ranges[0].material.color={1,0,0,1};
  auto red=render(recolored);
  check(traffic.vertex_bytes==0&&traffic.index_calls==0&&renderer.buffer_stats().weight_bytes==0&&renderer.buffer_stats().mask_bytes==0,
        "Material-only publication releases obsolete scalar stores without uploading geometry");
  check(close(at(red),{255,0,0,255}),"Retained geometry binds changed material factors correctly");
  glVertexAttrib1f(4,.75f);glVertexAttrib1f(6,1);
  check(render(recolored)==red&&traffic.vertex_calls==0,
        "Interleaved context attribute constants cannot leak into disabled scalar arrays");
  auto textured=std::make_shared<MeshSnapshot>(*scene);
  auto maps=std::make_shared<PaintSourceMaps>();maps->color.image=image({0,0,255,255});
  auto painter=PaintedMesh::from_maps(quad(),maps,64);textured->ranges[0].textures=painter->preview(white());
  auto blue=render(textured);
  check(close(at(blue),{0,0,255,255})&&traffic.vertex_calls==0&&traffic.index_calls==0,
        "Changed original texture data binds without repacking or uploading geometry");
  auto texture_uploads=renderer.texture_stats().uploads;
  CanvasTransform canvas;canvas.position={.2f,0,0};
  auto moved=render(textured,canvas);
  check(moved!=blue&&traffic.vertex_calls==0&&traffic.index_calls==0&&renderer.texture_stats().uploads==texture_uploads,
        "Canvas navigation uses GPU transforms with no geometry or texture upload");
  auto resized=std::make_shared<MeshSnapshot>(*textured);
  for(auto &p:resized->mesh.positions)p=p*.5f;
  resized->geometry_identity=std::make_shared<const int>(2);
  auto small=render(resized);
  check(traffic.vertex_bytes>0&&traffic.index_bytes==6*sizeof(uint32_t)&&at(small,24,48)[3]==0&&at(blue,24,48)[3]!=0,
        "Changed geometry identity refreshes actual vertex and index contents");
  auto generation=std::make_shared<MeshSnapshot>(*resized);generation->generation++;
  render(generation);
  check(traffic.vertex_bytes>0&&traffic.index_calls==1,
        "A new document generation cannot reuse a stale geometry identity");
  int address=0;
  auto alias1=std::make_shared<MeshSnapshot>(*scene);
  alias1->geometry_identity=std::shared_ptr<const void>(&address,[](const void*){});
  render(alias1);
  auto alias2=std::make_shared<MeshSnapshot>(*alias1);
  alias2->geometry_identity=std::shared_ptr<const void>(&address,[](const void*){});
  render(alias2);
  check(traffic.vertex_bytes>0&&traffic.index_calls==1,
        "Equal raw addresses with different ownership control blocks do not alias geometry");
  auto invalid=std::make_shared<MeshSnapshot>(*alias2);invalid->mesh.indices.resize(3);invalid->ranges[0].count=3;
  check(rejects([&]{render(invalid);}),"Reused geometry with changed packed counts rejects before publication");
  check(render(alias2)==baseline&&traffic.index_calls==1,
        "Rejected geometry publication invalidates the cache and recovers the previous valid scene");
  invalid=std::make_shared<MeshSnapshot>(*alias2);invalid->sculpt_masks.assign(4,std::numeric_limits<float>::quiet_NaN());
  check(rejects([&]{render(invalid);}),"Invalid scalar data cannot enter a retained geometry buffer");
  check(render(alias2)==baseline,"Scalar failure retains recoverable source geometry and material data");
  auto legacy=std::make_shared<MeshSnapshot>(*scene);legacy->geometry_identity.reset();render(legacy);
  auto legacy_copy=std::make_shared<MeshSnapshot>(*legacy);render(legacy_copy);
  check(traffic.vertex_bytes>0&&traffic.index_calls==1,
        "Snapshots without a geometry token retain conservative upload behavior");
  auto uv1_scene=snapshot();auto toon=typed(false);
  toon.textures[0].index=0;toon.textures[0].texcoord=1;
  auto split=std::make_shared<const Image>(Image{2,1,{255,0,0,255,0,0,255,255}});
  uv1_scene->ranges[0].appearance=appearance(toon,{split});
  uv1_scene->geometry_identity=std::make_shared<const int>(4);
  auto uv1_before=render(uv1_scene);
  check(at(uv1_before,36,48)[2]>at(uv1_before,36,48)[0]+40&&at(uv1_before,60,48)[2]>at(uv1_before,60,48)[0]+40,
        "Independent authored UV1 selects the blue source half across the whole primitive");
  auto uv1_factor=std::make_shared<MeshSnapshot>(*uv1_scene);
  uv1_factor->ranges[0].material.color={.5f,.5f,.5f,1};
  auto uv1_after=render(uv1_factor);
  check(traffic.vertex_calls==0&&traffic.index_calls==0&&at(uv1_after,36,48)[2]>at(uv1_after,36,48)[0]+40&&at(uv1_after,60,48)[2]>at(uv1_after,60,48)[0]+40,
        "Retained UV1 geometry survives native MToon factor updates without repacking");
  auto empty=std::make_shared<MeshSnapshot>();empty->geometry_identity=std::make_shared<const int>(3);
  auto blank=render(empty);
  check(std::all_of(blank.begin(),blank.end(),[](auto value){return value==0;})&&renderer.buffer_stats().vertex_bytes==0&&renderer.buffer_stats().index_bytes==0,
        "An empty scene releases geometry stores and cannot draw stale indices");
  check(glGetError()==GL_NO_ERROR,"Separate-buffer rendering leaves the GL error queue clean");
  std::cout<<"GPU buffer reuse complete: "<<checks<<" checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
