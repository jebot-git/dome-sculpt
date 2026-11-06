// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/material-visibility.hpp"
#include "dome/polygon-source.hpp"
#include "dome/texture-sets.hpp"
#include "polygon-conversion.hpp"
#include "polygon-surface-owner.hpp"
#include "polygon-surface-preview.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
using namespace dome;
namespace {
size_t checks=0;
void check(bool value,const char*message){if(!value)throw std::runtime_error(message);++checks;std::cout<<"PASS "<<message<<'\n';}
template<class F>void refuses(F&&f,const char*message){bool caught=false;try{f();}catch(const std::exception&e){caught=true;std::cout<<"EXPECTED "<<e.what()<<'\n';}check(caught,message);}
void ready(Document&d){check(d.wait_asset(std::chrono::seconds(120))&&d.wait_mesh(std::chrono::seconds(120)),"Actual asset and mesh workers reach a complete endpoint");if(!d.frame().state->error.empty())throw std::runtime_error(d.frame().state->error);}
void polygon_ready(Document&d){check(d.wait_polygon(std::chrono::seconds(120)),"Actual polygon worker reaches its queue boundary");if(!d.frame().state->error.empty())throw std::runtime_error(d.frame().state->error);}
Scene source(){
 Mesh m;m.positions={{-.5f,-.5f,0},{.5f,-.5f,0},{.5f,.5f,0},{-.5f,.5f,0}};m.normals.assign(4,{0,0,1});m.uv={{{.1f,.1f}},{{.9f,.1f}},{{.9f,.9f}},{{.1f,.9f}}};m.indices={0,1,2,0,2,3};
 Scene s;auto&o=s.active();o.mesh_active=true;o.material.double_sided=true;o.transform.position.x=-2;o.painted=std::make_shared<PaintedMesh>(m,64);
 PaintSettings ink;ink.channel=PaintChannel::Emission;ink.emission_units=PaintEmissionUnits::Physical;ink.emission={12,.002f,3};ink.front_only=false;
 Brush brush;brush.from=brush.to={0,0,0};brush.radius=2;brush.strength=1;brush.pressure=1;brush.normal={0,0,1};o.painted->begin_stroke();o.painted->stamp(brush,ink);check(o.painted->end_stroke()&&o.painted->has_physical_emission(),"The real initial source contains authored FLOAT paint, not a fabricated field marker");s.validate();return s;
}
uint64_t add_sets(Scene&s){
 const auto id=s.add("Independent material-set actor",Volume{});auto&o=s.object(id);o.transform.position.x=2;
 Mesh m;m.positions={{-.5f,-.5f,0},{.5f,-.5f,0},{0,.5f,0},{-.5f,-.5f,.5f},{.5f,-.5f,.5f},{0,.5f,.5f}};m.normals.assign(6,{0,0,1});m.uv={{{.1f,.1f}},{{.9f,.1f}},{{.5f,.9f}},{{.1f,.1f}},{{.9f,.1f}},{{.5f,.9f}}};m.indices={0,1,2,3,4,5};
 std::array<TextureSetInitialField,2>fields;fields[0].resolution=64;fields[1].resolution=128;for(auto&f:fields)f.material.double_sided=true;fields[0].material.name="Actual back slot";fields[1].material.name="Actual front slot";
 initialize_texture_sets(s,id,m,fields,std::array<TextureSetId,2>{0,1});select_texture_set(s,id,1);return id;
}
void owners(const Scene&s,uint64_t field,uint64_t sets){
 const auto&o=s.object(field);check(bool(o.material_bindings),"Mixed actor retains its actual material-binding owner");const auto&owner=o.material_bindings->polygon_surface_transfer;check(owner&&owner->matches_current(*o.painted)&&o.polygon->origin==owner->before(),"Mixed Scene40 retains the real39 current Paint, material field and exact before-root alias");
 check(owner->before()->paint->has_physical_emission(),"The nested39 before graph retains actual FLOAT paint planes");const auto&multi=s.object(sets);check(multi.texture_sets&&multi.texture_sets->active()==1&&multi.painted->mesh_owner()==multi.texture_sets->at(1).painted->mesh_owner(),"Independent40 fields retain genuine object/set identity and one actual geometry owner");
}
void mixed(){
 const auto original=source();const uint64_t field_id=original.active().id;Document d;d.replace(original);ready(d);d.begin_polygon(true);polygon_ready(d);d.edit_polygon(PolygonAction::SelectAll);polygon_ready(d);d.edit_polygon(PolygonAction::FlipFaces);polygon_ready(d);d.end_polygon(true);ready(d);
 auto scene=d.copy_scene();check(scene.encode().at("version")==39&&scene.active().material_bindings&&scene.active().material_bindings->polygon_surface_transfer,"Real accepted native Flip produces the genuine39 owning field");const auto set_id=add_sets(scene);scene.select(field_id);const auto wire=scene.encode();
 check(wire.at("version")==40&&wire.contains("polygon_surfaces")&&wire.contains("texture_set_geometry"),"One mixed outer Scene40 retains both actual feature pools");
 check(wire.at("objects")[0].at("painted").at("version")==6&&!wire.at("objects")[1].at("texture_sets").is_null(),"Separate actors keep genuine Paint6 and owning Paint7 collection representations");
 const auto first=Scene::decode(wire),second=Scene::decode(first.encode());check(first.encode()==wire&&second.encode()==wire,"Mixed39/40 source, pixels, aliases and material identities survive two actual owning cycles");owners(second,field_id,set_id);
 scene.save("mixed.dsculpt");auto reopened=Scene::load("mixed.dsculpt");check(reopened.encode()==wire,"Actual native Save/reopen preserves the complete mixed owning graph");
 Document live;live.replace(reopened);ready(live);const auto frame=live.frame();check(frame.surface->ranges.size()==3&&frame.surface->picking,"Actual native mixed worker publishes one field39 range and two independent40 ranges");
 bool field_range=false;size_t set_ranges=0;
 for(const auto&r:frame.surface->ranges){
  if(r.object==field_id){check(r.textures&&r.textures->polygon_surface&&r.textures->polygon_surface->matches(r.material_bindings,r.paint_sampling_attachment),"Mixed native range retains the actual39 material/attachment/preview control blocks");field_range=true;}else if(r.object==set_id){check(bool(r.texture_set_proof),"Each40 range retains its real selected-field range authority");++set_ranges;}
  const auto&a=frame.surface->mesh.positions.at(frame.surface->mesh.indices.at(r.first)),&b=frame.surface->mesh.positions.at(frame.surface->mesh.indices.at(r.first+1)),&c=frame.surface->mesh.positions.at(frame.surface->mesh.indices.at(r.first+2));const auto p=a*.2f+b*.3f+c*.5f,n=normalized(cross(b-a,c-a));const auto hit=frame.surface->picking->raycast(p+n*.2f,n*-1,.4f,0,r.object);
  check(hit&&frame.surface->picking->material_identity(hit->triangle)==SurfaceMaterialIdentity{r.object,r.texture_set},"Mixed actual ray preserves object/set material identity and logical range ownership");
 }
 check(field_range&&set_ranges==2,"Mixed publication neither drops39 nor collapses40 onto the primary material");
 auto nested=wire;const auto&genuine_field=wire.at("objects")[0].at("painted");nested["objects"][1]["texture_sets"]["additional"][0]["painted"]["payload"]=genuine_field;
 refuses([&]{Scene::decode(nested);},"A real39 Paint6 nested under Paint7 cannot borrow a different object's material/source authority");
 nested=wire;nested["objects"][1]["texture_sets"]["additional"][0]["material_bindings"]=wire.at("objects")[0].at("material_bindings");
 refuses([&]{Scene::decode(nested);},"An injected nested material edge cannot bypass genuine per-set source ownership or graph walks");
 nested=wire;nested["texture_set_geometry"]["topologies"][0]["mesh"]=nested["texture_set_geometry"]["meshes"].size();
 refuses([&]{Scene::decode(nested);},"Outer40 version and real39 nodes cannot authorize a foreign actual texture-set geometry reference");
 nested=wire;for(auto&object:nested["objects"])object["texture_sets"]=nullptr;
 refuses([&]{Scene::decode(nested);},"Larger outer40 number with an orphan geometry table cannot grant mixed-field capability");
 auto bad=wire;bad["version"]=39;refuses([&]{Scene::decode(bad);},"Outer39 cannot conceal an actual40 material collection");bad=wire;bad.erase("polygon_surfaces");refuses([&]{Scene::decode(bad);},"Outer40 cannot discard the actual39 owning recipe pool");
 bad=wire;bad["polygon_surfaces"]["nodes"][0]["before_origin"]=bad["polygon_sources"]["origins"].size();refuses([&]{Scene::decode(bad);},"Mixed40 cannot borrow an out-of-range39 before owner");bad=wire;bad["objects"][0]["painted"]["node"]=bad["polygon_surfaces"]["nodes"].size();refuses([&]{Scene::decode(bad);},"Mixed40 Paint6 remains paired to its own actual material field node");
 auto before_only=scene.snapshot();auto authority=std::make_shared<PolygonAuthority>();authority->origin=capture_polygon_origin(scene,field_id);auto&shell=before_only.object(field_id);shell.mesh_active=false;shell.painted.reset();shell.material_bindings.reset();shell.deformation.reset();shell.appearance.reset();shell.retopology.reset();shell.shape_source.reset();shell.rig_source.reset();shell.bake_recipe.reset();shell.polygon=authority;
 const auto retained=before_only.encode();check(retained.at("version")==40&&retained.contains("polygon_surfaces")&&Scene::decode(retained).encode()==retained,"Before-only39 source nodes remain genuine and reachable beside an actual40 actor");
 live.restore_polygon_source();ready(live);auto expected=original.snapshot();check(add_sets(expected)==set_id,"Expected Restore retains the same actual sibling object identity");expected.select(field_id);check(live.copy_scene().encode()==expected.encode(),"Restarted actual source Restore recovers full before39 data without changing sibling40 fields");check(live.undo()&&live.copy_scene().encode()==wire&&live.redo()&&live.copy_scene().encode()==expected.encode(),"Mixed actual Restore has exact global Undo/Redo across both features");
 check(scene.encode()==wire,"All mixed decode/negative/native/Restore controls retain the independent source Scene");
}
}
int main(int argc,char**argv)try{if(argc>1){std::filesystem::create_directories(argv[1]);std::filesystem::current_path(argv[1]);}mixed();std::cout<<"PASS "<<checks<<" genuine mixed polygon39/texture-set40 controls\n";return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
