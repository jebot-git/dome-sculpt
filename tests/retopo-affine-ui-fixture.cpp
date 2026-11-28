// SPDX-License-Identifier: GPL-3.0-or-later
// A genuine authored fixture, not a rewritten encoded scene. The source remains
// in z=0 so the unchanged real-loader controller plane is an independent oracle.
#include "dome/scene.hpp"
#include "dome/source-placement.hpp"
#include "dome/normal-preparation.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
#include <string_view>
using namespace dome;
int main(int argc,char **argv)try {
  const bool prepared=argc==3 && std::string_view(argv[2])=="prepared";
  const bool float_mtoon=argc==3 && std::string_view(argv[2])=="float-mtoon";
  const bool baked_mtoon=float_mtoon || (argc==3 && std::string_view(argv[2])=="baked-mtoon");
  const bool painted_mtoon=baked_mtoon || (argc==3 && std::string_view(argv[2])=="painted-mtoon");
  const bool mtoon=painted_mtoon || (argc==3 && std::string_view(argv[2])=="mtoon");
  if(argc!=2 && !prepared && !mtoon)return 2;
  Mesh m;m.positions={{-.6f,-.6f,0},{.6f,-.6f,0},{.6f,.6f,0},{-.6f,.6f,0}};
  m.normals.assign(4,{0,0,1});m.uv={{0,0},{1,0},{1,1},{0,1}};m.indices={0,1,2,0,2,3};
  Scene s;auto &o=s.active();o.name="Affine reference — původní 日本語";o.mesh_active=true;
  o.painted=std::make_shared<PaintedMesh>(m,64);
  if(mtoon){
    const auto info=[](int index){
      return nlohmann::json{{"index",index},{"extensions",{{"KHR_texture_transform",
          {{"rotation",.2},{"offset",{-.15,.18}},{"scale",{1.14,.83}}}}}}};
    };
    auto binding=std::make_shared<SourceMaterialBindings>();
    nlohmann::json authored={
        {"pbrMetallicRoughness",{{"baseColorTexture",info(0)},{"baseColorFactor",o.material.color}}},
        {"normalTexture",info(2)},{"emissiveTexture",info(3)},{"emissiveFactor",{.3,.12,.08}},
        {"extensions",{{"VRMC_materials_mtoon",{{"specVersion","1.0"},
            {"shadeColorFactor",{.55,.61,.64}},{"shadeMultiplyTexture",info(painted_mtoon?0:1)}}}}}};
    if(float_mtoon)authored["extensions"]["KHR_materials_emissive_strength"]={{"emissiveStrength",1.7}};
    binding->material=MToonMaterial::from_vrm1(authored,4);
    binding->native_reference={o.material.color,o.material.emissive,int(o.material.alpha),
                               o.material.alpha_cutoff,o.material.double_sided};
    for(size_t role=0;role<4;++role){
      Image image{16,16,std::vector<uint8_t>(16*16*4,255)};
      for(size_t i=0;i<16*16;++i){
        image.rgba[4*i]=uint8_t(120+3*(i%16));
        image.rgba[4*i+1]=uint8_t(100+3*(i/16));
        image.rgba[4*i+2]=role==size_t(MToonSlot::Normal)?237:145;
      }
      binding->roles[role].image=std::make_shared<const Image>(std::move(image));
      binding->roles[role].wrap_s=33648;
      binding->roles[role].mag_filter=role==size_t(MToonSlot::Shade)?9728:9729;
    }
    if(painted_mtoon){
      binding->roles[size_t(MToonSlot::Shade)]=binding->roles[size_t(MToonSlot::Base)];
      Brush b;b.from=b.to={0,0,0};b.radius=.3f;b.normal={0,0,1};b.strength=.58f;
      PaintSettings p;p.color={.02f,.08f,.44f,1};
      o.painted->begin_stroke();o.painted->stamp(b,p);
      if(!o.painted->end_stroke())throw std::runtime_error("MToon base stroke made no change");
      p.channel=PaintChannel::Emission;p.emission={.035f,.04f,.03f};
      o.painted->begin_stroke();o.painted->stamp(b,p);
      if(!o.painted->end_stroke())throw std::runtime_error("MToon emission stroke made no change");
      if(float_mtoon){
        p.emission_units=PaintEmissionUnits::Physical;p.emission={4.3f,.45f,.19f};
        o.painted->begin_stroke();o.painted->stamp(b,p);
        if(!o.painted->end_stroke())throw std::runtime_error("MToon physical stroke made no change");
      }
      o.painted->layer_properties("Visible color and emission",true,.63f);
      o.painted->add_layer("Hidden ink");
      p.channel=PaintChannel::Color;p.color={.88f,.08f,.11f,1};
      o.painted->begin_stroke();o.painted->stamp(b,p);
      if(!o.painted->end_stroke())throw std::runtime_error("Hidden MToon stroke made no change");
      o.painted->layer_properties("Hidden ink",false,.74f);
    }
    if(baked_mtoon){
      const auto filled=[](std::array<uint8_t,4> pixel){
        Image image{64,64,std::vector<uint8_t>(64*64*4)};
        for(size_t at=0;at<image.rgba.size();at+=4)
          std::copy(pixel.begin(),pixel.end(),image.rgba.begin()+at);
        return image;
      };
      BakedTextures bake;
      bake.maps.color=filled({56,75,79,255});
      bake.maps.metal_rough=filled({170,146,87,255});
      bake.maps.emission=filled({47,23,13,255});
      bake.maps.normal=filled({140,119,254,255});
      bake.coverage.resize(64*64);
      for(size_t i=0;i<bake.coverage.size();++i){
        const auto x=int(i%64),y=int(i/64);
        bake.coverage[i]=uint8_t(std::clamp((x-19)*16,0,255));
        bake.maps.normal.rgba[i*4]=uint8_t(132+y/4);
      }
      if(float_mtoon){
        bake.emission_field=BakedEmissionField::generate_compact(64,[&](size_t i){
          const float coverage=bake.coverage[i]/255.f;
          return BakedEmissionSample{{.06f*coverage,.04f*coverage,.02f*coverage},
              {(1.1f+(i/64)/128.f)*coverage,.12f*coverage,.03f*coverage},coverage};
        });
        bake.maps.emission={};
      }
      o.painted->install_bake(std::move(bake));
    }
    o.material_bindings=std::move(binding);
  }else{
    o.painted->begin_stroke();Brush b;b.from=b.to={0,0,0};b.radius=.22f;b.normal={0,0,1};
    PaintSettings p;p.color={.18f,.65f,.32f,.9f};o.painted->stamp(b,p);
    if(!o.painted->end_stroke())throw std::runtime_error("Real source paint did not change");
    o.painted->begin_stroke();p.channel=PaintChannel::Emission;
    p.emission_units=PaintEmissionUnits::Physical;p.emission={3,.4f,.2f};
    o.painted->stamp(b,p);
    if(!o.painted->end_stroke())throw std::runtime_error("Real source HDR paint did not change");
    o.painted->add_layer("保護");
    o.painted->begin_stroke();p.channel=PaintChannel::Mask;p.mask=0;o.painted->stamp(b,p);
    if(!o.painted->end_stroke())throw std::runtime_error("Real source mask did not change");
    o.painted->layer_properties("保護",false,0);
    BakedTextures bake;
    bake.coverage.assign(64*64,255);
    bake.maps.color={64,64,std::vector<uint8_t>(64*64*4,96)};
    bake.maps.metal_rough=bake.maps.color;
    bake.maps.normal=bake.maps.color;
    for(size_t i=0;i<bake.coverage.size();++i){
      const auto pixel=4*i;
      bake.maps.color.rgba[pixel+3]=255;
      bake.maps.metal_rough.rgba[pixel+3]=255;
      bake.maps.normal.rgba[pixel]=prepared?uint8_t(161+i%64/3):128;
      bake.maps.normal.rgba[pixel+1]=128;
      bake.maps.normal.rgba[pixel+2]=bake.maps.normal.rgba[pixel+3]=255;
    }
    bake.emission_field=BakedEmissionField::generate_compact(64,[](size_t){
      return BakedEmissionSample{{},{2.5f,.25f,.5f},1.f};
    });
    o.painted->install_bake(std::move(bake));
  }
  DeformationState d(m);d.skeleton=Skeleton{{{"root",-1,{},.2f}}};
  d.skin=SkinWeights{};d.skin->vertices.assign(4,{{0,1}});
  d.morphs=MorphSet(m);auto target=m.positions;for(auto &v:target){v.x+=.04f*(v.y+.6f);v.y+=.025f*(v.x+.6f);}
  d.morphs->set_target(m,"in-plane",target);
  d.expressions=AvatarExpressions{{{"aa",{{"in-plane",.6f}}}}};
  if (prepared) {
    s.set_deformation(o.id,d);
    auto conversion=convert_canonical_normals({o.painted,{}, {},o.deformation,o.material,{}});
    o.painted=conversion.paint->snapshot();
    o.material_bindings=std::move(conversion.material_bindings);
  }
  d.morph_weights={.35f};
  d.pose.resize(1);d.pose[0].translation={.01f,-.02f,0};d.pose[0].scale={1.1f,.85f,1.2f};
  s.set_deformation(o.id,std::move(d));
  o.affine_edit=std::make_shared<const NativeAffineEdit>(AffineTransform(
    std::array<double,16>{-1.4,.1,0,0,.35,.8,0,0,0,0,1.2,0,0,0,0,1}));
  // Retain an actual sculpt underneath the editable mesh as the old XR control
  // does, so source preservation covers that independent channel too.
  o.volume->begin_stroke();Brush sculpt;sculpt.radius=.18f;sculpt.blend=0;
  o.volume->stamp(sculpt);o.volume->end_stroke();
  s.validate();s.save(argv[1]);return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
