// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/material-bindings.hpp"
#include "dome/scene.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
using namespace dome;
namespace {
int checks=0;
void check(bool value,const char *message){if(!value)throw std::runtime_error(message);++checks;std::cout<<"PASS "<<message<<'\n';}
bool rejects(auto fn){try{fn();return false;}catch(const std::exception&){return true;}}
Mesh triangle(){Mesh m;m.positions={{0,0,0},{1,0,0},{0,1,0}};m.normals.assign(3,{0,0,1});m.indices={0,1,2};m.uv={{{0,0}},{{1,0}},{{0,1}}};return m;}
std::shared_ptr<NormalFrames> frame(){auto f=std::make_shared<NormalFrames>();f->origin_vertex_count=3;f->origin_vertices={0,1,2};f->normals.assign(3,{0,0,1});f->targets.push_back({1,std::vector<Vec3>(3,Vec3{.1f,0,-.01f}),{}});return f;}
}
int main()try {
 auto f=frame();f->validate();auto restored=NormalFrames::decode(nlohmann::json::from_cbor(nlohmann::json::to_cbor(f->encode())));
 check(restored.encode()==f->encode() && restored.tangents.empty(),"N-only frame preserves exact normal morph authority without invented UVs or tangents");
 auto payload=f->encode();const auto &bytes=payload.at("normals").get_binary();
 check(bytes[8]==0 && bytes[9]==0 && bytes[10]==128 && bytes[11]==63,"Native frame float buffers use a defined little-endian IEEE754 encoding");
 auto set=std::make_shared<NormalFrameSet>();set->pbr=set->mtoon=f;set->targets.push_back({1,0,std::nullopt,{}});set->current_targets={1};set->next_target=2;set->validate();
 NormalFrameArchive archive;archive.add(set);archive.add(set);auto second=std::make_shared<NormalFrameSet>(*set);archive.add(second);auto encoded=archive.encode();auto read=NormalFrameArchive::decode(encoded);
 check(encoded["sets"].size()==2 && encoded["frames"].size()==1 && archive.storage_bytes()==f->storage_bytes(),"Native archive deduplicates complete immutable frame payloads across material sets");
 check(read.at(0)->pbr==read.at(1)->mtoon && read.encode()==encoded,"Frame restart preserves shared role identity and exact binary channels");
 auto malformed=encoded;malformed["frames"][0]["origin_count"]=-1;check(rejects([&]{NormalFrameArchive::decode(malformed);}),"Negative frame domain cannot wrap into an unsigned identity");
 malformed=encoded;malformed["sets"][0]["current_targets"]={2};check(rejects([&]{NormalFrameArchive::decode(malformed);}),"Current native slots cannot refer to an unknown or replaced target identity");
 {auto oversized=encoded;oversized["frames"][0]["normals"]=nlohmann::json::binary(std::vector<uint8_t>(128*1024*1024+1));check(rejects([&]{NormalFrameArchive::decode(oversized);}),"Aggregate frame binary quota rejects oversized payload before allocating direction arrays");}
 auto bad=*f;bad.targets[0].tangents.assign(3,{1,0,0});check(rejects([&]{bad.validate();}),"N-only frame cannot silently add tangent morph channels");
 bad=*f;bad.origin_vertices[0]=3;check(rejects([&]{bad.validate();}),"Normal frame origin vertex identities remain bounded");
 bad=*f;bad.generated_tangents=true;check(rejects([&]{bad.validate();}),"Generated tangent provenance requires actual tangent and UV authority");
 auto m=triangle();auto paint=std::make_shared<PaintedMesh>(m,64);auto origin=std::make_shared<SourceUvOrigin>();origin->paint=paint->snapshot();
 auto deformation=std::make_shared<DeformationState>(m);deformation->morphs=MorphSet(m);auto edited=m.positions;edited[0].z=.2f;deformation->morphs->set_target(m,"native",edited);origin->deformation=deformation;
 auto binding=std::make_shared<SourceMaterialBindings>();binding->has_mtoon=false;binding->uv_origin=origin;binding->normal_frames=set;binding->normal_frame=SourceMaterialNormalFrame::OriginalAttributes;binding->validate();
 Scene scene;scene.active().painted=paint;scene.active().mesh_active=true;scene.active().material_bindings=binding;scene.set_deformation(scene.active().id,*deformation);scene.duplicate(scene.active().id);
 auto saved=scene.encode();check(saved["version"]==9 && saved["source_materials"]["version"]==2,"New original normal authority is explicitly Scene9/material archive2");
 auto loaded=Scene::decode(nlohmann::json::from_cbor(nlohmann::json::to_cbor(saved)));
 check(loaded.encode()==saved && loaded.object(1).material_bindings==loaded.object(2).material_bindings && loaded.object(1).material_bindings->normal_frames->pbr==loaded.object(1).material_bindings->normal_frames->mtoon,"Scene restart preserves original local rest/morph authority and role aliases exactly");
 for(int version:{7,8}){auto old=saved;old["version"]=version;check(rejects([&]{Scene::decode(old);}),"Older scene versions reject new frame schema instead of reinterpreting converted normals");}
 auto legacy=scene;legacy.object(1).material_bindings.reset();legacy.object(2).material_bindings.reset();check(legacy.encode()["version"]==5,"Ordinary deformation retains the existing native file shape");
 auto oldbinding=std::make_shared<SourceMaterialBindings>();oldbinding->has_mtoon=false;legacy.object(1).material_bindings=oldbinding;auto oldsaved=legacy.encode();check(oldsaved["version"]==8 && oldsaved["source_materials"]["version"]==1 && Scene::decode(oldsaved).encode()==oldsaved,"Existing Scene8/material1 retains unchanged UvDerivative meaning and archive layout");
 auto missing=*binding;missing.uv_origin.reset();check(rejects([&]{missing.validate();}),"Original normal frames require retained native position authority");
 auto unknown=*set;unknown.targets[0].origin_slot=1;auto unknownbinding=*binding;unknownbinding.normal_frames=std::make_shared<const NormalFrameSet>(unknown);check(rejects([&]{unknownbinding.validate();}),"Captured native morph slots cannot be inferred by target name or array count");
 {Scene ordinary;const auto expected=ordinary.encode();for(int version:{2,3}){auto old=expected;old["version"]=version;for(auto &object:old["objects"]){object.erase("deformation");if(version==2){object.erase("painted");object.erase("mesh_active");}}check(Scene::decode(old).encode()==expected,"Historical pre-deformation Scene2/3 remains readable without inventing required deformation fields");}}
 std::cout<<"Frame archive checks "<<checks<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
