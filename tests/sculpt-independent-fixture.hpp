// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/scene.hpp"
#include "dome/source-placement.hpp"
#include <cmath>
#include <stdexcept>
namespace sculpt_independent_fixture {
using namespace dome;
inline void require(bool value,const char *why) {if(!value)throw std::runtime_error(why);}
inline Volume sphere() {
  constexpr float voxel=.0125f,radius=.6f;
  const float band=voxel*4;const int n=int(std::ceil((radius+band)/voxel));
  Volume::Voxels values;
  for(int z=-n;z<=n;++z)for(int y=-n;y<=n;++y)for(int x=-n;x<=n;++x) {
    const float distance=length(Vec3{x*voxel,y*voxel,z*voxel})-radius;
    if(distance<band) {
      require(values.size()<4'000'000,"Original sphere sample guard reached");
      values.emplace(Int3{x,y,z},std::max(-band,distance));
    }
  }
  return Volume::from_samples(voxel,std::move(values));
}
inline void author(Scene &scene) {
  require(scene.objects().size()==4,"Independent authoring requires four objects");
  constexpr Vec3 points[]{{.55f,0,0},{0,.55f,0},{0,0,.55f},{-.55f,0,0}};
  for(size_t i=0;i<4;++i) {
    auto &v=*scene.object(i+1).volume;
    require(v.voxel_size()==.0125f,"Independent authoring preserves the production voxel size");
    const auto name="Independent authored detail "+std::to_string(i+1);
    const auto layer=v.add_layer(name);
    Brush b;b.from=b.to=points[i];b.normal=normalized(points[i]);b.radius=.12f;
    b.strength=.75f;b.blend=.1f;b.tool=Tool::Clay;
    v.begin_stroke();v.stamp(b);
    require(v.end_stroke(),"Independent authored field must change geometry");
    v.layer_properties(layer,name,true,false,.65f);
    v.fill_mask(.1f+float(i)*.05f);
    auto settings=v.sculpt_settings();settings.mask_edit=true;v.sculpt_settings(settings);
    b.from=b.to={0,0,.6f};b.normal={0,0,1};b.radius=.09f;
    v.begin_stroke();v.stamp(b);
    require(v.end_stroke(),"Independent mask must retain real sparse protection");
    settings.mask_edit=false;v.sculpt_settings(settings);
  }
}
inline Scene scene() {
  Scene result;const auto base=sphere();result.active().volume=std::make_shared<Volume>(base);
  result.active().material.double_sided=true;
  for(size_t i=1;i<4;++i) {
    CanvasTransform pose;pose.position={float(i)*1.3f,0,0};
    const auto id=result.add("Measured sphere "+std::to_string(i),base,pose);
    result.object(id).material.double_sided=true;
    if(i%3==1){result.object(id).material.alpha=Material::AlphaMode::Blend;result.object(id).material.color[3]=.6f;}
  }
  result.object(4).affine_edit=std::make_shared<NativeAffineEdit>(AffineTransform(
      std::array<double,16>{-1,0,0,0,.17,1.1,0,0,.07,.09,.85,0,0,0,0,1}));
  author(result);result.select(1);return result;
}
} // namespace sculpt_independent_fixture
