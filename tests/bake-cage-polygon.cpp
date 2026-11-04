// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_BAKE_CAGE_HELPERS_ONLY
#include "bake-cage.cpp"
#include "dome/polygon-scene.hpp"
int main(int argc,char **argv)try{
  const auto out=std::filesystem::path(argc>1?argv[1]:"bake-cage-polygon");
  std::filesystem::create_directories(out);
  auto scene=cage_scene();
  scene.object(3).painted=std::make_shared<PaintedMesh>(scene.active().painted->mesh(),64);
  scene.object(3).visible=true;scene.object(3).locked=false;scene.select(3);
  Document document;document.replace(scene);ready_cage(document);
  auto ready=[&]{require_cage(document.wait_polygon()&&document.frame().state->error.empty(),
    "Real polygon worker completes without an authoring error");};
  document.begin_polygon();ready();
  document.edit_polygon(PolygonAction::Vertices);ready();
  document.edit_polygon(PolygonAction::SelectAll);ready();
  document.polygon_pointer({-.5f,0,1},{.5f,0,-1},PolygonAction::BeginDrag);ready();
  document.polygon_pointer({-.44f,0,1.03f},{.5f,0,-1},PolygonAction::Drag);ready();
  document.polygon_pointer({-.44f,0,1.03f},{.5f,0,-1},PolygonAction::EndDrag);ready();
  require_cage(document.frame().state->polygon->changed,
    "Controller-compatible pointer gesture genuinely moves the selected cage vertices");
  document.end_polygon(true);ready();ready_cage(document);
  const auto edited=document.copy_scene();
  require_cage(edited.active().polygon&&edited.active().polygon->origin&&
    edited.active().painted->mesh().positions.size()==6,
    "Native Apply retains source identity and publishes its real split-corner derivative");
  std::cout<<"CAGE packed vertices "<<edited.active().painted->mesh().positions.size()<<std::endl;
  document.select_object(1);ready_cage(document);
  const auto before=document.copy_scene().encode();
  document.bake_from_object(2,cage_settings());
  auto save=document.save_async(out/"edited-cage.dsculpt");save.get();ready_cage(document);
  require_cage(document.frame().state->error.empty()&&document.copy_scene().active().painted->baked(),
    "Real polygon-edited cage is usable by the native Bake worker");
  const auto baked=document.copy_scene();
  const auto &pixels=baked.active().painted->baked()->maps.color.rgba;
  bool blue=true;for(size_t i=0;i<pixels.size();i+=4)blue&=pixels[i]==0&&pixels[i+1]==0&&pixels[i+2]==255&&pixels[i+3]==255;
  require_cage(blue,"Polygon-edited cage reaches every independently located off-axis blue source texel");
  require_cage(baked.encode()["objects"][2]==before["objects"][2]&&
    Scene::load(out/"edited-cage.dsculpt").encode()==baked.encode(),
    "Actual cage topology, retained source and ordered native Save remain exact");
  require_cage(document.undo()&&document.copy_scene().encode()==before&&
    document.redo()&&document.copy_scene().encode()==baked.encode(),
    "Polygon cage Bake is exactly one Undo/Redo transaction");
  std::cout<<"PASS "<<cage_checks<<" polygon cage checks\n";return 0;
}catch(const std::exception &error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
