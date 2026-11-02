// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "cut-color-fixture.hpp"
namespace current_color_bake_fixture {
using namespace dome;
inline Scene fixture(bool seam=false,bool posed=false){
 auto scene=cut_color_fixture::scene(seam);if(posed)cut_color_fixture::pose(scene);
 const auto source=scene.active().id;auto mesh=scene.active().deformation?scene.active().deformation->preview(scene.active().painted->mesh()):scene.active().painted->mesh();
 auto material=scene.active().material;scene.object(source).name="Read-only current RGBA source";scene.object(source).locked=true;scene.object(source).visible=false;
 auto target=scene.add("Ordinary target",Volume{});scene.object(target).painted=std::make_shared<PaintedMesh>(std::move(mesh),256);scene.object(target).mesh_active=true;scene.object(target).material=material;scene.select(target);scene.validate();return scene;
}
inline nlohmann::json source_only(Scene scene,uint64_t source){std::vector<uint64_t> erase;for(const auto &o:scene.objects())if(o.id!=source)erase.push_back(o.id);for(auto id:erase)scene.erase(id);scene.select(source);return scene.encode();}
}
