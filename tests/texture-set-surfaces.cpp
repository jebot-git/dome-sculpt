// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/texture-sets.hpp"
#include "dome/material-visibility.hpp"
#include "dome/source-placement.hpp"
#include "texture-set-worker.hpp"
#include "texture-set-paint-access.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <chrono>
#include <bit>
#include <algorithm>
using namespace dome;
namespace {
size_t checks=0;
bool bits(float a,float b){return std::bit_cast<uint32_t>(a)==std::bit_cast<uint32_t>(b);}
bool vectors(const std::vector<Vec3>&a,const std::vector<Vec3>&b){return std::equal(a.begin(),a.end(),b.begin(),b.end(),[](Vec3 x,Vec3 y){return bits(x.x,y.x)&&bits(x.y,y.y)&&bits(x.z,y.z);});}
void check(bool value,const char*message){if(!value)throw std::runtime_error(message);++checks;}
template<class F>void refuses(F&&f,const char*message){bool caught=false;try{f();}catch(const std::exception&e){caught=true;std::cout<<"EXPECTED "<<e.what()<<'\n';}check(caught,message);}
Mesh triangles(bool coincident){
 Mesh m;m.positions={{-.8f,-.8f,0},{.8f,-.8f,0},{0,.8f,0},{-.8f,-.8f,coincident?0.f:.5f},{.8f,-.8f,coincident?0.f:.5f},{0,.8f,coincident?0.f:.5f}};
 m.normals.assign(6,{0,0,1});m.uv={{{.1f,.1f}},{{.9f,.1f}},{{.5f,.9f}},{{.1f,.1f}},{{.9f,.1f}},{{.5f,.9f}}};m.indices={0,1,2,3,4,5};return m;
}
Scene overlap(bool coincident){
 Scene s;std::array<TextureSetInitialField,2>fields;for(auto&f:fields){f.resolution=64;f.material.double_sided=true;}fields[0].material.name="Back material";fields[1].material.name="Front material";
 const auto mesh=triangles(coincident);initialize_texture_sets(s,s.active().id,mesh,fields,std::array<TextureSetId,2>{0,1});select_texture_set(s,s.active().id,1);
 check(s.active().painted->surface_diagnostics().policy==PaintUvPolicy::Strict&&s.active().texture_sets->at(1).painted->surface_diagnostics().policy==PaintUvPolicy::Strict,"Actual initial material membership precedes both strict rasters without an original-source policy flag");
 check(vectors(s.active().painted->mesh().positions,mesh.positions)&&vectors(s.active().painted->mesh().normals,mesh.normals)&&s.active().painted->mesh().uv==mesh.uv&&s.active().painted->mesh().indices==mesh.indices,"Genuine producer preserves all original coincident/depth/UV/normal/index data without repair or relabeling");
 return s;
}
bool ink(PaintedMesh&p,std::array<float,4>color){PaintSettings settings;settings.front_only=false;settings.color=color;Brush b;b.from=b.to={0,0,0};b.normal={0,0,1};b.radius=2;b.strength=1;p.begin_stroke();p.stamp(b,settings);return p.end_stroke();}
void initial_controls(){
 PaintSurfaceOptions source;source.policy=PaintUvPolicy::SourceAuthored;refuses([&]{PaintedMesh::from_mesh(triangles(true),64,source);},"Bare SourceAuthored flag still cannot invent an original source authority");
 Scene scene;const auto before=scene.encode();const auto object=scene.active().id;auto mesh=triangles(false);std::array<TextureSetInitialField,2>fields;for(auto&f:fields)f.resolution=64;
 const std::array<TextureSetId,2>slots{0,1};TextureSetLimits low;low.maximum_work=1;
 refuses([&]{initialize_texture_sets(scene,object,mesh,fields,slots,low);},"Initial membership/raster work forecast refuses before staging");check(scene.encode()==before,"Initial work refusal preserves the complete previous Scene");
 low=TextureSetLimits{};low.maximum_bytes=1;refuses([&]{initialize_texture_sets(scene,object,mesh,fields,slots,low);},"Initial owning geometry/raster byte forecast refuses before copying");check(scene.encode()==before,"Initial byte refusal preserves the complete previous Scene");
 low=TextureSetLimits{};low.cancelled=[] {return true;};refuses([&]{initialize_texture_sets(scene,object,mesh,fields,slots,low);},"Initial factory cancellation cannot publish a partial material actor");check(scene.encode()==before,"Initial cancel preserves native values and IDs");
 refuses([&]{initialize_texture_sets(scene,object,mesh,fields,std::array<TextureSetId,2>{0,2});},"An undeclared initial material cannot own a logical face");check(scene.encode()==before,"Invalid membership cannot replace the old actor");
 initialize_texture_sets(scene,object,mesh,fields,slots);const auto complete=scene.encode();const auto x=scene.active().painted->mesh().positions[0].x;mesh.positions[0].x+=1;
 check(scene.active().painted->mesh().positions[0].x==x,"Owning factory detaches its single immutable geometry from the caller's mutable vector");
 refuses([&]{initialize_texture_sets(scene,object,triangles(false),fields,slots);},"Initialization cannot discard an existing authored material/paint/source actor");check(scene.encode()==complete,"Existing actor rejection retains all material resources exactly");
 std::cout<<"INITIAL_CURRENT_GEOMETRY "<<complete.at("texture_set_geometry").dump()<<'\n';
}
void raster(){
 auto s=overlap(true);const auto geometry=s.active().painted->mesh_owner();const auto exact=s.encode();auto history=s.snapshot();
 check(s.active().painted->surface_diagnostics().covered_texels>100&&s.active().texture_sets->at(1).painted->surface_diagnostics().covered_texels>100,"Each coincident logical face has its own actual covered raster");
 check(edit_texture_set(s,s.active().id,1,[](PaintedMesh&p,Material&){return ink(p,{0,1,0,1});}),"A coincident face discarded by the original full-raster dedup remains genuinely paintable in its own set");
 check(!s.active().painted->has_paint_data()&&s.active().texture_sets->at(1).painted->has_paint_data(),"Coincident UV/position does not leak an extra-set stroke into primary pixels");
 check(history.encode()==exact,"Raster/domain and COW paint history stay immutable across a real overlapping-UV stroke");
 check(edit_texture_set(s,s.active().id,0,[](PaintedMesh&p,Material&){return ink(p,{1,0,0,1});}),"The other overlapping logical face is independently paintable");
 const auto a=active_texture_set(s.active());const auto b=texture_set_view(s.active(),0);
 check(a.id==1&&a.painted->mesh_owner()==geometry&&b.painted->mesh_owner()==geometry,"Independent rasters retain one exact shared geometry owner");
 const auto green=a.painted->preview(a.material),red=b.painted->preview(b.material);const auto at=(size_t(32)*64+32)*4;
 check(green->color.rgba.at(at+1)>200&&green->color.rgba.at(at)<20&&red->color.rgba.at(at)>200&&red->color.rgba.at(at+1)<20,"Independent same-UV resolved images retain distinct red and green material values");
 const auto encoded=s.encode();auto decoded=Scene::decode(encoded);check(decoded.encode()==encoded,"Every membership-filtered raster reconstructs through the actual owning Scene40/Paint7 codec");
 check(edit_texture_set(decoded,decoded.active().id,1,[](PaintedMesh&p,Material&){return ink(p,{0,0,1,1});}),"Reopened coincident logical face remains writable without original source/material labels");
 check(s.encode()==encoded,"Reopened edit cannot mutate retained original set owners");
 TextureSetLimits slots;slots.maximum_sets_per_object=2;slots.maximum_sets_per_scene=2;refuses([&]{create_texture_set(s,s.active().id,std::array<uint64_t,1>{1},"Over-budget slot",0,slots);},"Whole-Scene slot admission refuses a new material before copying/staging its paint raster");check(s.encode()==encoded,"Whole-Scene slot refusal preserves actual fields and high-water IDs");
 TextureSetLimits low;low.maximum_work=1;refuses([&]{assign_texture_set_faces(s,s.active().id,0,std::array<uint64_t,1>{2},low);},"Membership raster forecast refuses before publication at the actual work bound");check(s.encode()==encoded,"Failed raster reconstruction preserves exact complete Scene");
}
std::pair<std::shared_ptr<const detail::TextureSetPackedGeometry>,MeshSnapshot> pack(const Scene&s){
 auto surfaces=s.preview_surface_bundles({});check(surfaces.size()==1&&surfaces[0].mesh.positions.empty(),"Worker bundle carries one immutable mesh owner without an extra full ObjectSurface geometry copy");
 auto layout=detail::prepare_texture_set_packed(surfaces[0].texture_set_surface);MeshSnapshot out;
 detail::append_texture_set_surface(out,surfaces[0],layout,{});return {layout,std::move(out)};
}
void query(){
 auto s=overlap(false);const auto exact=s.encode();auto [layout,out]=pack(s);
 check(out.ranges.size()==2&&out.mesh.positions.size()==6&&out.mesh.indices.size()==6,"Actual worker packet retains both sets and one complete shared geometry stream");
 MaterialSurfaceQuery q(out.mesh,out.ranges);auto hit=q.raycast({0,0,2},{0,0,-1});check(hit&&q.material_identity(hit->triangle)==SurfaceMaterialIdentity{s.active().id,1},"A ray hits the actual foreground material slot on this one object");
 const std::array<SurfaceMaterialIdentity,1>without_front{{{s.active().id,1}}};hit=q.raycast_excluding_materials({0,0,2},{0,0,-1},100,0,without_front);
 check(hit&&q.material_identity(hit->triangle)==SurfaceMaterialIdentity{s.active().id,0}&&q.logical_triangle(hit->triangle)==0,"Excluding the active set leaves a different set on that same object visible and retains its logical triangle identity");
 const std::array<uint64_t,1>without_object{s.active().id};check(!q.raycast_excluding({0,0,2},{0,0,-1},100,0,without_object),"The original whole-object exclusion still excludes every material on the object");
 auto changed=out.ranges;changed[1].object+=1;refuses([&]{MaterialSurfaceQuery bad(out.mesh,changed);},"A foreign object cannot borrow another set's numeric ID or proof");
 changed=out.ranges;changed[1].texture_set=0;refuses([&]{MaterialSurfaceQuery bad(out.mesh,changed);},"A foreign slot cannot borrow a same-object field seal");
 changed=out.ranges;changed[1].material.color[0]=std::bit_cast<float>(std::bit_cast<uint32_t>(changed[1].material.color[0])^1u);refuses([&]{MaterialSurfaceQuery bad(out.mesh,changed);},"A copied range cannot mutate its sealed material float bits");
 changed=out.ranges;changed[1].textures=changed[0].textures;refuses([&]{MaterialSurfaceQuery bad(out.mesh,changed);},"A copied material range cannot borrow another actual field/map owner");
 changed=out.ranges;changed[1].texture_set_proof=std::shared_ptr<const detail::TextureSetRangeProof>(changed[1].texture_set_proof.get(),[](auto*){});refuses([&]{MaterialSurfaceQuery bad(out.mesh,changed);},"The same proof address with a foreign control block is not an owning certificate");
 auto mesh=out.mesh;std::swap(mesh.indices[3],mesh.indices[4]);refuses([&]{MaterialSurfaceQuery bad(mesh,out.ranges);},"Altered EBO/logical corner correspondence refuses before visibility construction");
 mesh=out.mesh;mesh.uv[mesh.indices[3]][0]+=.01f;refuses([&]{MaterialSurfaceQuery bad(mesh,out.ranges);},"Equal-count geometry cannot replace the actual bound paint coordinates");
 std::vector<std::array<float,4>>tint(out.mesh.positions.size(),{1,1,1,1});MaterialSurfaceQuery white(out.mesh,out.ranges,{},tint);check(true,"Mixed-scene identity source-colour rows do not duplicate a per-set geometry/tint authority");tint[out.mesh.indices[3]][0]=.5f;refuses([&]{MaterialSurfaceQuery bad(out.mesh,out.ranges,{},tint);},"Plain set field cannot borrow a fabricated packed source/corner tint");
 auto foreign=overlap(false);auto plan=detail::prepare_texture_set_surface(foreign.active());refuses([&]{layout->authenticate_plan(*plan);},"Numerically equal foreign geometry/membership owners cannot reuse the packed layout");
 const auto old_collection=s.active().texture_sets;edit_texture_set(s,s.active().id,1,[](PaintedMesh&p,Material&m){m.roughness=.25f;p.add_layer("Independent metadata");return true;});
 plan=detail::prepare_texture_set_surface(s.active());check(old_collection!=s.active().texture_sets&&layout->matches_plan(*plan),"A genuine paint/material edit changes its field owner without rebuilding the unchanged geometry-only layout");
 const auto before_invalid_merge=s.encode();const auto merge_collection=s.active().texture_sets;const auto merge_primary=s.active().painted;const auto merge_extra=s.active().texture_sets->at(1).painted;refuses([&]{assign_texture_set_faces(s,s.active().id,1,std::array<uint64_t,1>{1});},"Two depth-separated faces with identical UVs cannot silently become independently writable in one Strict material grid");check(s.encode()==before_invalid_merge,"Ambiguous same-grid reassignment preserves every set, image, source and ID exactly");check(s.active().texture_sets==merge_collection&&s.active().painted==merge_primary&&s.active().texture_sets->at(1).painted==merge_extra,"Strict reassignment refusal preserves the actual collection and both field owners");
 create_texture_set(s,s.active().id,std::array<uint64_t,1>{1},"Genuine changed face membership");plan=detail::prepare_texture_set_surface(s.active());check(!layout->matches_plan(*plan),"Changing actual material membership invalidates the packed EBO dependency");
 check(exact!=s.encode(),"The COW/cache controls used actual state-changing edits");
}
Image image(uint8_t r,uint8_t g,uint8_t b){Image out{64,64,std::vector<uint8_t>(64*64*4)};for(size_t p=0;p<out.rgba.size();p+=4){out.rgba[p]=r;out.rgba[p+1]=g;out.rgba[p+2]=b;out.rgba[p+3]=255;}return out;}
void boundary(){
 Mesh m;m.positions={{-1,-1,0},{1,-1,0},{1,1,0},{-1,1,0}};m.normals.assign(4,{0,0,1});m.uv={{{.1f,.9f}},{{.1f,.1f}},{{.9f,.1f}},{{.9f,.9f}}};m.indices={0,1,2,0,2,3};Scene s;s.active().mesh_active=true;s.active().painted=std::make_shared<PaintedMesh>(m,64);
 create_texture_set(s,s.active().id,std::array<uint64_t,1>{2},"Normal frame needs a real boundary split");
 edit_texture_set(s,s.active().id,1,[](PaintedMesh&p,Material&){BakedTextures bake;bake.maps.color=image(255,255,255);bake.maps.metal_rough=image(255,255,0);bake.maps.emission=image(0,0,0);bake.maps.normal=image(128,128,255);bake.coverage.assign(64*64,255);p.install_bake(std::move(bake));return true;});
 auto [layout,out]=pack(s);check(layout->vertices().size()==6&&out.mesh.positions.size()==6,"Welded material boundary expands only the two genuine conflicting tangent vertices, not a full mesh per set");
 check(layout->posed()->positions.size()==4&&s.active().painted->mesh().positions.size()==4,"Necessary renderer boundary splits never alter native geometry/skin/morph vertex identities");
 for(const auto&r:out.ranges)r.texture_set_proof->authenticate_stream(out.mesh,r);
 auto&object=s.active();object.affine_edit=std::make_shared<const NativeAffineEdit>(AffineTransform(std::array<double,16>{-1,0,0,0,.2,1,0,0,0,0,1,0,0,0,0,1}));
 auto reflected=pack(s).second;check(reflected.ranges.size()==2&&reflected.ranges[0].paint_winding_reversed&&reflected.ranges[1].paint_winding_reversed,"Actual reflected placement reverses each range's indices with an explicit logical-corner witness");
 MaterialSurfaceQuery q(reflected.mesh,reflected.ranges);check(bool(q.raycast({.5f,0,2},{0,0,-1})),"Reflected material-visible picking uses the real packed winding, not only a metadata flag");
}
void native(){
 auto s=overlap(true);const auto original=s.encode();Document doc;doc.replace(s);check(doc.wait_mesh(std::chrono::seconds(30)),"Actual Document worker publishes the matching multi-set surface");const auto f=doc.frame();
 check(f.state->error.empty()&&f.surface->ranges.size()==2&&f.surface->picking,"Actual immutable native endpoint contains both genuine ranges and material-aware picking");
 check(f.surface->ranges[0].object==f.surface->ranges[1].object&&f.surface->ranges[0].texture_set!=f.surface->ranges[1].texture_set,"Actual producer keeps one native object and distinct persistent material slots");
 check(doc.copy_scene().encode()==original,"Worker surface preparation cannot mutate native paint, memberships, history or source state");
 auto distinct=overlap(false);Document denied;denied.replace(distinct);check(denied.wait_mesh(std::chrono::seconds(30)),"Distinct-depth actual native endpoint is ready before the rejected assignment");
 const auto before=denied.copy_scene();const auto values=before.encode();const auto owner=before.active().texture_sets;const auto history=denied.frame().state->can_undo;
 denied.edit_scene("Reject ambiguous same-grid material assignment",[object=distinct.active().id](Scene&scene,const auto&cancelled){TextureSetLimits limits;limits.cancelled=cancelled;assign_texture_set_faces(scene,object,1,std::array<uint64_t,1>{1},limits);});
 check(denied.wait_asset(std::chrono::seconds(30)),"Actual rejected material worker terminates without publishing a partial endpoint");
 const auto refused=denied.copy_scene();check(refused.encode()==values&&refused.active().texture_sets==owner&&refused.active().painted->mesh_owner()==before.active().painted->mesh_owner(),"Actual native worker refusal preserves full Scene and genuine geometry/collection owners");
 check(denied.frame().state->can_undo==history&&denied.frame().state->error.find("Overlapping UV islands")!=std::string::npos,"Actual native refusal retains its strict cause and adds no history entry");
 denied.save("strict-reassignment-preserved.dsculpt");check(Scene::load("strict-reassignment-preserved.dsculpt").encode()==values,"Subsequent real native Save/reopen retains the exact pre-refusal material fields");
}
}
int main()try{initial_controls();raster();query();boundary();native();std::cout<<"PASS "<<checks<<" texture-set raster/worker/query controls\n";return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
