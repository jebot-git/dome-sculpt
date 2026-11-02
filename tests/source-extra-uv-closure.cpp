// SPDX-License-Identifier: GPL-3.0-or-later
#include "import-authoring-fixtures.hpp"
#include "dome/creator-archive.hpp"
#include "dome/source-placement.hpp"
#include "dome/polygon-scene.hpp"
#include "polygon-conversion.hpp"
#include "scene-feature-version.hpp"
#include <iostream>
#include <limits>
using namespace dome;
namespace {unsigned checks=0;void check(bool v,const char *s){if(!v)throw std::runtime_error(s);++checks;std::cout<<"PASS "<<s<<'\n';}bool refuses(auto f){try{f();return false;}catch(const std::exception &e){std::cout<<"REFUSAL "<<e.what()<<'\n';return true;}}}
int main(int argc,char **argv)try {
  if(argc!=2)return 2;const std::filesystem::path dir=argv[1];std::filesystem::create_directories(dir);
  import_fixture::Fixture fixture;while(fixture.bin.size()%4)fixture.bin.push_back(0);
  const auto uv=fixture.values({0,0,0,1,1,1,1,0},2,"VEC2");
  fixture.j["meshes"][0]["primitives"][0]["attributes"]["TEXCOORD_2"]=uv;
  fixture.j["meshes"][0]["primitives"][0]["attributes"]["TEXCOORD_7"]=uv;
  const auto path=dir/"unused-extra-uv.glb";fixture.save(path);Scene before;ImportAuthoringOptions options;options.paint_resolution=64;
  auto imported=prepare_authoring_import(before,path,options).scene;
  uint64_t selected=0;for(const auto &o:imported.objects())if(o.appearance){selected=o.id;break;}
  check(selected!=0,"Real native import retains the unprepared UV0-textured source");
  const auto &part=imported.object(selected);const auto original=part.appearance;
  check(part.painted->source_maps() && part.painted->source_maps()->extra_uv_sets.empty() && !part.painted->source_maps()->uses_alternate_uv(),"Unused raw additional arrays do not invent active sampling roles");
  check(original->asset->primitives.front().extra_uv_sets.size()==2,"Native raw model owns every unsampled UV2 and UV7 array");
  const auto encoded=imported.encode();check(encoded.at("version")==26 && Scene::decode(encoded).encode()==encoded,"Unused raw additional coordinates alone require durable Scene26");
  auto downgraded=encoded;downgraded["version"]=13;check(refuses([&]{Scene::decode(downgraded);}),"An old outer version cannot smuggle unused raw source arrays");
  auto corrupt=[&](auto edit,bool shallow){auto s=imported.snapshot();auto a=std::make_shared<CreatorAppearance>(*original);auto asset=std::make_shared<ImportedAsset>(*a->asset);edit(*asset);a->asset=asset;s.object(selected).appearance=a;return refuses([&]{if(shallow)s.validate(false);else s.encode();});};
  check(corrupt([](auto &a){a.primitives.front().extra_uv_sets[2].pop_back();},true),"Unsampled raw arrays with a shortened vertex domain refuse shallow admission");
  check(corrupt([](auto &a){a.primitives.front().extra_uv_sets[2][0][0]=std::numeric_limits<float>::quiet_NaN();},true),"Unsampled nonfinite raw coordinates refuse shallow admission");
  check(corrupt([](auto &a){a.primitives.front().extra_uv_sets[8]=a.primitives.front().extra_uv_sets.at(2);},true),"Unknown raw additional coordinate IDs refuse before archiving");
  check(corrupt([](auto &a){a.primitives.front().extra_uv_sets[2][0][0]=.125f;},false),"Same-size typed raw coordinate substitution fails canonical accessor replay");
  check(corrupt([](auto &a){a.primitives.front().extra_uv_sets.erase(2);},false),"Dropping an unused typed array fails canonical raw-source closure");
  check(corrupt([](auto &a){a.primitives.front().extra_uv_sets[2][0][0]=-0.f;},false),"Canonical extra-coordinate replay preserves signed-zero bits");
  Scene placement;const auto group=placement.add_group("Source placement");
  auto node=std::make_shared<SourceNodePlacement>();node->model=std::make_shared<const CreatorSourceModel>(*original);node->source_node=0;node->source_instance=1;placement.group(group).source_placement=node;
  const auto placed=placement.encode();check(placed.at("version")==26 && Scene::decode(placed).encode()==placed,"Model-only original placement retains unused raw UV arrays and Scene26");
  downgraded=placed;downgraded["version"]=13;check(refuses([&]{Scene::decode(downgraded);}),"Model-only placement cannot downgrade its source feature version");
  placement.active().group=group;const auto capsule=capture_polygon_origin(placement,placement.active().id);
  Scene retained;retained.active().shape_source=capsule;const auto saved_capsule=retained.encode();
  check(saved_capsule.at("version")==26 && Scene::decode(saved_capsule).encode()==saved_capsule,"Detached source capsule alone retains model-only additional UV feature closure");
  downgraded=saved_capsule;downgraded["version"]=16;check(refuses([&]{Scene::decode(downgraded);}),"Retained placement closure cannot borrow a lower shape-source version");
  PaintImageArchive images;CreatorArchiveLimits limits;limits.cancelled=[] {return true;};CreatorAssetArchive archive(limits);
  check(refuses([&]{archive.add_model(*node->model,images);}) && archive.size()==0 && images.size()==0,"Cancelled model admission publishes neither source nor image archive entries");
  const auto &raw=original->asset->primitives.front().extra_uv_sets.at(2);auto values=std::make_shared<const std::vector<std::array<float,2>>>(raw);auto maps=std::make_shared<PaintSourceMaps>(*part.painted->source_maps());maps->color.texcoord=2;maps->extra_uv_sets[2]=values;
  auto paired=imported.snapshot();paired.object(selected).painted=PaintedMesh::from_maps(part.painted->mesh(),maps,64);paired.validate();check(paired.encode().at("version")==26,"Actual original vertex remapping authenticates a supplied raw UV2 sampling owner");
  auto wrong=std::make_shared<std::vector<std::array<float,2>>>(raw);wrong->front()[0]=.25f;maps=std::make_shared<PaintSourceMaps>(*maps);maps->extra_uv_sets[2]=wrong;paired.object(selected).painted=PaintedMesh::from_maps(part.painted->mesh(),maps,64);
  check(refuses([&]{paired.validate(false);}),"Provided-tangent sources also reject same-sized unrelated active UV owners");
  check(imported.encode()==encoded,"Failed closure and pairing probes preserve the complete original scene exactly");
  std::cout<<"PASS "<<checks<<" unused additional source UV closure checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
