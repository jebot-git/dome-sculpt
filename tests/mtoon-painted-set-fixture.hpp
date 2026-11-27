// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/material-bindings.hpp"
#include "dome/texture-sets.hpp"

namespace mtoon_paint_fixture {
enum class Background { Default, Uniform, SharedShade, Varying, Sharp };
inline void author(dome::Scene &scene,dome::TextureSetId set,dome::Vec3 center,
                   bool unsafe_mask=false,Background background=Background::Default){
 using namespace dome;
 const auto object=scene.active().id;
 const auto change=[&](PaintedMesh &paint,Material &material){
  material.color={.21f,.33f,.24f,unsafe_mask?.5002f:.6f};
  material.alpha=Material::AlphaMode::Mask;
  material.alpha_cutoff=unsafe_mask?.501f:.45f;
  material.emissive={.03f,.015f,.07f};
  material.unlit=false;
  const auto stamp=[&](PaintSettings settings,Vec3 position,float radius){
   Brush brush;
   brush.from=brush.to=position;
   brush.normal={0,0,1};
   brush.radius=radius;
   brush.strength=1;
   settings.front_only=false;
   paint.begin_stroke();
   paint.stamp(brush,settings);
   if(!paint.end_stroke())throw std::runtime_error("MToon test brush missed its actual texture-set grid");
  };
  PaintSettings color;
  color.color={.86f,.07f,.034f,.08f};
  stamp(color,center,.32f);
  paint.add_layer("Emissive glaze");
  paint.layer_properties("Emissive glaze",true,.62f);
  PaintSettings glow;
  glow.channel=PaintChannel::Emission;
  glow.emission={.05f,.63f,.18f};
  stamp(glow,center+Vec3{.19f,-.11f,0},.22f);
  PaintSettings mask;
  mask.channel=PaintChannel::Mask;
  mask.mask=.27f;
  stamp(mask,center+Vec3{.14f,-.08f,0},.1f);
  return true;
 };
 if(!edit_texture_set(scene,object,set,change))throw std::runtime_error("Actual MToon texture-set authoring did not change");
 const auto current=texture_set_view(scene.active(),set).material;
 SourceMaterialBindings material;
 nlohmann::json source={
     {"alphaMode","MASK"},{"alphaCutoff",current.alpha_cutoff},
     {"doubleSided",current.double_sided},
     {"pbrMetallicRoughness",{{"baseColorFactor",current.color}}},
     {"emissiveFactor",current.emissive},
     {"extensions",{{"VRMC_materials_mtoon",{
       {"specVersion","1.0"},{"shadeColorFactor",{.12f,.08f,.16f}},
       {"shadeMultiplyTexture",{{"index",0}}}}}}}
 };
 const bool uniform_roles=background!=Background::Default;
 if(uniform_roles){
  source["pbrMetallicRoughness"]["baseColorTexture"]={{"index",1},
      {"extensions",{{"KHR_texture_transform",{{"offset",{.17f,.35f}}}}}}};
  source["emissiveTexture"]={{"index",2}};
  material.roles[size_t(MToonSlot::Base)].image=
      std::make_shared<const Image>(Image{2,1,{112,160,192,255,112,160,192,255}});
  material.roles[size_t(MToonSlot::Emission)].image=
      std::make_shared<const Image>(Image{1,1,{75,180,112,255}});
 }
 if(background==Background::Varying||background==Background::Sharp){
  const auto resolution=texture_set_view(scene.active(),set).painted->resolution();
  Image base{resolution,resolution,std::vector<uint8_t>(size_t(resolution)*resolution*4,255)};
  Image emission=base;
  for(uint32_t y=0;y<resolution;++y)for(uint32_t x=0;x<resolution;++x){
   const size_t at=(size_t(y)*resolution+x)*4;
   const auto u=uint8_t(x*15/resolution),v=uint8_t(y*15/resolution);
   base.rgba[at]=background==Background::Sharp?(x%2?255:0):uint8_t(112+u+v);
   base.rgba[at+1]=160+u;
   base.rgba[at+2]=192+v;
   emission.rgba[at]=75+v;
   emission.rgba[at+1]=180+u;
   emission.rgba[at+2]=112+u+v;
  }
  auto &base_role=material.roles[size_t(MToonSlot::Base)];
  auto &emission_role=material.roles[size_t(MToonSlot::Emission)];
  base_role.image=std::make_shared<const Image>(std::move(base));
  emission_role.image=std::make_shared<const Image>(std::move(emission));
  for(auto *role:{&base_role,&emission_role}){
   role->wrap_s=role->wrap_t=33071;
   role->min_filter=role->mag_filter=9729;
  }
  source["pbrMetallicRoughness"]["baseColorTexture"]={{"index",1}};
 }
 auto &shade=material.roles[size_t(MToonSlot::Shade)];
 if(background==Background::SharedShade||background==Background::Varying||background==Background::Sharp){
  auto &toon=source["extensions"]["VRMC_materials_mtoon"];
  toon["shadeMultiplyTexture"]=source["pbrMetallicRoughness"]["baseColorTexture"];
  toon["shadingShiftFactor"]=-.8f;
  shade=material.roles[size_t(MToonSlot::Base)];
 }else shade.image=std::make_shared<const Image>(Image{2,1,{94,111,161,255,81,126,95,255}});
 material.material=MToonMaterial::from_vrm1(source,uniform_roles?3:1);
 material.native_reference={current.color,current.emissive,int(current.alpha),
                            current.alpha_cutoff,current.double_sided};
 if(!bind_texture_set_mtoon(scene,object,set,material))
   throw std::runtime_error("Actual test material was not bound to its texture set");
}
} // namespace mtoon_paint_fixture
