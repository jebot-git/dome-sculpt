// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/scene.hpp"
#include "dome/material-bindings.hpp"
#include "dome/retopo-authoring.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
#include <unistd.h>
using namespace dome;
namespace {
size_t checks=0;
void check(bool yes,const char *message){if(!yes)throw std::runtime_error(message);++checks;std::cout<<"PASS "<<message<<'\n';}
template<class F>bool rejects(F f){try{f();return false;}catch(const std::exception&){return true;}}
template<class F>bool error_contains(F f,std::string_view expected){try{f();return false;}catch(const std::exception &e){return std::string_view(e.what()).find(expected)!=std::string_view::npos;}}
Mesh plane(){Mesh m;m.positions={{0,0,0},{1,0,0},{1,1,0},{0,1,0}};m.normals.assign(4,{0,0,1});m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.indices={0,1,2,0,2,3};return m;}
RetopoMesh patch() {
  RetopoMesh result;std::vector<TopologyID> ids;
  auto mesh=plane();for(size_t v=0;v<mesh.positions.size();++v)ids.push_back(result.add_vertex({mesh.positions[v],mesh.uv[v],v==0}));
  result.add_face(ids,3);result.set_seam(result.edges().begin()->first,true);return result;
}
Scene combined() {
  Scene scene;auto source=std::make_shared<PaintSourceMaps>();
  source->color.image=std::make_shared<const Image>(Image{1,1,{32,64,128,255}});
  scene.active().painted=PaintedMesh::from_maps(plane(),source,64);scene.active().mesh_active=true;
  auto original=std::make_shared<SourceUvOrigin>();original->paint=scene.active().painted->snapshot();
  auto binding=std::make_shared<SourceMaterialBindings>();binding->has_mtoon=false;binding->uv_origin=original;
  auto material=scene.duplicate(1);scene.object(material).material_bindings=binding;
  scene.set_deformation(material,DeformationState(scene.object(material).painted->mesh()));
  auto retopo=scene.duplicate(material);scene.object(retopo).retopology=std::make_shared<const RetopoPatch>(RetopoPatch{1,patch()});
  return scene;
}
void erase_binding_fields(nlohmann::json &j){j.erase("source_materials");for(auto &o:j["objects"])o.erase("material_bindings");}
void erase_retopology_fields(nlohmann::json &j){for(auto &o:j["objects"])o.erase("retopology");}
}
int main()try {
  auto scene=combined();const auto encoded=scene.encode();
  check(encoded["version"]==8 && encoded.contains("source_materials") && encoded["objects"][0].contains("retopology"),
        "Combined source materials and retopology write one unambiguous version-eight schema");
  auto decoded=Scene::decode(encoded);
  check(decoded.encode()==encoded && decoded.object(2).material_bindings==decoded.object(3).material_bindings &&
        decoded.object(3).retopology->source==1 && decoded.object(3).retopology->topology.encode()==scene.object(3).retopology->topology.encode(),
        "Combined restart retains shared immutable materials and exact topology IDs, cursor, pins, seams and source");
  check(decoded.object(2).material_bindings->uv_origin->paint->encode()==scene.object(2).material_bindings->uv_origin->paint->encode() &&
        decoded.object(2).material_bindings->uv_origin->paint->source_maps()->color.image==decoded.object(1).painted->source_maps()->color.image,
        "Combined image archive preserves first-source paint and deduplicated original image authority");
  auto snapshot=decoded.snapshot();auto copy=snapshot.duplicate(3);
  check(snapshot.object(copy).retopology==decoded.object(3).retopology && snapshot.object(copy).material_bindings==decoded.object(3).material_bindings && decoded.encode()==encoded,
        "Native snapshot and duplication retain immutable patch/material owners without mutating the original scene");
  auto material7=encoded;material7["version"]=7;erase_retopology_fields(material7);
  auto loaded_material7=Scene::decode(material7);
  check(loaded_material7.object(2).material_bindings && !loaded_material7.object(3).retopology && loaded_material7.encode()["version"]==8,
        "Historical material-only version seven loads by its complete archive and binding shape");
  auto retopo7=encoded;retopo7["version"]=7;erase_binding_fields(retopo7);
  auto loaded_retopo7=Scene::decode(retopo7);
  check(loaded_retopo7.object(3).retopology && !loaded_retopo7.object(2).material_bindings && loaded_retopo7.encode()["version"]==8,
        "Historical retopology-only version seven loads without inventing material archives");
  auto hybrid7=encoded;hybrid7["version"]=7;
  check(Scene::decode(hybrid7).encode()==encoded,"Complete legacy version-seven feature shapes can coexist without losing either authority");
  {
    auto bad=hybrid7;bad["source_materials"]=nullptr;
    check(rejects([&]{Scene::decode(bad);}),"Malformed material archive never falls through to the valid retopology variant");
    bad=hybrid7;bad["objects"][1].erase("material_bindings");
    check(rejects([&]{Scene::decode(bad);}),"A partially declared legacy material feature is rejected");
    bad=retopo7;bad["objects"][0]["material_bindings"]=nullptr;
    check(rejects([&]{Scene::decode(bad);}),"A stray null binding key cannot disguise a missing material archive");
    bad=hybrid7;bad["objects"][0].erase("retopology");
    check(rejects([&]{Scene::decode(bad);}),"A partial retopology feature cannot be discarded because material data is valid");
    bad=encoded;bad.erase("source_materials");
    check(rejects([&]{Scene::decode(bad);}),"Version eight requires its material archive even for null object bindings");
    bad=encoded;bad["objects"][0].erase("retopology");
    check(rejects([&]{Scene::decode(bad);}),"Version eight requires every explicit nullable retopology field");
    bad=encoded;bad["objects"][1]["material_bindings"]=999;
    check(rejects([&]{Scene::decode(bad);}),"Unknown material identities reject before scene publication");
    bad=encoded;bad["source_materials"]["version"]=1.0;
    check(rejects([&]{Scene::decode(bad);}),"Material archive version type is validated rather than guessed from numeric equality");
  }
  {
    auto orphan=decoded.snapshot();orphan.erase(1);auto saved=orphan.encode();auto reopened=Scene::decode(saved);
    check(reopened.object(3).retopology->source==1 && reopened.encode()==saved,
          "A deleted original object remains a stable orphan retopology reference through native restart");
    check(rejects([&]{RetopoWorkspace unavailable(reopened);}),"Reopening an unavailable retopology reference refuses cleanly");
    auto bad=encoded;bad["objects"][2]["retopology"]["source"]=3;
    check(rejects([&]{Scene::decode(bad);}),"Self-referential topology source IDs reject");
    bad=encoded;bad["objects"][2]["retopology"]["source"]=bad["next_id"];
    check(rejects([&]{Scene::decode(bad);}),"Never-allocated retopology source IDs reject while deleted allocated IDs remain valid");
    bad=encoded;bad["objects"][1]["retopology"]={{"source",3},{"topology",scene.object(3).retopology->topology.encode()}};
    bad["objects"][2]["retopology"]["source"]=2;
    check(error_contains([&]{Scene::decode(bad);},"cycle"),"Indirect reference cycles reject before persistent topology allocation");
  }
  {
    auto dormant=decoded.snapshot();dormant.object(3).mesh_active=false;auto saved=dormant.encode();
    check(saved["version"]==8 && source_is_dormant(dormant.object(3)) && Scene::decode(saved).encode()==saved,
          "Dormant topology and source data remain exact through the combined native format");
    check(rejects([&]{dormant.require_exportable_sources();}),"A visible dormant editable patch cannot silently become flattened export geometry");
    auto old=material7;old["objects"][1]["mesh_active"]=false;
    check(bool(Scene::decode(old).object(2).deformation),"Historical material version seven keeps its original dormant deformation semantics");
    old=retopo7;old["objects"][1]["mesh_active"]=false;
    check(rejects([&]{Scene::decode(old);}),"Retopology-only legacy seven does not invent the other writer's dormant deformation feature");
  }
  {
    Scene plain;plain.active().painted=std::make_shared<PaintedMesh>(plane(),64);plain.active().mesh_active=true;
    plain.set_deformation(1,DeformationState(plain.active().painted->mesh()));auto saved=plain.encode();
    check(saved["version"]==5,"Plain source-free scenes retain the existing version-five writer");
    for(int version:{4,5,6}) {
      auto old=saved;old["version"]=version;auto dormant=old;dormant["objects"][0]["mesh_active"]=false;
      check(!rejects([&]{Scene::decode(old);}) && rejects([&]{Scene::decode(dormant);}),
            "Legacy versions four through six retain active deformation and refuse dormant semantics");
    }
    auto unknown=saved;unknown["version"]=7;
    check(rejects([&]{Scene::decode(unknown);}),"Legacy version seven without any recognized feature shape is not guessed");
    unknown=saved;unknown["version"]=8.0;
    check(rejects([&]{Scene::decode(unknown);}),"Scene schema version must have an integer type");
  }
  {
    auto bad=encoded;bad["source_materials"]["paints"]=nlohmann::json::array();
    auto paint=encoded["source_materials"]["paints"][0];paint["resolution"]=2048;
    for(size_t i=0;i<5;++i)bad["source_materials"]["paints"].push_back(paint);
    check(error_contains([&]{Scene::decode(bad);},"allocations"),"Combined retained paint allocations are preflighted before any oversized SurfaceMap construction");
  }
  {
    auto metadata=nlohmann::json{{"version",1},{"next",100001},{"vertices",nlohmann::json::array()},{"edges",nlohmann::json::array()},{"faces",nlohmann::json::array()}};
    for(uint32_t i=1;i<=100'000;++i)metadata["vertices"].push_back({i,{0,0,0},{0,0},false});
    const auto payload=metadata.dump();auto bad=encoded;bad["objects"]=nlohmann::json::array({encoded["objects"][0]});bad["active"]=1;bad["next_id"]=13;
    for(uint64_t i=2;i<=12;++i){auto object=encoded["objects"][2];object["id"]=i;object["retopology"]["topology"]=payload;bad["objects"].push_back(std::move(object));}
    check(error_contains([&]{Scene::decode(bad);},"Scene retopology budget"),"Aggregate topology record quota is checked before allocating persistent patch maps");
  }
  auto path=std::filesystem::temp_directory_path()/("dome-scene8-"+std::to_string(getpid())+".dsculpt");
  decoded.save(path);check(Scene::load(path).encode()==encoded,"Actual atomic native save/open preserves the complete combined schema");std::filesystem::remove(path);
  std::cout<<checks<<" checks passed\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
