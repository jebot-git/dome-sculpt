// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_DERIVED_KEY_RENDER_NO_MAIN
#include "independent-seam-key-render.cpp"
#include "key-mirror-correspondence.hpp"

namespace {
thread_local size_t replay_calls=0;
const auto test_thread=std::this_thread::get_id();
std::atomic_bool cancellable_preview_replay=false;
}

extern "C" detail::KeyMirrorCorrespondence real_mirror(const Mesh &,detail::KeyMirrorPlane,float,
    const detail::KeyMirrorLimits &) asm("__real__ZN4dome6detail28reflected_key_correspondenceERKNS_4MeshENS0_14KeyMirrorPlaneEfRKNS0_15KeyMirrorLimitsE");
extern "C" detail::KeyMirrorCorrespondence observe_mirror(const Mesh &,detail::KeyMirrorPlane,float,
    const detail::KeyMirrorLimits &) asm("__wrap__ZN4dome6detail28reflected_key_correspondenceERKNS_4MeshENS0_14KeyMirrorPlaneEfRKNS0_15KeyMirrorLimitsE");
extern "C" detail::KeyMirrorCorrespondence observe_mirror(const Mesh &mesh,detail::KeyMirrorPlane plane,
    float tolerance,const detail::KeyMirrorLimits &limits) {
  ++replay_calls;
  if(std::this_thread::get_id()!=test_thread&&limits.cancelled)cancellable_preview_replay=true;
  return real_mirror(mesh,plane,tolerance,limits);
}

namespace {
void check_mirror_preview(Scene source,Fbo &fbo) {
  Document doc;
  doc.replace(source);
  settled(doc);
  const auto first=doc.frame().surface;
  check(first->ranges.size()==1&&first->ranges.front().key_material_preview&&cancellable_preview_replay,
      "The document mesh worker authenticates the prepared Mirror with a cancellation callback");
  doc.shading(false);
  settled(doc);
  const auto current=doc.frame().surface;
  check(current!=first&&first->ranges.front().key_material_preview==current->ranges.front().key_material_preview,
      "View publication reuses verification for the exact same immutable material and textures");

  fbo.bind();
  SurfaceRenderer renderer;
  replay_calls=0;
  const auto worker_pixels=draw(renderer,current);
  MaterialSurfaceQuery query(current->mesh,current->ranges);
  check(replay_calls==0,"Cold rendering and picking consume the worker proof without replaying Mirror");

  auto raw=std::make_shared<MeshSnapshot>(*current);
  raw->ranges.front().key_material_preview.reset();
  fbo.bind();
  SurfaceRenderer raw_renderer;
  const auto raw_pixels=draw(raw_renderer,raw);
  check(replay_calls>0&&raw_pixels==worker_pixels,
      "A standalone range replays its geometry proof and produces identical pixels");
  std::cout<<"STANDALONE_MIRROR_REPLAYS "<<replay_calls<<'\n';

  auto borrowed=std::make_shared<MeshSnapshot>(*current);
  borrowed->ranges.front().textures=std::make_shared<TextureMaps>(*current->ranges.front().textures);
  fbo.bind();
  SurfaceRenderer borrowed_renderer;
  check(rejects([&]{draw(borrowed_renderer,borrowed);})&&rejects([&]{
    MaterialSurfaceQuery wrong(borrowed->mesh,borrowed->ranges);
  }),"Render and picking refuse the proof on a byte-identical foreign texture owner");
  borrowed->ranges.front()=current->ranges.front();
  borrowed->ranges.front().material_bindings=std::make_shared<SourceMaterialBindings>(*current->ranges.front().material_bindings);
  check(rejects([&]{draw(borrowed_renderer,borrowed);})&&rejects([&]{
    MaterialSurfaceQuery wrong(borrowed->mesh,borrowed->ranges);
  }),"Render and picking refuse the proof on a byte-identical foreign key material");
  borrowed->ranges.front()=current->ranges.front();
  borrowed->ranges.front().material_bindings=std::shared_ptr<const SourceMaterialBindings>(
      current->ranges.front().material_bindings.get(),[](const SourceMaterialBindings *){});
  check(rejects([&]{MaterialSurfaceQuery wrong(borrowed->mesh,borrowed->ranges);}),
      "A borrowed binding address cannot replace its actual ownership lease");

  const auto &object=source.active();
  auto moved=object.transform;
  moved.position.x+=.05f;
  doc.object_properties(object.id,object.name,object.visible,object.locked,moved,object.material);
  settled(doc);
  const auto placed=doc.frame().surface;
  check(placed->mesh.positions.front().x!=current->mesh.positions.front().x&&
      placed->ranges.front().key_material_preview==current->ranges.front().key_material_preview,
      "A real geometry reposition reuses the exact material verification");
  fbo.bind();
  replay_calls=0;
  draw(renderer,placed);
  check(replay_calls==0,"A geometry upload on the render thread requires no Mirror replay");

  doc.replace(source);
  settled(doc);
  const auto replacement=doc.frame().surface;
  check(replacement->generation!=current->generation&&replacement->ranges.front().key_material_preview&&
      replacement->ranges.front().key_material_preview!=current->ranges.front().key_material_preview,
      "A new document generation receives its own worker verification");
}
}
int main(int argc,char**argv)try{if(argc<3||argc>4||(argc==4&&std::string_view(argv[3])!="--hardware"))return 2;const bool hardware=argc==4;const std::filesystem::path out=argv[1];NativeGraphics graphics;const std::string device=reinterpret_cast<const char*>(glGetString(GL_RENDERER)),vendor=reinterpret_cast<const char*>(glGetString(GL_VENDOR));std::cout<<"GL_VENDOR "<<vendor<<"\nGL_RENDERER "<<device<<std::endl;const bool software=device.find("llvmpipe")!=std::string::npos||device.find("softpipe")!=std::string::npos;check(hardware?(vendor.find("Intel")!=std::string::npos&&device.find("Arc")!=std::string::npos&&device.find("A770")!=std::string::npos&&device.find("UHD")==std::string::npos&&!software):software,"Prepared Mirror graphics use the explicitly requested actual device");auto source=Scene::load(argv[2]);check(source.active().painted->mesh().positions.size()==12&&source.active().material_bindings->uv_origin->paint->mesh().positions.size()==5,"Genuine prepared Mirror keeps current12 and original5 frame domains distinct");Fbo fbo;fbo.bind();check_mirror_preview(source,fbo);SurfaceRenderer renderer;prepared_normal_consumer::Renderer external;rendered(source,false,out,renderer,fbo,external);
 pose(source,1,1);Document doc;doc.replace(source);settled(doc);doc.shading(false);settled(doc);const auto exact=doc.copy_scene().encode();const auto path=out/"normal-sensitivity.glb";doc.export_asset(path,AssetExportFormat::Glb).get();prepared_normal_consumer::Model model(path);fbo.bind();external.render(model.data);const auto full=read();auto &normal=model.data->meshes[0].primitives[0].material->normal_texture;check(normal.texture!=nullptr&&normal.scale>0,"Prepared Mirror export owns its actual positive normal map");const auto strength=normal.scale;normal.scale=0;fbo.bind();external.render(model.data);const auto difference=delta(full,read());normal.scale=strength;std::cout<<"MIRROR_NORMAL_MAP_SENSITIVITY "<<difference<<std::endl;check(difference>2,"Independent emitted GLB pixels visibly require the nonconstant normal field");check(doc.copy_scene().encode()==exact,"Sensitivity probe leaves actual native source and material untouched");std::cout<<"PASS "<<checks<<" prepared Mirror graphical checks\n";return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
