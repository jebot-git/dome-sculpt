// SPDX-License-Identifier: GPL-3.0-or-later
#include "normal-bake-frames.hpp"
#include "dome/paint.hpp"
#include "dome/tangents.hpp"
#include "dome/humanoid-creator.hpp"
#include <iostream>
using namespace dome;
namespace {
unsigned checks=0;
void check(bool value,const char *text){if(!value)throw std::runtime_error(text);++checks;std::cout<<"PASS "<<text<<'\n';}
template<class F>bool refuses(F f){try{f();return false;}catch(const std::exception&){return true;}}
bool equal(Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;}
Mesh mesh(){Mesh m;m.positions={{0,0,0},{1,0,0},{1,1,0},{0,1,0}};m.normals.assign(4,{0,0,1});m.uv={{0,0},{1,0},{1,1},{0,1}};m.indices={0,1,2,0,2,3};return m;}
}
int main()try {
  auto m=mesh();DeformationState state(m);state.morphs=MorphSet(m);
  auto edited=m.positions;edited[2].z=.25f;state.morphs->set_target(m,"raised",edited);
  auto edited2=m.positions;edited2[0].z=-.1f;state.morphs->set_target(m,"lowered",edited2);
  state.morph_weights={.35f,.2f};
  auto result=detail::normal_bake_frame_candidate(m,&state,nullptr);
  const auto &set=*result.frames;const auto &f=*set.pbr;
  check(result.authored_native_normals&&result.authored_native_tangents&&set.pbr==set.mtoon&&!set.canonical,
        "Detached generic conversion authors explicit shared attributes without fabricated canonical authority");
  check(set.current_targets==std::vector<uint64_t>{1,2}&&set.next_target==3&&set.targets[1].origin_slot==1,
        "Every native target receives a stable ID and explicit native slot authority");
  check(f.origin_vertices==m.indices&&f.source_vertices.empty()&&f.source_corners.empty(),
        "Native geometry does not invent raw imported source identities");
  auto base=tangent_corners(m);
  check(base==f.tangents,"Base tangent attributes equal actual Mikk output");
  auto authored=detail::normal_bake_author_native_target(m,f,edited,71);
  check(authored.attributes.id==71&&authored.positions.size()==m.positions.size()&&authored.attributes.normals.size()==m.indices.size()&&authored.attributes.tangents.size()==m.indices.size(),
        "Reusable key factory emits explicit native POSITION and full corner N/T under the supplied stable ID");
  bool key_same=true;for(size_t c=0;c<m.indices.size();++c)key_same&=equal(authored.attributes.normals[c],f.targets[0].normals[c])&&equal(authored.attributes.tangents[c],f.targets[0].tangents[c]);
  check(key_same&&equal(authored.positions[2],state.morphs->targets[0].delta[2]),"Independent single-key authoring matches the full candidate without editing any retained key");
  Mesh split;for(auto v:m.indices){split.positions.push_back(m.positions[v]);split.normals.push_back(m.normals[v]);split.uv.push_back(m.uv[v]);split.indices.push_back(uint32_t(split.indices.size()));}
  std::vector<Vec3> split_delta;for(auto v:m.indices)split_delta.push_back(authored.positions[v]);
  auto origin_delta=detail::normal_bake_target_origin_positions(split,m,split_delta);
  bool mapped=true;for(size_t v=0;v<m.positions.size();++v)mapped&=equal(origin_delta[v],authored.positions[v]);
  check(mapped,"Exact ordered-corner remap restores native origin POSITION authority after a real UV vertex split");
  auto contradictory=split_delta;contradictory[3].x+=.01f;
  check(refuses([&]{detail::normal_bake_target_origin_positions(split,m,contradictory);}),"Disagreeing UV copies cannot silently average a new target back onto the retained origin");
  auto moved=split;moved.positions[0].x+=.01f;
  check(refuses([&]{detail::normal_bake_target_origin_positions(moved,m,split_delta);}),"Origin remapping refuses changed rest positions even when corner counts agree");
  auto unreferenced=m;unreferenced.positions.push_back({2,2,0});unreferenced.normals.push_back({0,0,1});unreferenced.uv.push_back({.5f,.5f});
  check(refuses([&]{detail::normal_bake_target_origin_positions(split,unreferenced,split_delta);}),"Unreferenced origin vertices require explicit whole-domain authority instead of invented zero deltas");
  for(size_t slot=0;slot<2;++slot){std::vector<float> weights(2);weights[slot]=1;auto key=state.morphs->evaluate(m,weights);auto tangents=tangent_corners(key);
    bool same=true;for(size_t c=0;c<m.indices.size();++c){auto v=m.indices[c];same&=equal(f.normals[c]+f.targets[slot].normals[c],key.normals[v]);same&=length(Vec3{base[c][0],base[c][1],base[c][2]}+f.targets[slot].tangents[c]-Vec3{tangents[c][0],tangents[c][1],tangents[c][2]})<1e-7f;}
    check(same,"Normal and tangent target deltas reconstruct the corresponding native key before skin");
  }
  auto zero=state;zero.morph_weights.clear();auto zero_result=detail::normal_bake_frame_candidate(m,&zero,nullptr);
  check(zero_result.frames->pbr->tangents==f.tangents&&equal(zero_result.frames->pbr->targets[0].normals[2],f.targets[0].normals[2]),
        "Current preview morph weights cannot contaminate authored key attributes");
  auto paint=std::make_shared<PaintedMesh>(m,64);auto origin=std::make_shared<SourceUvOrigin>();origin->paint=paint->snapshot();origin->deformation=std::make_shared<const DeformationState>(state);
  auto n=std::make_shared<NormalFrames>(f);n->tangents.clear();n->generated_tangents=false;n->generated_uv.reset();for(auto &target:n->targets)target.tangents.clear();
  // Deliberately artist-authored directions, not normals derived from geometry.
  n->normals[1]=normalized(Vec3{.15f,.1f,1});n->targets[0].normals[1]={.02f,-.03f,.01f};
  n->source_vertex_count=4;n->source_corner_count=6;n->source_vertices=m.indices;n->source_corners={0,1,2,3,4,5};
  auto asset=std::make_shared<ImportedAsset>();ImportedPrimitive primitive;primitive.mesh=m;primitive.source_mesh=primitive.source_primitive=0;asset->primitives.push_back(primitive);
  auto appearance=std::make_shared<CreatorAppearance>();appearance->asset=asset;appearance->source_mesh=appearance->source_primitive=0;appearance->source_vertices={0,1,2,3};origin->appearance=appearance;
  auto retained=std::make_shared<NormalFrameSet>(set);retained->pbr=retained->mtoon=n;
  SourceMaterialBindings binding;binding.has_mtoon=false;binding.normal_frame=SourceMaterialNormalFrame::OriginalAttributes;binding.normal_frames=retained;binding.uv_origin=origin;binding.validate();
  auto upgraded=detail::normal_bake_frame_candidate(m,&state,&binding);const auto &u=*upgraded.frames->pbr;
  check(!upgraded.authored_native_normals&&upgraded.authored_native_tangents&&u.generated_tangents&&u.generated_uv==0,
        "N-only upgrade explicitly authors current-native-UV tangents without relabeling artist normals");
  bool exact=u.normals.size()==n->normals.size();for(size_t c=0;c<u.normals.size();++c)exact&=equal(u.normals[c],n->normals[c])&&equal(u.targets[0].normals[c],n->targets[0].normals[c]);
  check(exact&&u.source_vertices==n->source_vertices&&u.source_corners==n->source_corners,
        "Artist base/target N and raw source corner/vertex identities survive tangent authoring exactly");
  check(n->tangents.empty()&&retained->pbr==n&&!retained->canonical,
        "Detached generation never mutates retained original objects");
  auto inactive=*retained;inactive.current_targets={1};auto inactive_binding=binding;inactive_binding.normal_frames=std::make_shared<const NormalFrameSet>(inactive);
  auto one=state;one.morphs->targets.resize(1);one.morph_weights={.35f};auto inactive_result=detail::normal_bake_frame_candidate(m,&one,&inactive_binding);
  check(inactive_result.frames->pbr->targets.size()==2&&!inactive_result.frames->pbr->targets[1].tangents.empty(),
        "Inactive retained target tangents derive from captured native POSITION authority");
  auto complete=binding;complete.normal_frames=upgraded.frames;auto reused=detail::normal_bake_frame_candidate(m,&state,&complete);
  check(reused.frames->pbr==upgraded.frames->pbr&&!reused.authored_native_normals&&!reused.authored_native_tangents,
        "Complete retained artist channels reuse immutable authority without recomputation");
  detail::NormalBakeFrameOptions tiny;tiny.max_bytes=1;
  check(refuses([&]{detail::normal_bake_frame_candidate(m,&state,nullptr,tiny);}),"Memory admission refuses before detached corner allocation");
  tiny={};tiny.max_work=1;check(refuses([&]{detail::normal_bake_frame_candidate(m,&state,nullptr,tiny);}),"Work admission bounds complete target generation");
  tiny={};tiny.cancelled=[]{return true;};check(refuses([&]{detail::normal_bake_frame_candidate(m,&state,nullptr,tiny);}),"Cancellation refuses before publication and leaves input authority intact");
  check(refuses([&]{detail::normal_bake_author_native_target(m,f,edited,0);}),"Native target authoring cannot publish a missing stable target identity");
  check(refuses([&]{detail::normal_bake_author_native_target(m,f,edited,72,tiny);}),"Native key authoring obeys cancellation independently of full preparation");
  check(refuses([&]{detail::normal_bake_target_origin_positions(split,m,split_delta,tiny);}),"Origin-domain remapping obeys cancellation before publication");
  auto opposite=f;for(auto &t:opposite.tangents)t[3]=-t[3];
  check(refuses([&]{detail::normal_bake_author_native_target(m,opposite,edited,72);}),"A new native key cannot replace an incompatible immutable authored tangent W");
  auto wrong=state;wrong.morphs->targets[0].delta[0].x+=.01f;check(refuses([&]{detail::normal_bake_frame_candidate(m,&wrong,&binding);}),"Changed native POSITION target cannot retain unrelated authored normal authority");
  std::cout<<"PASS "<<checks<<" detached normal authoring checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
