// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/document.hpp"
#include "dome/polygon-scene.hpp"
#include <nlohmann/json.hpp>
namespace polygon_test {
using namespace dome;
inline dome::Scene fixture(){
 Scene s;Volume sculpt(.075f);sculpt.begin_stroke();Brush brush;brush.radius=.3f;brush.blend=0;sculpt.stamp(brush);sculpt.end_stroke();
 auto detail=sculpt.add_layer("Retained hidden detail");sculpt.begin_stroke();brush.from=brush.to={.08f,0,.15f};brush.radius=.14f;sculpt.stamp(brush);sculpt.end_stroke();
 sculpt.layer_properties(detail,"Retained hidden detail",false,true,.375f);sculpt.select_layer(1);sculpt.fill_mask(.35f);
 s.active().volume=std::make_shared<Volume>(sculpt.geometry_snapshot());
 auto p=std::make_shared<PolygonAuthority>();auto cube=PolygonMesh::cube().encode();
 // Six independent padded charts are a genuine strict untextured starting surface.
 for(size_t f=0;f<cube["faces"].size();++f)for(auto &corner:cube["faces"][f]["corners"]){
   auto uv=corner[2].get<std::array<float,2>>();corner[2]={.025f+float(f%3)/3+uv[0]*.25f,.025f+float(f/3)/2+uv[1]*.4f};corner[3]=false;
 }
 p->topology=PolygonMesh::decode(cube);
 s.active().painted=std::make_shared<PaintedMesh>(p->topology.triangulate().mesh,64);s.active().mesh_active=true;s.active().polygon=p;
 s.active().painted->begin_stroke();Brush paint_brush;paint_brush.from=paint_brush.to={0,0,.5f};paint_brush.radius=.15f;PaintSettings ink;ink.color={.35f,.15f,.8f,1};s.active().painted->stamp(paint_brush,ink);s.active().painted->end_stroke();
 s.validate();return s;
}
}
