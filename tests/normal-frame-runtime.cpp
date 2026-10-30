// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/normal-frame-runtime.hpp"
#include <iostream>
#include "dome/document.hpp"
#include "dome/source-uv-bake.hpp"
#include "normal-frame-fixtures.hpp"
using namespace dome;
namespace {int checks=0;void check(bool v,const char*s){if(!v)throw std::runtime_error(s);++checks;std::cout<<"PASS "<<s<<'\n';}bool rejects(auto f){try{f();return false;}catch(const std::exception&){return true;}}bool near(Vec3 a,Vec3 b){return length(a-b)<1e-6f;}}
int main()try {
 Mesh mesh;mesh.positions={{0,0,0},{1,0,0},{1,1,0},{0,1,0}};mesh.normals.assign(4,{0,0,1});mesh.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};mesh.indices={0,1,2,0,2,3};
 auto f=std::make_shared<NormalFrames>();f->origin_vertex_count=4;f->origin_vertices=mesh.indices;f->normals.assign(6,{0,0,1});f->tangents.assign(6,{1,0,0,1});f->targets.push_back({1,std::vector<Vec3>(6,Vec3{1,0,0}),std::vector<Vec3>(6,Vec3{0,1,0})});
 auto set=std::make_shared<NormalFrameSet>();set->pbr=set->mtoon=f;set->targets={{1,0,std::nullopt,{}}};set->current_targets={1};set->next_target=2;
 auto origin=std::make_shared<SourceUvOrigin>();origin->paint=std::make_shared<PaintedMesh>(mesh,64);auto rest=std::make_shared<DeformationState>(mesh);rest->morphs=MorphSet(mesh);auto edited=mesh.positions;edited[0].z=.1f;rest->morphs->set_target(mesh,"original",edited);origin->deformation=rest;
 SourceMaterialBindings binding;binding.has_mtoon=false;binding.uv_origin=origin;binding.normal_frames=set;binding.normal_frame=SourceMaterialNormalFrame::OriginalAttributes;binding.validate();
 DeformationState current=*rest;current.morph_weights={.5f};current.skeleton=Skeleton{{{"root",-1,{},1}}};current.skin=SkinWeights{};current.skin->vertices.assign(4,{{0,1}});current.pose={{{},{},{2,1,.5f}}};
 detail::normal_frame_current_authority(binding,mesh,&current);check(true,"Legitimate current skin, pose and morph weight edits retain original local POSITION authority");
 auto layout=detail::normal_frame_layout(mesh,*set);check(layout.vertices.size()==4 && layout.indices==mesh.indices,"Complete equal base and target frames reuse vertices without welding distinct atlas identities");
 auto posed=detail::normal_frame_preview(*set,layout,&current);
 check(near(posed.normals[0][0],{.25f,0,2}) && near(Vec3{posed.tangents[0][0][0],posed.tangents[0][0][1],posed.tangents[0][0][2]},{2,.5f,0}),"Raw morph directions transport through actual anisotropic skin matrices using declared joint normal policy");
 check(std::abs(length(posed.normals[0][0])-1)>.5f && std::abs(dot(posed.normals[0][0],Vec3{posed.tangents[0][0][0],posed.tangents[0][0][1],posed.tangents[0][0][2]}))>.1f,"Attribute transport preserves nonorthogonal raw directions for version-specific shader evaluation");
 auto secondary=std::make_shared<NormalFrames>(*f);secondary->targets[0].normals[3].y=.2f;auto separate=*set;separate.mtoon=secondary;auto split=detail::normal_frame_layout(mesh,separate);
 check(split.vertices.size()==5 && split.indices[0]!=split.indices[3],"A secondary role target normal forces a split even when every base tangent agrees");
 auto onlyN=std::make_shared<NormalFrames>(*f);onlyN->tangents.clear();onlyN->targets[0].tangents.clear();auto nset=*set;nset.pbr=onlyN;nset.mtoon.reset();auto nlayout=detail::normal_frame_layout(mesh,nset);auto normal=detail::normal_frame_preview(nset,nlayout,&current);
 check(normal.tangents[0].empty() && near(normal.normals[0][0],{.25f,0,2}),"N-only frames receive the same morph/skin normal transport without inventing a tangent");
 auto stale=mesh;stale.positions[0].z=.01f;check(rejects([&]{detail::normal_frame_current_authority(binding,stale,&current);}),"Changed native rest positions require explicit frame reauthoring");
 auto replaced=current;replaced.morphs->targets[0].delta[0].z=.2f;check(rejects([&]{detail::normal_frame_current_authority(binding,mesh,&replaced);}),"Morph replacement cannot reuse historical target normal identity silently");
 replaced=current;replaced.morphs->targets[0].name="renamed";detail::normal_frame_current_authority(binding,mesh,&replaced);check(true,"Renaming a target preserves its explicit identity and POSITION authority");
 auto reordered=*set;reordered.current_targets.clear();binding.normal_frames=std::make_shared<const NormalFrameSet>(reordered);check(rejects([&]{detail::normal_frame_current_authority(binding,mesh,&current);}),"Current native slot table must exactly cover active targets");binding.normal_frames=set;
 check(rejects([&]{detail::normal_frame_layout(mesh,*set,[]{return true;});}),"Immutable layout publication is cancelled before a partial result is returned");
 check(rejects([&]{detail::normal_frame_layout(mesh,*set,{},8);}),"Frame splitting obeys an explicit CPU work quota");
 Scene scene;scene.active().painted=std::make_shared<PaintedMesh>(mesh,64);scene.active().mesh_active=true;
 scene.active().material_bindings=std::make_shared<const SourceMaterialBindings>(binding);scene.set_deformation(scene.active().id,current);
 {
  Document document;document.replace(scene);check(document.wait_mesh(),"Actual asynchronous mesh worker publishes the retained-frame preview");
  auto published=document.frame().surface;
  check(published->ranges.size()==1 && published->ranges[0].source_frame && !published->ranges[0].source_normal_only && published->source_normals.size()==4 && near(published->source_normals[0],{.25f,0,2}),"Worker packs posed original attributes and full frame layout into one immutable renderer snapshot");
  check(document.copy_scene().active().material_bindings->uv_origin->deformation==origin->deformation && near(document.copy_scene().active().painted->mesh().positions[0],mesh.positions[0]),"Worker never modifies original rest geometry or captured native deformation");
  auto posed_scene=scene;auto edited_pose=current;edited_pose.pose[0].scale={1,2,1};posed_scene.set_deformation(posed_scene.active().id,edited_pose);
  document.replace(posed_scene);check(document.wait_mesh(),"A pose-only edit publishes new original normal directions");auto posed_snapshot=document.frame().surface;
  check(posed_snapshot->normal_frame_layout_builds==published->normal_frame_layout_builds && near(posed_snapshot->source_normals[0],{.5f,0,1}),"Pose-only changes reuse the immutable complete-channel split layout without rebuilding it");
  auto replacement=std::make_shared<SourceMaterialBindings>(*scene.active().material_bindings);replacement->normal_frames=std::make_shared<const NormalFrameSet>(*set);posed_scene.active().material_bindings=replacement;
  document.replace(posed_scene);check(document.wait_mesh() && document.frame().surface->normal_frame_layout_builds==posed_snapshot->normal_frame_layout_builds+1,"A replaced frame-set identity invalidates the cached layout before publication");
 }
 auto nbindings=std::make_shared<SourceMaterialBindings>(binding);nbindings->normal_frames=std::make_shared<const NormalFrameSet>(nset);scene.active().material_bindings=nbindings;
 {
  Document document;document.replace(scene);check(document.wait_mesh(),"Actual mesh worker accepts a genuinely N-only material frame");
  auto published=document.frame().surface;check(published->ranges[0].source_normal_only && near(published->source_normals[0],{.25f,0,2}) && nbindings->normal_frames->pbr->tangents.empty(),"GPU attribute padding does not create a retained native tangent authority for N-only materials");
 }
 auto textures=std::make_shared<PaintSourceMaps>();textures->normal.image=std::make_shared<const Image>(Image{1,1,{255,128,128,255}});
 scene.active().painted=PaintedMesh::from_maps(mesh,textures,64);scene.active().material_bindings=std::make_shared<const SourceMaterialBindings>(binding);
 SourceUvBakeInput input;input.paint=scene.active().painted;input.current_material=scene.active().material_bindings;input.current_deformation=scene.active().deformation;
 for(auto v:mesh.indices){input.source_vertices.push_back(v);input.target.positions.push_back(mesh.positions[v]);input.target.normals.push_back(mesh.normals[v]);auto q=mesh.uv[v];input.target.uv.push_back({1-q[1],q[0]});input.target.indices.push_back(uint32_t(input.target.positions.size()-1));}
 SourceUvBakeOptions options;options.resolution=64;auto baked=bake_source_uv(input,options);auto adoption=adopt_source_uv_bake(baked);
 check(adoption.material_bindings->normal_frames==set && baked.maps->normal.image->rgba[0]==255 && baked.maps->normal.image->rgba[1]==128,"Frame-aware UV transfer preserves raw normal encodings and immutable normal payload identity");
 check(adoption.deformation->skin->vertices.size()==6 && adoption.deformation->morphs->targets[0].delta.size()==6 && adoption.deformation->morph_weights==current.morph_weights,"UV vertex splitting remaps actual current skin/morph authority without losing current pose or weights");
 scene.active().painted=adoption.paint;scene.active().material_bindings=adoption.material_bindings;scene.active().deformation=adoption.deformation;scene.validate();auto stored=scene.encode();
 check(Scene::decode(stored).encode()==stored,"CPU native archive preserves a frame-aware baked atlas and remapped deformation exactly");
 auto lost=baked;lost.normal_frames.reset();check(rejects([&]{adopt_source_uv_bake(lost);}),"Native adoption refuses silently discarded original normal authority");
 auto many=std::make_shared<DeformationState>(*rest);many->morphs->targets.clear();for(size_t n=0;n<128;++n)many->morphs->targets.push_back({"target"+std::to_string(n),std::vector<Vec3>(4)});
 Mesh sparse=mesh;sparse.positions.assign(65'000,mesh.positions[0]);sparse.normals.assign(65'000,{0,0,1});sparse.uv.assign(65'000,{0,0});std::vector<uint32_t> remap(65'000,0);
 check(rejects([&]{detail::normal_frame_remap_deformation(many,mesh,sparse,remap);}),"Deformation remap preflights expanded morph dimensions before allocating authoring arrays");
 {
  auto source=normal_frame_fixture::cube();const auto stored=source.encode();Document document;document.replace(source);check(document.wait_mesh(),"Retained-frame source publishes before the native plane-cut preview");document.begin_cut();check(document.wait_cut() && document.frame().state->cut->preview && !document.frame().state->cut->preview->cap_mesh.indices.empty(),"Native plane-cut worker produces a cap for a retained-frame source");auto preview=document.frame().surface;const auto count=preview->mesh.positions.size();
  check(preview->source_normals.size()==count && preview->canonical_normals.size()==count && preview->source_tangents.size()==count && preview->ranges.back().object==UINT64_MAX && !preview->ranges.back().source_frame,"Generic cut cap pads shared direction streams without claiming source frame authority");document.end_cut(false);check(document.copy_scene().encode()==stored,"Cancelling retained-frame cut preview preserves the complete original native payload");
 }
 std::cout<<"Normal frame runtime checks "<<checks<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
