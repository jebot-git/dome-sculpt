// SPDX-License-Identifier: GPL-3.0-or-later
// Actual512 plain writable fixtures; no application hooks or backend overrides.
#include "dome/document.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
using namespace dome;
int main(int argc,char**argv)try{if(argc!=3)return 2;std::string mode=argv[1];if(mode=="--inspect"){std::cout<<Scene::load(argv[2]).encode().dump()<<'\n';return 0;}if(mode=="--xr-fixture"){Document d;d.seed();d.prepare_mesh(512);if(!d.wait_asset()||!d.frame().state->mesh_active)throw std::runtime_error(d.frame().state->error);d.save(argv[2]);return 0;}
Mesh m;m.positions={{-.5f,-.5f,0},{.5f,-.5f,0},{.5f,.5f,0},{-.5f,.5f,0}};m.normals.assign(4,{0,0,1});m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.indices={0,1,2,0,2,3};Scene s;s.active().painted=std::make_shared<PaintedMesh>(m,512);s.active().mesh_active=true;DeformationState rig(m);rig.skeleton=Skeleton{{{"root",-1,{},1},{"limb",0,{},.5f}}};rig.skin=SkinWeights{};rig.skin->vertices.assign(4,{{1,1}});rig.pose.resize(2);rig.pose[1].translation={1,0,0};rig.morphs=MorphSet(m);auto positions=m.positions;for(auto&p:positions)p.x+=2;rig.morphs->set_target(m,"Moved limb",positions);rig.morph_weights={1};s.set_deformation(1,rig);s.active().transform.position.x=-3;s.save(argv[2]);return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
