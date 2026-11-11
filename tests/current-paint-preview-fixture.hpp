// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/document.hpp"
#include "current-paint-preview.hpp"
#include "emission-paint-access.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
namespace current_fixture {
using namespace dome;
inline size_t checks=0;
inline void check(bool value,const char*message){if(!value)throw std::runtime_error(message);++checks;}
inline bool refuses(auto run){try{run();return false;}catch(const std::exception&e){std::cout<<"EXPECTED "<<e.what()<<'\n';return true;}}
inline Mesh quad(){Mesh m;m.positions={{-.75f,-.75f,0},{.75f,-.75f,0},{.75f,.75f,0},{-.75f,.75f,0}};m.normals.assign(4,{0,0,1});m.uv={{{.08f,.08f}},{{.92f,.08f}},{{.92f,.92f}},{{.08f,.92f}}};m.indices={0,1,2,0,2,3};return m;}
inline void stroke(PaintedMesh&p,PaintSettings s,Vec3 at,float strength=.63f,float radius=.23f){s.front_only=false;Brush b;b.from=b.to=at;b.radius=radius;b.normal={0,0,1};b.strength=strength;b.pressure=.79f;p.begin_stroke();p.stamp(b,s);check(p.end_stroke(),"Actual native fixture stamp authors values");}
inline Scene fixture(uint32_t resolution=64,bool physical=false){
 auto p=std::make_shared<PaintedMesh>(quad(),resolution);PaintSettings s;s.color={.017f,.47f,.81f,.23f};s.metallic=.83f;s.roughness=.17f;s.emission={.91f,.031f,.27f};
 for(int layer=0;layer<3;++layer){if(layer)p->add_layer("Layer "+std::to_string(layer));for(int c=0;c<5;++c){s.channel=PaintChannel(c);s.mask=.41f;stroke(*p,s,{-.31f+layer*.27f,-.36f+c*.17f,0});}p->layer_properties("Layer "+std::to_string(layer),layer!=2,layer==0?.73f:.37f);}
 p->select_layer(p->layers().front().id);
 if(physical){s.channel=PaintChannel::Emission;s.emission_units=PaintEmissionUnits::Physical;s.emission={10000,.0003f,7};stroke(*p,s,{.03f,0,0},.00002f);}
 Scene scene;auto&o=scene.active();o.painted=p;o.mesh_active=true;o.material.color={.23f,.61f,.037f,.67f};o.material.emissive={.013f,.037f,.071f};o.material.metallic=.31f;o.material.roughness=.68f;return scene;
}
inline std::shared_ptr<const MeshSnapshot> published(Document&d,const Scene&s){d.replace(s);check(d.wait_mesh(),"Actual document worker publishes frame");check(d.frame().state->error.empty(),"Actual document worker reports no error");auto frame=d.frame().surface;check(frame&&!frame->ranges.empty(),"Actual document frame owns material ranges");return frame;}
inline auto owner(const MeshSnapshot&s){auto p=s.ranges.front().textures->current_grid_preview;check(bool(p),"Actual publisher selected render-only current-grid authority");return p;}
inline std::shared_ptr<const TextureMaps> reference(const Scene&s){const auto&o=s.active();return o.painted->has_physical_emission()||std::any_of(o.material.emissive.begin(),o.material.emissive.end(),[](float x){return x>1;})?detail::EmissionExportAccess::preview(*o.painted,o.material,{}):o.painted->composite(o.material);}
inline std::shared_ptr<MeshSnapshot> legacy_frame(const MeshSnapshot&actual,const Scene&s){auto copy=std::make_shared<MeshSnapshot>(actual);copy->ranges.front().textures=reference(s);return copy;}
}
