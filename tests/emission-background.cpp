// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_HDR_SCENE_FIXTURE_ONLY
#include "emission-scene.cpp"
#undef DOME_HDR_SCENE_FIXTURE_ONLY
#include "emission-authority.hpp"
int main(int argc,char**argv)try{
 if(argc!=2)return 2;const std::filesystem::path dir=argv[1];std::filesystem::create_directories(dir);
 auto ordinary=hdr_scene::plain();const auto legacy=ordinary.encode();auto current=ordinary.snapshot();current.active().material.emissive={10000,.005f,12};auto value=current.encode();
 check(value.at("version")==27&&!value.at("objects")[0].at("painted").contains("version"),"Actual native radiance selects Scene27 without fabricating physical paint");
 hdr_scene::restart(current,dir/"native-background.dsculpt");
 auto bad=value;bad["version"]=26;check(refuses([&]{Scene::decode(bad);}),"Native radiance cannot masquerade as a legacy unit material");
 check(refuses([&]{current.active().painted->composite(current.active().material);}),"Unimplemented legacy image preview refuses instead of clipping background radiance");
 for(auto radiance:{std::array<float,3>{0,0,0},std::array<float,3>{.005f,10000,12}}){
  auto binding=std::make_shared<SourceMaterialBindings>();binding->has_mtoon=false;binding->physical_emission=radiance;binding->validate();auto scene=ordinary.snapshot();scene.active().material_bindings=binding;const auto saved=scene.encode();
  check(saved.at("version")==27&&saved.at("source_materials").at("version")==11&&saved.at("source_materials").at("base_version")==1,"Absolute background owns material11 over real base1, even authored zero");
  auto read=hdr_scene::restart(scene,dir/(radiance[1]?"absolute-hdr.dsculpt":"absolute-zero.dsculpt"));check(read.active().material_bindings->physical_emission==binding->physical_emission,"Absolute intent is distinct from historical native factor correction");
  auto capsule=capture_polygon_origin(scene,scene.active().id);auto retained=ordinary.snapshot();retained.active().shape_source=retained.active().rig_source=capsule;auto restart=hdr_scene::restart(retained,dir/(radiance[1]?"before-hdr.dsculpt":"before-zero.dsculpt"));check(restart.active().rig_source->material_bindings->physical_emission==binding->physical_emission,"First rig/shape before-state preserves explicit physical background including zero");
  auto downgraded=saved;downgraded["source_materials"]["version"]=1;downgraded["source_materials"].erase("base_version");check(refuses([&]{Scene::decode(downgraded);}),"Unknown physical field cannot ride inside legacy material1");
 }
 auto baseline=std::make_shared<SourceMaterialBindings>();baseline->has_mtoon=false;baseline->native_reference.emission={12,.005f,10000};auto source=ordinary.snapshot();source.active().material_bindings=baseline;check(source.encode().at("source_materials").at("version")==11,"An HDR source comparison baseline is reachable physical state without inventing an override");hdr_scene::restart(source,dir/"physical-reference.dsculpt");
 auto invalid=std::make_shared<SourceMaterialBindings>(*baseline);invalid->physical_emission={-1,0,0};check(refuses([&]{invalid->validate();}),"Physical background validates finite nonnegative radiance");invalid->physical_emission={10001,0,0};check(refuses([&]{invalid->validate();}),"Bounded physical background keeps existing maximum input radiance");
 check(ordinary.encode()==legacy,"Ordinary unit-state output does not acquire HDR fields");std::cout<<"Physical background: "<<checks<<" checks\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
