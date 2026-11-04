// SPDX-License-Identifier: GPL-3.0-or-later
#define main mirror_native_workflow_main
#include "xr-mirror-controls.cpp"
#undef main
#include "dome/normal-sampling.hpp"
#include "dome/paint-sampling.hpp"
namespace {
Scene posed_fixture(){auto s=fixture();auto &o=s.active();o.transform.position={0,1.3f,-1.1f};auto d=*o.deformation;d.skeleton=Skeleton{{{"Root",-1,{},.3f},{"Tip",0,{{0,.2f,0}},.2f}}};SkinWeights skin;for(int v=0;v<6;++v)skin.vertices.push_back({{uint32_t(v<3?0:1),1}});d.skin=skin;for(const auto &bone:d.skeleton->bones)d.pose.push_back(bone.rest);d.pose[1].translation.x=.025f;d.morph_weights={.35f,.2f};s.set_deformation(o.id,d);s.validate();return s;}
nlohmann::json inspect_scene(const Scene &s){const auto &o=s.active();PaintImageArchive images;NormalSamplingArchive normals;PaintSamplingArchive writable;auto paint=o.painted->encode(&images,&normals,&writable);paint["normal_domains"]=normals.encode(images);paint["writable_domains"]=writable.encode();paint["images"]=images.encode();auto deformation=*o.deformation;auto targets=deformation.morphs->targets;deformation.morphs.reset();deformation.morph_weights.clear();nlohmann::json keys=nlohmann::json::array();for(const auto &target:targets){nlohmann::json delta=nlohmann::json::array();for(auto v:target.delta)delta.push_back({v.x,v.y,v.z});keys.push_back({{"name",target.name},{"delta",delta}});}auto original=s.snapshot();ShapeKeyEdit restore;restore.action=ShapeKeyAction::Restore;author_shapekey(original,o.id,restore);auto encoded=s.encode();return {{"scene",encoded},{"channels",{{"paint",paint},{"deformation",deformation.encode(o.painted->mesh())},{"material",encoded["objects"][0]["material"]},{"transform",encoded["objects"][0]["transform"]}}},{"keys",keys},{"restored_source",original.encode()}};}
}
int main(int argc,char **argv)try{if(argc!=3)return 2;const std::string_view mode=argv[1];if(mode=="--ui-fixture"){posed_fixture().save(argv[2]);return 0;}if(mode=="--inspect"){auto s=Scene::load(argv[2]);auto result=inspect_scene(s);std::cout<<"DOME_MIRROR_JSON "<<result.dump()<<'\n';return 0;}return 2;}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
