// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "current-color-bake-fixture.hpp"
namespace current_color_target_fixture {
using namespace dome;
inline Scene fixture(bool seam=false,uint32_t resolution=64){
 auto scene=cut_color_fixture::scene(seam,false,resolution);const auto target=scene.active().id;
 auto material=scene.active().material;auto mesh=scene.active().painted->mesh();
 const auto source=scene.add("Hidden ordinary source",Volume{});
 scene.object(source).painted=std::make_shared<PaintedMesh>(std::move(mesh),resolution);scene.object(source).mesh_active=true;scene.object(source).material=material;
 scene.object(target).name="Current corner RGBA target";scene.object(target).visible=true;scene.object(target).locked=false;
 scene.object(source).name="Hidden ordinary source";scene.object(source).visible=false;scene.object(source).locked=true;
 scene.select(target);scene.validate();return scene;
}
}
