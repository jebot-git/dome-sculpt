// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/creation.hpp"
#include "dome/document.hpp"
#include "dome/material-bindings.hpp"
#include <iostream>
using namespace dome;
namespace {int checks=0;void check(bool v,const char*s){if(!v)throw std::runtime_error(s);++checks;std::cout<<"PASS "<<s<<'\n';}}
int main()try {
 auto controls=humanoid_baseline_defaults("vroid-beta-female");controls.head_width=.2f;controls.eye_size=.2f;controls.leg_length=.1f;controls.waist_width=-.1f;
 Scene scene;append_humanoid(scene,controls,"Frame capture",{},64);
 size_t parts=0,bytes=0;bool aliases=true,proof=true;NormalFrameArchive frames;
 for(const auto &o:scene.objects())if(o.material_bindings) {
  ++parts;const auto &b=*o.material_bindings;frames.add(b.normal_frames);
  aliases&=b.normal_frames->pbr==b.normal_frames->mtoon;
  proof&=b.normal_frame==SourceMaterialNormalFrame::OriginalAttributes && b.uv_origin->creator_version==1 && b.uv_origin->creator_parameters==controls.encode().dump() && b.uv_origin->deformation==o.deformation;
  for(size_t c=0;c<o.painted->mesh().indices.size();++c)proof&=b.normal_frames->pbr->source_vertices[c]==o.appearance->source_vertices[o.painted->mesh().indices[c]];
 }
 bytes=frames.storage_bytes();check(parts==19 && proof,"Every actual shaped CC0 avatar part captures actual Shape controls, original corners and immutable native deformation authority");
 check(aliases && bytes>0 && bytes<=128*1024*1024,"Actual compatible UV0 PBR/MToon roles share complete generated frame channels within total archive budget");
 auto encoded=scene.encode();auto reopened=Scene::decode(nlohmann::json::from_cbor(nlohmann::json::to_cbor(encoded)));
 check(encoded["version"]==9 && encoded["source_materials"]["version"]==2 && reopened.encode()==encoded,"Actual nineteen-part avatar Scene9 archive survives native restart without changing original IDs or shaped frame keys");
 Document document;document.replace(reopened);check(document.wait_mesh(std::chrono::seconds(30)),"Actual shaped CC0 avatar publishes all original normal frames through the native mesh worker");
 auto surface=document.frame().surface;size_t framed=0;for(const auto &r:surface->ranges)if(r.source_frame)++framed;
 check(framed==19 && surface->source_normals.size()==surface->mesh.positions.size() && surface->source_tangents.size()==surface->mesh.positions.size(),"All actual avatar draw ranges carry complete packed retained normal/tangent data");
 std::cout<<"Actual creator frame checks "<<checks<<" payload_bytes "<<bytes<<" vertices "<<surface->mesh.positions.size()<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
