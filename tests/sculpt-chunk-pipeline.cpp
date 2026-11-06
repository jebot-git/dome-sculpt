// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/material-visibility.hpp"
#include "dome/source-placement.hpp"
#include "sculpt-chunk-masks.hpp"
#include "sculpt-placed-chunks.hpp"
#include "affine-surface.hpp"
#include <nlohmann/json.hpp>
#include <bit>
#include <filesystem>
#include <iostream>
using namespace dome;
using namespace dome::detail;
namespace {
size_t pipeline_checks=0;
void pipeline_check(bool value,const char *text){if(!value)throw std::runtime_error(text);++pipeline_checks;std::cout<<"PASS "<<text<<'\n';}
template<class F> bool fails(F fn){try{fn();return false;}catch(const std::exception &){return true;}}
bool bits(Vec3 a,Vec3 b){return std::bit_cast<std::array<uint32_t,3>>(a)==std::bit_cast<std::array<uint32_t,3>>(b);}
void stamp(Volume &v,Vec3 point,float radius=.085f){Brush b;b.from=b.to=point;b.radius=radius;b.tool=Tool::Clay;b.blend=0;v.begin_stroke();v.stamp(b);v.end_stroke();}
Volume shells(){
  Volume::Voxels values;
  for(int x=-16;x<=16;++x)for(int y=-9;y<=9;++y)for(int z=-9;z<=9;++z){
    const Vec3 a{float(x)-6.17f,float(y)+.31f,float(z)-.23f},b{float(x)+6.39f,float(y)-.41f,float(z)+.37f};
    values.emplace(Int3{x,y,z},std::clamp(std::min(length(a)-4.35f,length(b)-3.93f),-4.f,4.f)*.04f);
  }
  return Volume::from_samples(.04f,std::move(values));
}
Scene fixture(){
  Scene scene;scene.active().volume=std::make_shared<Volume>(shells());scene.active().material.double_sided=true;
  auto plane=scene.add("Packed intervening plane",Volume{});auto &o=scene.object(plane);o.mesh_active=true;
  Mesh mesh;mesh.positions={{-.3f,-.3f,-.9f},{.3f,-.3f,-.9f},{0,.3f,-.9f}};mesh.normals.assign(3,{0,0,1});mesh.indices={0,1,2};mesh.uv={{0,0},{1,0},{.5f,1}};
  o.painted=std::make_shared<PaintedMesh>(std::move(mesh),64);o.material.double_sided=true;
  auto second=scene.add("Second sculpt",shells());scene.object(second).transform.position={1.3f,0,0};scene.object(second).material.double_sided=true;
  const auto hidden=scene.add("Hidden sculpt",shells());scene.object(hidden).visible=false;scene.select(1);return scene;
}
std::shared_ptr<MeshSnapshot> reference(const Scene &scene,bool masks=false){
  auto result=std::make_shared<MeshSnapshot>();result->generation=7;
  for(auto object:scene.preview_surfaces()){
    auto mesh=object.preview_mesh();const auto affine=bool(object.affine_edit)||std::any_of(object.groups.begin(),object.groups.end(),[](const auto &g){return g.affine_edit||g.source_placement;});
    const auto frame=affine?std::optional<AffineInteractionFrame>(placement_frame(object)):std::nullopt;
    const auto world=affine?CanvasTransform{}:object.world_transform();
    const size_t offset=result->mesh.positions.size(),first=result->mesh.indices.size();
    for(size_t v=0;v<mesh.positions.size();++v){
      if(masks)result->sculpt_masks.push_back(object.id==scene.active().id&&!scene.active().mesh_active?scene.object(object.id).volume->mask_sample(mesh.positions[v]):-1.f);
      const auto p=ob::transform(world,{mesh.positions[v].x,mesh.positions[v].y,mesh.positions[v].z});
      const auto n=ob::rotate(world.orientation,{mesh.normals[v].x,mesh.normals[v].y,mesh.normals[v].z});
      result->mesh.positions.push_back(frame?placed_position(*frame,mesh.positions[v]):Vec3{p.x,p.y,p.z});
      result->mesh.normals.push_back(frame?placed_normal(*frame,mesh.normals[v]):Vec3{n.x,n.y,n.z});
      result->mesh.uv.push_back(mesh.uv.empty()?std::array<float,2>{}:mesh.uv[v]);
    }
    for(auto index:mesh.indices)result->mesh.indices.push_back(uint32_t(offset+index));
    if(frame&&frame->orientation()==AffineOrientation::Reversing)for(size_t c=first;c<result->mesh.indices.size();c+=3)std::swap(result->mesh.indices[c+1],result->mesh.indices[c+2]);
    result->ranges.push_back({object.id,first,mesh.indices.size(),object.material,object.textures,{},offset});
  }
  return result;
}
void corners_equal(const Scene &scene,const SculptScenePreview &preview){
  const auto expected=reference(scene);
  for(const auto &object:preview.chunks->objects()){
    const auto range=std::find_if(expected->ranges.begin(),expected->ranges.end(),[&](const auto &r){return r.object==object.object;});
    pipeline_check(range!=expected->ranges.end(),"A chunk object has a genuine independent whole-mesher range");size_t at=range->first;
    object.geometry->for_each_run([&](const SculptSurfaceRun &run){
      const auto placed=object.placed->find(run.chunk);const auto &mesh=*placed->geometry_owner();
      for(size_t c=run.first;c<run.first+run.count;++c){
        const auto corner=object.reflected()&&c%3?c/3*3+3-c%3:c;
        const auto v=mesh.indices.at(corner),w=expected->mesh.indices.at(at++);
        if(!bits(object.placed_position(mesh.positions.at(v)),expected->mesh.positions.at(w))||!bits(object.placed_normal(mesh.normals.at(v)),expected->mesh.normals.at(w)))throw std::runtime_error("Original-order independent world corner/normal mismatch");
      }
    });
    pipeline_check(at==range->first+range->count,"Original Scene/cell/triangle draw runs preserve every whole-reference corner and normal bit");
  }
}
void ready(Document &document){if(!document.wait_mesh(std::chrono::seconds(45)))throw std::runtime_error("Mesh worker did not publish: "+document.frame().state->error);if(!document.frame().state->error.empty())throw std::runtime_error(document.frame().state->error);}
}
int main(int argc,char **argv)try{
  if(argc!=2)throw std::runtime_error("Usage: sculpt-chunk-pipeline output-directory");const std::filesystem::path output=argv[1];std::filesystem::create_directories(output);
  auto scene=fixture();const auto original=scene.encode();
  auto first=SculptSceneChunks::capture(scene,{7,1,1,true,true});
  pipeline_check(first.chunks->objects().size()==2&&first.legacy.size()==1,"Actual mixed capture includes two generated objects and one genuine packed object, excluding hidden geometry");
  pipeline_check(first.chunks->object_order()==std::vector<uint64_t>{1,2,3,4},"Original authored Scene order is retained separately from draw queue order");
  corners_equal(scene,first);
  const auto *one=first.chunks->find(1);pipeline_check(one&&one->placed&&one->placed->has_queries()&&one->masks,"Worker capture owns real acceleration and independently sampled scalar packets");
  one->geometry->for_each_chunk([&](const auto &chunk){const auto values=one->masks->find(chunk);for(size_t v=0;v<values->values.size();++v)if(values->values[v]!=scene.object(1).volume->mask_sample(chunk->mesh.positions[v]))throw std::runtime_error("Independent mask sampling differs");});
  pipeline_check(true,"Every scalar equals the actual Volume sampler at its own original local vertex");
  auto full=reference(scene);MaterialSurfaceQuery oracle(full->mesh,full->ranges);
  // Empty packed geometry is a valid separate chunk-only scene query, rather
  // than a fake packed surface or a local-as-global triangle number.
  MaterialSurfaceQuery actual(Mesh{},std::span<const SurfaceRange>{},{},std::span<const std::array<float,4>>{},PaintVertexCoordinates{},first.chunks);
  size_t probes=0;
  one->placed->for_each([&](const auto &packet){
    const auto &mesh=*packet->geometry_owner();if(probes>=12)return;
    const auto a=mesh.indices[0],b=mesh.indices[1],c=mesh.indices[2];const auto &object=*one;const auto center=(object.placed_position(mesh.positions[a])+object.placed_position(mesh.positions[b])+object.placed_position(mesh.positions[c]))/3.f;
    const auto n=normalized(object.placed_normal(mesh.normals[a])+object.placed_normal(mesh.normals[b])+object.placed_normal(mesh.normals[c]));const auto p=center+n*.03f;
    const auto hit=actual.raycast_target(p,n*-1,.08f,0,1),expected=oracle.raycast_target(p,n*-1,.08f,0,1);
    if(!hit||!expected||hit->packed()||hit->object!=1||!one->geometry->owns(hit->chunk)||length(hit->surface.position-expected->surface.position)>2e-5f||std::abs(hit->surface.distance-expected->surface.distance)>2e-5f)throw std::runtime_error("Actual typed chunk ray differs from independent packed reference");
    ++probes;
  });
  pipeline_check(probes>0,"Typed immutable chunk ray hits retain actual object/material identity and match independent whole-reference geometry");
  pipeline_check(fails([&]{actual.raycast({0,0,1},{0,0,-1});}),"An untyped caller cannot reinterpret a chunk-local triangle as a packed triangle");
  const auto before_geometry=one->geometry;const auto before_placed=one->placed;const auto before_masks=one->masks;
  scene.object(1).volume->fill_mask(.7f);auto scalar=SculptSceneChunks::capture(scene,{7,2,1,true,true},first.chunks);
  const auto *masked=scalar.chunks->find(1);
  pipeline_check(masked->geometry==before_geometry&&masked->placed==before_placed&&masked->masks!=before_masks,"A real mask edit changes scalar ownership without geometry extraction, placed geometry or BVH rebuild");
  pipeline_check(scalar.chunks->find(3)->geometry==first.chunks->find(3)->geometry&&scalar.chunks->find(3)->placed==first.chunks->find(3)->placed,"Unrelated object's immutable placed owners remain exact");
  masked->geometry->for_each_chunk([&](const auto &chunk){for(float v:masked->masks->find(chunk)->values)if(v!=.7f)throw std::runtime_error("Real full-mask scalar was not sampled");for(float v:before_masks->find(chunk)->values)if(v!=0)throw std::runtime_error("Previous scalar owner was mutated");});
  pipeline_check(true,"New real protection values coexist with immutable unchanged earlier scalar packets");
  scene.object(1).volume->fill_mask(0);stamp(*scene.object(1).volume,{.08f,.04f,0});auto edited=SculptSceneChunks::capture(scene,{7,3,1,true,true},scalar.chunks);corners_equal(scene,edited);
  pipeline_check(edited.chunks->find(3)->placed==scalar.chunks->find(3)->placed,"A local sculpt changes no placed owner in another object");
  size_t polls=0;pipeline_check(fails([&]{SculptSceneChunks::capture(scene,{7,4,1,true,true},edited.chunks,{},[&]{return ++polls>=8;});}),"Cancellation rejects a detached candidate before publication");
  pipeline_check(first.chunks->find(1)->geometry==before_geometry&&original!=scene.encode(),"Retained previous source remains immutable through changed geometry and cancellation");
  for(double sign:{1.,-1.}){
    auto affine_scene=fixture();affine_scene.object(1).affine_edit=std::make_shared<NativeAffineEdit>(AffineTransform(std::array<double,16>{sign,0,0,0,.27,1.2,0,0,.13,.09,.8,0,.1,-.2,.15,1}));
    auto affine=SculptSceneChunks::capture(affine_scene,{8,1,1,true,true});corners_equal(affine_scene,affine);
    pipeline_check(affine.chunks->find(1)->reflected()==(sign<0),"Genuine sheared/reflected authored placement retains exact handedness");
    const auto reference_scene=reference(affine_scene);MaterialSurfaceQuery independent(reference_scene->mesh,reference_scene->ranges);
    MaterialSurfaceQuery query(Mesh{},std::span<const SurfaceRange>{},{},std::span<const std::array<float,4>>{},PaintVertexCoordinates{},affine.chunks);
    size_t affine_probes=0;
    affine.chunks->find(1)->placed->for_each([&](const auto &packet){
      if(affine_probes>=8)return;const auto &mesh=*packet->geometry_owner();
      const auto a=mesh.indices[0],b=mesh.indices[1],c=mesh.indices[2];const auto &object=*affine.chunks->find(1);const auto center=(object.placed_position(mesh.positions[a])+object.placed_position(mesh.positions[b])+object.placed_position(mesh.positions[c]))/3.f;
      const auto n=normalized(object.placed_normal(mesh.normals[a])+object.placed_normal(mesh.normals[b])+object.placed_normal(mesh.normals[c])),p=center+n*.03f;
      const auto actual=query.raycast_target(p,n*-1,.08f,0,1),expected=independent.raycast_target(p,n*-1,.08f,0,1);
      if(!actual||!expected||actual->object!=1||actual->packed()||length(actual->surface.position-expected->surface.position)>3e-5f||std::abs(actual->surface.distance-expected->surface.distance)>3e-5f||length(actual->surface.normal-expected->surface.normal)>3e-4f)
        throw std::runtime_error("Affine mixed ray metric/normal differs from independently packed world geometry");
      const auto contact=query.closest_target(p,1),nearest=independent.closest_target(p,1);
      if(contact.packed()||contact.object!=1||std::abs(contact.surface.distance-nearest.surface.distance)>3e-5f||length(contact.surface.position-nearest.surface.position)>3e-5f)
        throw std::runtime_error("Affine chunk nearest metric differs from independent world metric");
      ++affine_probes;
    });
    pipeline_check(affine_probes>0,"Sheared/reflected typed ray and nearest hits retain exact world distance, normal and object identity");
  }
  auto disk=fixture();disk.save(output/"input.dsculpt");Document document;document.open(output/"input.dsculpt");document.configure_selection(true);ready(document);
  const auto initial=document.copy_scene().encode();const auto retained=document.frame().surface;
  pipeline_check(retained->sculpt_chunks&&retained->ranges.size()==1&&retained->picking,"Actual Document publishes mixed immutable chunks, genuine packed range and typed picking");
  Brush b;b.from=b.to={.08f,.04f,0};b.radius=.085f;b.tool=Tool::Clay;b.blend=0;
  pipeline_check(document.begin_desktop(),"Actual native sculpt gesture begins");document.stamp_desktop(b);document.cancel_desktop();ready(document);
  pipeline_check(document.copy_scene().encode()==initial,"Native gesture Cancel preserves exact model/history endpoint");
  pipeline_check(document.begin_desktop(),"Actual native sculpt gesture can retry after Cancel");document.stamp_desktop(b);document.end_desktop();ready(document);const auto accepted=document.copy_scene().encode();
  pipeline_check(accepted!=initial&&document.frame().surface->sculpt_chunks!=retained->sculpt_chunks,"Real accepted native sculpt publishes a distinct complete immutable chunk snapshot");
  document.save(output/"accepted.dsculpt");pipeline_check(Scene::load(output/"accepted.dsculpt").encode()==accepted,"Ordered Save persists accepted real scene independently of derived chunk caches");
  pipeline_check(document.undo(),"Accepted sculpt has an Undo endpoint");ready(document);pipeline_check(document.copy_scene().encode()==initial,"One Undo restores exact original source scene");
  pipeline_check(document.redo(),"Accepted sculpt has a Redo endpoint");ready(document);pipeline_check(document.copy_scene().encode()==accepted,"One Redo restores exact accepted source scene");
  pipeline_check(retained->sculpt_chunks->find(1)&&retained->sculpt_chunks->find(1)->geometry->report().indices>0,"Previously acquired complete worker snapshot remains valid after Cancel, Apply, Save, Undo and Redo");
  std::cout<<"PASS total "<<pipeline_checks<<'\n';return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
