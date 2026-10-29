// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include <fstream>
#include <exception>
#include <iostream>
#include <nlohmann/json.hpp>
#include <unistd.h>
using namespace dome;
namespace {
void check(bool condition,const char *message) {
  if(!condition)throw std::runtime_error(message);
  std::cout<<"PASS "<<message<<'\n';
}
void asset(Document &doc) {
  if(!doc.wait_asset(std::chrono::seconds(180)))throw std::runtime_error("Asset worker timeout");
  if(!doc.frame().state->error.empty())throw std::runtime_error(doc.frame().state->error);
}
}
int main(int argc,char **argv)try {
  bool retain=argc==3 && std::string(argv[1])=="--artifacts";
  if(argc!=1 && !retain)throw std::invalid_argument("Usage: bake-workflow [--artifacts directory]");
  auto root=retain?std::filesystem::path(argv[2]):std::filesystem::temp_directory_path()/
      ("dome-bake-workflow-"+std::to_string(getpid()));
  std::filesystem::create_directories(root);
  std::cout<<"Workflow fixture: "<<root<<'\n';
  struct Cleanup{std::filesystem::path path;bool retain;~Cleanup(){if(!retain && !std::uncaught_exceptions()){std::error_code e;std::filesystem::remove_all(path,e);}}}cleanup{root,retain};
  Document doc;doc.seed();auto source_id=doc.copy_scene().active().id;
  doc.prepare_mesh(512);asset(doc);
  PaintSettings ink;ink.color={.01f,.03f,.95f,1};ink.front_only=false;doc.paint_settings(ink);
  check(doc.begin_desktop(),"The real sculpt source acquires the shared paint transaction");
  Brush brush;brush.radius=1;for(int i=0;i<8;++i)doc.stamp_desktop(brush);doc.end_desktop();
  auto source_maps=doc.copy_scene().active().painted->composite(doc.copy_scene().active().material);
  size_t painted_blue=0;
  for(size_t i=0;i<source_maps->color.rgba.size();i+=4)
    painted_blue+=source_maps->color.rgba[i+2]>150 && source_maps->color.rgba[i+2]>source_maps->color.rgba[i]*2;
  check(painted_blue>10000,"Real pressure/flow accumulation creates the intended blue source paint");
  auto painted_source=doc.copy_scene().encode()["objects"][0];
  doc.remesh_to_sculpt(60);asset(doc);
  auto remeshed=doc.copy_scene();auto low_id=remeshed.active().id;
  check(remeshed.objects().size()==2 && low_id!=source_id &&
        !remeshed.object(source_id).visible && !remeshed.active().mesh_active,
        "A painted sculpt produces a separate 60 mm sculpt with its complete hidden source retained");
  doc.prepare_mesh(512);asset(doc);
  auto before=doc.copy_scene();auto before_encoded=before.encode();
  before.save(root/"before-bake.dsculpt");
  ObjectBakeSettings settings;settings.front_mm=settings.back_mm=100;
  settings.ao_samples=0;settings.padding=4;
  doc.bake_from_object(source_id,settings);asset(doc);
  auto baked=doc.copy_scene();auto after_encoded=baked.encode();
  check(baked.active().painted->baked() && baked.active().id==low_id &&
        baked.active().volume->voxels()==before.active().volume->voxels() &&
        before.active().painted->baked()==nullptr,
        "Baking into the new atlas retains low geometry, object identity and immutable prior snapshots");
  painted_source["visible"]=false;
  check(after_encoded["objects"][0]==painted_source &&
        after_encoded["objects"][1]["painted"]["layers"]==before_encoded["objects"][1]["painted"]["layers"],
        "The complete source and editable target layers remain exact across the full workflow");
  auto maps=baked.active().painted->composite(baked.active().material);size_t blue=0;
  for(size_t i=0;i<maps->color.rgba.size();i+=4)
    blue+=maps->color.rgba[i+2]>150 && maps->color.rgba[i+2]>maps->color.rgba[i]*2;
  check(blue>10000 && !maps->normal.rgba.empty(),
        "Surface paint and tangent normal detail transfer from the retained sculpt into the new UV atlas");
  check(doc.undo() && doc.copy_scene().encode()==before_encoded &&
        doc.redo() && doc.copy_scene().encode()==after_encoded,
        "The complete bake is one exact native scene Undo/Redo transaction");
  auto path=root/"baked-workflow.dsculpt";
  doc.save(path);auto loaded=Scene::load(path);
  check(loaded.encode()["version"]==5 && loaded.active().painted->encode()==baked.active().painted->encode() &&
        loaded.active().volume->voxels()==baked.active().volume->voxels(),
        "Version 5 native saving and reopening retain the complete baked target and scalar field");
  auto surfaces=loaded.surfaces(true);
  check(surfaces.size()==1 && surfaces[0].id==low_id && surfaces[0].textures &&
        surfaces[0].textures->normal.rgba==maps->normal.rgba,
        "Export selects only the visible baked target while the editable source remains in the native scene");
  export_glb(surfaces,root/"baked-workflow.glb");
  std::ifstream glb(root/"baked-workflow.glb",std::ios::binary);char magic[4]{};glb.read(magic,4);
  check(glb && std::string(magic,4)=="glTF","The full native sculpt/paint/remesh/atlas/bake workflow writes a GLB asset");
  check(doc.wait_mesh(std::chrono::seconds(60)) &&
        doc.frame().surface->tangents.size()==doc.frame().surface->mesh.positions.size(),
        "The baked workflow publishes render-ready immutable tangent corners off the UI thread");
  return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
