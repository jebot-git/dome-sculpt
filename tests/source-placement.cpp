// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/source-placement.hpp"
#include "dome/import-image.hpp"
#include "dome/polygon-source.hpp"
#include "polygon-conversion.hpp"
#include "import-authoring-fixtures.hpp"
#include <iostream>
#include <cstdlib>
#include <new>
using namespace dome;
namespace {
thread_local bool armed=false,hit=false;
thread_local size_t calls=0,fail_at=SIZE_MAX;
size_t checks=0;
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);++checks;}
template<class F>void refuses(F f,const char*m){bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed,m);}
void near(const AffineTransform&a,const AffineTransform&b,const char*m){for(size_t i=0;i<16;++i)if(std::abs(a.matrix()[i]-b.matrix()[i])>1e-10*std::max(1.,std::abs(a.matrix()[i])))throw std::runtime_error(m);++checks;}
}
void *operator new(size_t n){if(armed&&++calls>=fail_at){hit=true;throw std::bad_alloc();}if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void *operator new[](size_t n){return ::operator new(n);}
void operator delete(void*p)noexcept{std::free(p);}
void operator delete[](void*p)noexcept{std::free(p);}
void operator delete(void*p,size_t)noexcept{std::free(p);}
void operator delete[](void*p,size_t)noexcept{std::free(p);}
int main(int argc,char**)try {
  import_fixture::Fixture fixture;fixture.j["nodes"][0]["scale"]={2,1,-3};
  fixture.j["nodes"][1]["rotation"]={0,0,std::sin(.37f),std::cos(.37f)};
  auto model=std::make_shared<CreatorSourceModel>();model->original_bytes=std::make_shared<const std::vector<uint8_t>>(fixture.bytes());
  model->asset=std::make_shared<const ImportedAsset>(import_glb(*model->original_bytes));
  auto images=std::make_shared<std::vector<std::shared_ptr<const Image>>>();for(const auto &i:model->asset->images)images->push_back(std::make_shared<const Image>(decode_import_image(i)));model->decoded_images=images;
  Scene scene;const auto root=scene.add_group("Imported source"),first=scene.add_group("Reflected parent",{},root),second=scene.add_group("Rotated child",{},first);
  auto node0=std::make_shared<SourceNodePlacement>();node0->model=model;node0->source_node=0;node0->source_instance=UINT64_MAX;
  auto node1=std::make_shared<SourceNodePlacement>(*node0);node1->source_node=1;
  scene.group(first).source_placement=node0;scene.group(second).source_placement=node1;
  auto raw=AffineTransform{}.matrix();raw[4]=.2;auto edit=std::make_shared<const NativeAffineEdit>(AffineTransform(raw));
  scene.group(first).affine_edit=edit;scene.active().affine_edit=edit;scene.active().group=second;
  const auto &mesh=model->asset->primitives[0].mesh;scene.active().painted=std::make_shared<PaintedMesh>(mesh,64);scene.active().mesh_active=true;
  const auto expected=edit->matrix()*AffineTransform(model->asset->nodes[0].local)*AffineTransform(model->asset->nodes[1].local)*edit->matrix();
  near(scene.world_matrix(scene.active().id),expected,"source locals and native prefixes use declared order");
  refuses([&]{(void)scene.world_transform(scene.active().id);},"legacy Canvas query cannot approximate affine placement");
  const auto encoded=scene.encode();check(encoded.at("version")==13 && encoded.contains("affine_state")&&encoded.contains("polygon_sources"),"Scene13 includes full empty polygon and placement shapes");
  check(encoded.at("source_models").size()==1,"source nodes retain one model without primitive appearances");
  auto restored=Scene::decode(nlohmann::json::from_cbor(nlohmann::json::to_cbor(encoded)));
  near(restored.world_matrix(restored.active().id),expected,"affine placement survives exact CBOR restart");
  check(restored.encode()==encoded,"Scene13 writer/read retains complete structural state");
  check(restored.active().affine_edit==restored.group(first).affine_edit,"native edit aliases survive restart");
  check(restored.group(first).source_placement->model==restored.group(second).source_placement->model,"model aliases survive restart");
  check(restored.group(first).source_placement->source_instance==UINT64_MAX,"source instance is unrestricted provenance, not live group ID");
  auto surfaces=restored.surfaces();near(surfaces[0].world_matrix(),expected,"rest surface producer retains placement owner");
  auto bad=encoded;bad["version"]=12;refuses([&]{Scene::decode(bad);},"older Scene refuses new placement fields");
  bad=encoded;bad.erase("affine_state");refuses([&]{Scene::decode(bad);},"missing placement table refuses");
  bad=encoded;bad["affine_state"]["nodes"][0]["model"]=1234;refuses([&]{Scene::decode(bad);},"invalid model ref refuses before adoption");
  bad=encoded;bad["affine_state"]["nodes"][0]["node"]=99999;refuses([&]{Scene::decode(bad);},"invalid source node refuses");
  bad=encoded;bad["affine_state"]["edits"].push_back(AffineTransform{}.matrix());refuses([&]{Scene::decode(bad);},"orphan native edit refuses");
  bad=encoded;bad["affine_state"]["nodes"].push_back(bad["affine_state"]["nodes"][0]);refuses([&]{Scene::decode(bad);},"orphan source node refuses");
  bad=encoded;bad["affine_state"]["edits"][0][0]=0;bad["affine_state"]["edits"][0][1]=0;bad["affine_state"]["edits"][0][2]=0;
  refuses([&]{Scene::decode(bad);},"singular native edit refuses");
  bad=encoded;bad["affine_state"]["edits"][0][3]=.1;
  refuses([&]{Scene::decode(bad);},"projective native edit refuses");
  bad=encoded;bad["affine_state"]["nodes"][0]["instance"]=-1;
  refuses([&]{Scene::decode(bad);},"negative provenance instance refuses");
  bad=encoded;bad["affine_state"]["edits"]=nlohmann::json::array();for(size_t i=0;i<4097;++i)bad["affine_state"]["edits"].push_back(AffineTransform{}.matrix());
  refuses([&]{Scene::decode(bad);},"native edit table quota refuses before decode");
  auto capsule=capture_source_placement(scene,scene.active().id);near(capsule->world_matrix(),expected,"captured group chain evaluates without live lookup");
  auto origin=capture_polygon_origin(scene,scene.active().id);check(origin->placement!=nullptr,"shared origin capture preserves full placement");
  Scene retained;auto authority=std::make_shared<PolygonAuthority>();authority->topology=prepare_polygon_topology(mesh,nullptr).topology;authority->active=false;authority->origin=origin;retained.active().polygon=authority;
  const auto only=retained.encode();check(only.at("version")==13 && only.at("polygon_sources").at("version")==2,"retained-only affine placement propagates Scene13");
  bad=only;bad["affine_state"]["capsules"][0]["groups"][0]["parent"]=999;
  refuses([&]{Scene::decode(bad);},"captured path cannot depend on a missing live group");
  bad=only;bad["affine_state"]["capsules"].push_back(bad["affine_state"]["capsules"][0]);
  refuses([&]{Scene::decode(bad);},"orphan captured placement refuses");
  auto again=Scene::decode(only);near(again.active().polygon->origin->placement->world_matrix(),expected,"retained capsule survives source group deletion/restart");
  check(again.encode()==only,"retained-only source state roundtrips exactly");
  bad=only;bad["version"]=12;refuses([&]{Scene::decode(bad);},"old Scene rejects retained-only placement");
  if(argc>1) {
    // A retained-only capsule exercises nested source/model registration as well
    // as current group/object references. Persistent denial stays armed during
    // cleanup: source identity and retries must survive every observed boundary.
    for(const auto *input:{&scene,&retained}) {
      const auto baseline=input->encode();
      calls=0;fail_at=SIZE_MAX;armed=true;{auto measured=input->encode();}armed=false;
      const auto encode_calls=calls;
      for(size_t at=1;at<=encode_calls;++at) {
        calls=0;fail_at=at;hit=false;armed=true;bool caught=false;
        try{auto denied=input->encode();}catch(const std::bad_alloc&){caught=true;}
        armed=false;check(hit&&caught,"every Scene13 encode allocation unwinds under persistent denial");
        check(input->encode()==baseline,"failed Scene13 encode and exact retry preserve source");
      }
      calls=0;fail_at=SIZE_MAX;armed=true;{auto measured=Scene::decode(baseline);}armed=false;
      const auto decode_calls=calls;
      for(size_t at=1;at<=decode_calls;++at) {
        calls=0;fail_at=at;hit=false;armed=true;bool caught=false;
        try{auto denied=Scene::decode(baseline);}catch(const std::bad_alloc&){caught=true;}
        armed=false;check(hit&&caught,"every Scene13 decode allocation unwinds under persistent denial");
        check(Scene::decode(baseline).encode()==baseline,"failed Scene13 restart retries exactly");
      }
      std::cout<<encode_calls<<" encode/"<<decode_calls<<" decode permanent allocation boundaries passed\n";
    }
  }
  auto detached=scene;detached.erase_group(root,false);near(detached.world_matrix(detached.active().id),expected,"deleting affine groups preserves object world geometry");
  check(detached.active().group==0 && detached.active().affine_edit,"detached source is represented by native local affine prefix");
  Scene legacy;check(legacy.encode().at("version")==5 && !legacy.encode().contains("affine_state"),"ordinary source-free scene retains old writer shape");
  std::cout<<checks<<" Scene13 placement checks passed\n";return 0;
}catch(const std::exception&e){armed=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
