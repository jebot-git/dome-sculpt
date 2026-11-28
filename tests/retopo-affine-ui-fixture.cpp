// SPDX-License-Identifier: GPL-3.0-or-later
// A genuine authored fixture, not a rewritten encoded scene. The source remains
// in z=0 so the unchanged real-loader controller plane is an independent oracle.
#include "dome/scene.hpp"
#include "dome/source-placement.hpp"
#include <iostream>
using namespace dome;
int main(int argc,char **argv)try {
  if(argc!=2)return 2;
  Mesh m;m.positions={{-.6f,-.6f,0},{.6f,-.6f,0},{.6f,.6f,0},{-.6f,.6f,0}};
  m.normals.assign(4,{0,0,1});m.uv={{0,0},{1,0},{1,1},{0,1}};m.indices={0,1,2,0,2,3};
  Scene s;auto &o=s.active();o.name="Affine reference — původní 日本語";o.mesh_active=true;
  o.painted=std::make_shared<PaintedMesh>(m,64);
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
    bake.maps.normal.rgba[pixel]=bake.maps.normal.rgba[pixel+1]=128;
    bake.maps.normal.rgba[pixel+2]=bake.maps.normal.rgba[pixel+3]=255;
  }
  bake.emission_field=BakedEmissionField::generate_compact(64,[](size_t){
    return BakedEmissionSample{{},{2.5f,.25f,.5f},1.f};
  });
  o.painted->install_bake(std::move(bake));
  DeformationState d(m);d.skeleton=Skeleton{{{"root",-1,{},.2f}}};
  d.skin=SkinWeights{};d.skin->vertices.assign(4,{{0,1}});
  d.morphs=MorphSet(m);auto target=m.positions;for(auto &v:target){v.x+=.04f*(v.y+.6f);v.y+=.025f*(v.x+.6f);}
  d.morphs->set_target(m,"in-plane",target);d.morph_weights={.35f};
  d.expressions=AvatarExpressions{{{"aa",{{"in-plane",.6f}}}}};
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
