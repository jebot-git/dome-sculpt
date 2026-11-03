// SPDX-License-Identifier: GPL-3.0-or-later
#define main original_graphics_fixture
#include "mtoon-render.cpp"
#undef main
#include "dome/source-paint.hpp"
#include "dome/material-bindings.hpp"
#include "dome/scene.hpp"
int main(int argc,char **argv)try{
 if(argc!=3)return 2;
 if(std::string_view(argv[1])=="--inspect"){std::cout<<Scene::load(argv[2]).encode().dump()<<'\n';return 0;}
 if(std::string_view(argv[1])!="--fixture")return 2;
 auto raw=quad();raw.uv={{-1.25f,-.5f},{1.75f,-.5f},{1.75f,2.5f},{-1.25f,2.5f}};
 auto maps=std::make_shared<PaintSourceMaps>();maps->color.image=image({100,190,230,255});maps->emission.image=image({70,100,40,255});
 PaintSurfaceOptions policy;policy.policy=PaintUvPolicy::SourceAuthored;auto before=PaintedMesh::from_maps(raw,maps,64,policy);auto paint=prepare_source_paint(before,quad(),std::array<uint32_t,4>{0,1,2,3});
 PaintSettings setting;setting.channel=PaintChannel::Emission;setting.emission_units=PaintEmissionUnits::Physical;setting.emission={.5f,.005f,.01f};setting.front_only=false;Brush b;b.from=b.to={0,0,0};b.normal={0,0,1};b.radius=.4f;b.strength=.7f;paint->begin_stroke();paint->stamp(b,setting);if(!paint->end_stroke())return 3;
 Scene scene;auto &o=scene.active();o.mesh_active=true;o.painted=paint;o.material=white();o.material.color={.5f,.7f,.9f,1};o.material.emissive={.1f,.2f,.05f};o.name="Source paint MToon";
 auto holder=std::make_shared<SourceMaterialBindings>();holder->material=typed(false);holder->native_reference={o.material.color,o.material.emissive,int(o.material.alpha),o.material.alpha_cutoff,o.material.double_sided};holder->material.parameters.base_color=o.material.color;holder->material.parameters.shade_color={.15f,.3f,.2f};holder->material.parameters.emission={.1f,.2f,.05f};holder->material.textures[0].index=0;holder->roles[0].image=maps->color.image;holder->material.textures[3].index=1;holder->roles[3].image=maps->emission.image;
 auto origin=std::make_shared<SourceUvOrigin>();origin->paint=before->snapshot();origin->material=holder->material;origin->roles=holder->roles;holder->uv_origin=origin;o.material_bindings=holder;scene.validate();scene.save(argv[2]);return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
