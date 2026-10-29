// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/paint.hpp"
#include "dome/projection.hpp"
#include "dome/source-maps.hpp"
#include "dome/scene.hpp"
#include "dome/humanoid-creator.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
namespace {
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);std::cout<<"PASS "<<m<<'\n';}
template<class F>bool rejects(F f){try{f();return false;}catch(const std::exception&){return true;}}
Mesh quad(float x=0,float z=0,bool mirror=false){Mesh m;m.positions={{x-1,-1,z},{x+1,-1,z},{x+1,1,z},{x-1,1,z}};m.normals.assign(4,{0,0,1});m.uv=mirror?std::vector<std::array<float,2>>{{{1,0}},{{0,0}},{{0,1}},{{1,1}}}:std::vector<std::array<float,2>>{{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.indices={0,1,2,0,2,3};return m;}
void append(Mesh&m,const Mesh&o){auto offset=uint32_t(m.positions.size());m.positions.insert(m.positions.end(),o.positions.begin(),o.positions.end());m.normals.insert(m.normals.end(),o.normals.begin(),o.normals.end());m.uv.insert(m.uv.end(),o.uv.begin(),o.uv.end());for(auto i:o.indices)m.indices.push_back(i+offset);}
std::shared_ptr<const PaintSourceMaps> maps(){auto s=std::make_shared<PaintSourceMaps>();s->color.image=std::make_shared<const Image>(Image{2,1,{255,0,0,0,0,128,255,255}});return s;}
PaintSurfaceOptions source(){PaintSurfaceOptions o;o.policy=PaintUvPolicy::SourceAuthored;return o;}
Brush brush(Vec3 p){Brush b;b.from=b.to=p;b.normal={0,0,1};b.radius=.4f;return b;}
Material white(){Material m;m.color={1,1,1,1};return m;}
}
int main(int argc,char **argv)try{
  Mesh linked=quad(-2);append(linked,quad(2,0,true));auto original=linked;auto appearance=maps();
  check(rejects([&]{PaintedMesh p(linked,64);})&&rejects([&]{PaintedMesh::from_maps(linked,appearance,64);}),"Ordinary authoring and default source maps keep strict independent UV validation");
  auto paint=PaintedMesh::from_maps(linked,appearance,64,source());const auto &d=paint->surface_diagnostics();
  check(d.policy==PaintUvPolicy::SourceAuthored&&d.linked_texels==4096&&d.additional_candidates==4096,"Explicit source UV policy retains bounded surface candidates for every shared mirrored chart texel");
  check(paint->mesh().indices==original.indices&&paint->mesh().uv==original.uv&&paint->mesh().positions.size()==original.positions.size(),"Source UV acceptance preserves original vertex order, triangles and authored UVs");
  auto initial=paint->composite(white())->color.rgba;PaintSettings settings;settings.color={0,1,0,1};auto b=brush({2,0,0});paint->begin_stroke();paint->stamp(b,settings);
  check(paint->end_stroke()&&paint->pixel(paint->active_layer(),32,32).data[1]==255,"A brush on the later mirrored surface updates its shared UV pixel instead of choosing the first source face");
  auto after=paint->composite(white())->color.rgba;auto held=paint->snapshot();
  check(after!=initial&&paint->undo()&&paint->composite(white())->color.rgba==initial&&paint->redo()&&paint->composite(white())->color.rgba==after,"Linked source painting preserves exact ordinary undo and redo");
  paint->begin_stroke();settings.color={0,0,1,1};paint->stamp(b,settings);paint->cancel_stroke();check(paint->composite(white())->color.rgba==after&&held->composite(white())->color.rgba==after,"Cancelling linked paint preserves prior source-backed snapshots exactly");
  Mesh depth=quad();append(depth,quad(0,.1f));auto nearest=PaintedMesh::from_maps(depth,appearance,64,source());b=brush({0,0,.1f});b.pressure=.5f;settings.color={1,1,1,1};nearest->begin_stroke();nearest->stamp(b,settings);nearest->end_stroke();
  auto coverage=nearest->pixel(nearest->active_layer(),32,32).data[9];check(coverage>=125&&coverage<=128,"Multiple eligible source surfaces select the nearest candidate and apply brush alpha once");
  ProjectionCamera camera;camera.pose.position={0,0,2};camera.width=camera.height=2;PaintProjection projection(Image{1,1,{255,0,0,255}},camera,depth);
  auto projected=PaintedMesh::from_maps(depth,appearance,64,source());b=brush({0,0,0});projected->begin_stroke();projected->stamp_projection(b,settings,projection);check(projected->end_stroke()&&projected->pixel(projected->active_layer(),32,32).data[0]==255,"Source projection selects a visible eligible candidate when the nearer brush candidate is occluded");
  auto stable=PaintedMesh::from_maps(linked,appearance,64,source());auto wide=camera;wide.width=10;
  PaintProjection colors(Image{2,1,{255,0,0,255,0,0,255,255}},wide,linked);auto tie=brush({0,0,0});tie.radius=3;
  stable->begin_stroke();stable->stamp_projection(tie,settings,colors);stable->end_stroke();auto &chosen=stable->pixel(stable->active_layer(),32,32);
  check(chosen.data[0]>chosen.data[2],"Equidistant linked source projection resolves consistently to the lowest eligible face ID");
  Mesh facing=quad();for(auto &n:facing.normals)n={0,0,-1};append(facing,quad(0,.1f));auto front=PaintedMesh::from_maps(facing,appearance,64,source());front->begin_stroke();front->stamp(b,settings);check(front->end_stroke(),"Shared source charts retain independently eligible front-facing surface normals");
  Mesh zero=quad();for(auto &uv:zero.uv)uv={.25f,.25f};check(rejects([&]{PaintedMesh p(zero,64);}),"Strict authoring still rejects zero-area UV triangles");
  auto retained=PaintedMesh::from_maps(zero,appearance,64,source());
  check(retained->mesh().indices==zero.indices&&retained->surface_diagnostics().unpaintable_uv_faces==std::vector<uint32_t>{0,1}&&retained->surface_diagnostics().covered_texels==0,"Zero-area source UV faces remain rendered with explicit unpaintable-face diagnostics");
  retained->begin_stroke();retained->stamp(brush({0,0,0}),settings);check(!retained->end_stroke()&&retained->has_paint_data()&&retained->layers()[0].tiles[0]==nullptr,"Unpaintable source UV faces do not allocate paint coverage or discard original appearance");
  auto restored=PaintedMesh::decode(paint->encode());check(restored->surface_diagnostics().policy==PaintUvPolicy::SourceAuthored&&restored->surface_diagnostics().linked_texels==4096&&restored->composite(white())->color.rgba==after,"Native restart reconstructs the explicit source UV policy and exact linked paint");
  auto invalid=paint->encode();invalid["uv_policy"]="silently-relax-all";check(rejects([&]{PaintedMesh::decode(invalid);}),"Unknown persisted UV policy cannot relax strict authoring silently");
  auto options=source();options.max_additional_candidates=0;check(rejects([&]{PaintedMesh::from_maps(linked,appearance,64,options);}),"Sparse linked surface storage has an explicit construction budget");
  options=source();options.max_candidates_per_texel=1;check(rejects([&]{PaintedMesh::from_maps(linked,appearance,64,options);}),"Per-texel candidate limits bound linked source evaluation");
  options=source();options.max_raster_samples=10;check(rejects([&]{PaintedMesh::from_maps(linked,appearance,64,options);}),"Source UV raster work is bounded before completing a large surface build");
  options=source();options.cancelled=[] {return true;};check(rejects([&]{PaintedMesh::from_maps(linked,appearance,64,options);}),"Cancelled source UV construction publishes no painted asset");
  options=source();options.max_candidate_checks=6000;auto bounded=PaintedMesh::from_maps(depth,appearance,64,options);auto before=bounded->encode();b=brush({0,0,0});b.radius=2;bounded->begin_stroke();
  check(rejects([&]{bounded->stamp(b,settings);})&&!bounded->pending_changes(),"Excess candidate evaluation rolls a partially processed stamp back exactly");bounded->cancel_stroke();check(bounded->encode()==before,"Candidate budget failure preserves exact native source and paint state");
  for(int arg=1;arg<argc;++arg){auto path=std::filesystem::path(argv[arg]);bool female=path.filename().string().find("female")!=std::string::npos;
    auto created=create_humanoid(humanoid_baseline_defaults(female?"vroid-beta-female":"vroid-beta-male"),{},path.parent_path().parent_path());size_t faces=0,linked_texels=0,zero_faces=0;Scene scene;bool first=true;
    for(auto &part:created.parts){auto accepted=PaintedMesh::from_maps(part.mesh,part.source_maps,256,source());faces+=accepted->mesh().indices.size()/3;linked_texels+=accepted->surface_diagnostics().linked_texels;zero_faces+=accepted->surface_diagnostics().unpaintable_uv_faces.size();
      check(accepted->mesh().indices==part.mesh.indices&&accepted->mesh().uv==part.mesh.uv&&accepted->source_maps()->color.image==part.source_maps->color.image,"Actual source part keeps all original authored triangles, UVs and immutable texture ownership");
      uint64_t id=scene.active().id;if(!first)id=scene.add(part.name,Volume{});first=false;auto &object=scene.object(id);object.painted=accepted;object.mesh_active=true;object.appearance=part.appearance;object.deformation=std::make_shared<const DeformationState>(part.deformation);object.material.color=part.color;object.material.toon=part.toon;object.material.roughness=part.roughness;
    }
    check(created.parts.size()==19&&zero_faces==28&&linked_texels>0,"All19 actual baseline parts accept authored UVs with28 zero-area faces explicitly retained");
    auto j=scene.encode();auto reload=Scene::decode(nlohmann::json::from_cbor(nlohmann::json::to_cbor(j)));bool exact=reload.objects().size()==19;
    for(size_t i=0;i<reload.objects().size();++i){auto &a=scene.objects()[i],&b=reload.objects()[i];exact&=a.painted->mesh().indices==b.painted->mesh().indices&&a.painted->mesh().uv==b.painted->mesh().uv&&a.appearance->source_vertices==b.appearance->source_vertices&&a.deformation->encode(a.painted->mesh())==b.deformation->encode(b.painted->mesh());}
    check(exact&&j.at("source_models").size()==1&&j.at("source_images").size()==25,"Actual grouped source-compatible Scene restart retains all parts and one shared original model/image cache");
    std::cout<<"BASELINE "<<argv[arg]<<" parts="<<created.parts.size()<<" triangles="<<faces<<" unpaintable_uv_faces="<<zero_faces<<" linked_texels="<<linked_texels<<'\n';
  }
  std::cout<<"Native source-authored UV tests complete\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
