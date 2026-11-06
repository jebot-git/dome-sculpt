// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_CANONICAL_OWNING_HELPERS_ONLY
#include "canonical-normal-owning.cpp"
#include "polygon-surface-paint.hpp"
#include "polygon-surface-preview.hpp"
#include "polygon-conversion.hpp"
#include "dome/paint-sampling.hpp"
using namespace dome::detail;
namespace {
size_t native_checks=0;
void expect(bool b,const char*s){if(!b)throw std::runtime_error(s);++native_checks;std::cout<<"PASS "<<s<<std::endl;}
void polygon_ready(Document&d){expect(d.wait_polygon(std::chrono::seconds(120)),"Actual polygon queue reaches endpoint");if(!d.frame().state->error.empty())throw std::runtime_error(d.frame().state->error);}
Vec3 world_point(const Scene&s,Vec3 p){const auto v=s.world_matrix(s.active().id).point({p.x,p.y,p.z});return {float(v[0]),float(v[1]),float(v[2])};}
void float_stroke(Document&d){
 d.paint_command(SpatialCommand::AddLayer);settled(d);const auto snap=d.frame().surface;expect(snap&&!snap->ranges.empty(),"Actual native current-field paint target is published");
 const auto&r=snap->ranges.front();const auto a=snap->mesh.positions.at(snap->mesh.indices.at(r.first)),b=snap->mesh.positions.at(snap->mesh.indices.at(r.first+1)),c=snap->mesh.positions.at(snap->mesh.indices.at(r.first+2));
 const auto p=a*.3f+b*.3f+c*.4f,n=normalized(cross(b-a,c-a));PaintSettings ink;ink.front_only=false;ink.channel=PaintChannel::Emission;ink.emission_units=PaintEmissionUnits::Physical;ink.emission={12,.002f,3};d.paint_settings(ink);
 NativeStrokeSample sample;sample.origin=p+n;sample.direction=n*-1;sample.brush.radius=.2f;sample.brush.strength=.73f;sample.brush.blend=.2f;
 expect(d.begin_native_stroke()&&d.sample_native_stroke(sample),"Actual native gesture hits the genuine current writable grid");d.end_native_stroke();expect(d.wait_native_stroke(std::chrono::seconds(120)),"Native physical gesture finishes");settled(d);expect(d.copy_scene().active().painted->has_physical_emission(),"Real FLOAT emission metadata is present");
}
Scene source(bool canonical,bool reflected){
 Scene s;if(canonical)s=mirror_conversion_model();else{auto m=mirrored_fan();m.indices={0,1,2,0,2,3};m.positions.resize(4);m.normals.resize(4);m.uv.resize(4);m.uv_pins.resize(4);s.active().mesh_active=true;s.active().painted=std::make_shared<PaintedMesh>(m,64);}
 auto a=AffineTransform{}.matrix();a[0]=reflected?-1.2:1.2;a[1]=.1;a[4]=.3;a[5]=.8;a[9]=.15;a[10]=1.1;a[12]=.02;a[14]=.1;s.active().affine_edit=std::make_shared<const NativeAffineEdit>(AffineTransform(a));s.validate();return s;
}
void drag(Document&d,bool cancel){
 const auto scene=d.copy_scene();const auto topo=d.frame().state->polygon->topology;const auto mesh=topo->triangulate().mesh;const auto c=mesh.indices.front();const auto p=world_point(scene,mesh.positions[c]*.3f+mesh.positions[mesh.indices[1]]*.3f+mesh.positions[mesh.indices[2]]*.4f);const auto matrix=scene.world_matrix(scene.active().id);auto v=matrix.normal({mesh.normals[c].x,mesh.normals[c].y,mesh.normals[c].z});Vec3 n=normalized(Vec3{float(v[0]),float(v[1]),float(v[2])});
 d.polygon_pointer(p+n,n*-1,PolygonAction::BeginDrag);polygon_ready(d);expect(d.frame().state->polygon->moving,"Real affine ray opens polygon move gesture");const Vec3 delta{.006f,.003f,0};d.polygon_pointer(p+n+delta,n*-1,PolygonAction::Drag);polygon_ready(d);d.polygon_pointer(p+n+delta,n*-1,cancel?PolygonAction::CancelDrag:PolygonAction::EndDrag);polygon_ready(d);
}
void owning_negatives(const nlohmann::json&wire){
 expect(wire.at("version")==39&&wire.at("source_materials").at("version")==21&&wire.at("objects").at(0).at("painted").at("version")==6,"Actual field publication emits Scene39/material21/Paint6");
 auto bad=wire;bad["version"]=37;expect(owning_failure([&]{Scene::decode(bad);},"Scene39"),"Older owning context cannot borrow current polygon field capability");
 bad=wire;bad["polygon_surfaces"]["nodes"][0]["before_origin"]=bad["polygon_sources"]["origins"].size();expect(owning_failure([&]{Scene::decode(bad);},"reference"),"Out-of-range actual before-root reference refuses before replay");
 bad=wire;bad["source_materials"]["polygon_surface_transfer"]=false;expect(owning_failure([&]{Scene::decode(bad);},"declared capabilities"),"Version alone does not grant polygon field semantics");
 bad=wire;size_t field_binding=0;while(bad["source_materials"]["bindings"][field_binding]["polygon_surface_transfer"].is_null())++field_binding;const auto root=bad["polygon_surfaces"]["nodes"][0]["before_origin"].get<size_t>();bad["polygon_sources"]["origins"][root]["material_bindings"]=field_binding;expect(owning_failure([&]{Scene::decode(bad);},"incomplete/future material prefix"),"Cross-pool owning cycle refuses before allocating/replaying current paint");
 PaintImageArchive images;expect(owning_failure([&]{PaintedMesh::decode(wire.at("objects").at(0).at("painted"),&images);},"owning Scene39 reader"),"Standalone Paint6 cannot invent a Scene/material source-root authority");
}
void before_only(const Scene&current){
 Scene carrier;auto authority=std::make_shared<PolygonAuthority>();authority->origin=capture_polygon_origin(current,current.active().id);carrier.active().polygon=authority;const auto wire=carrier.encode();expect(wire.at("version")==39&&!carrier.active().painted&&!carrier.active().material_bindings,"Actual before-only field controls determine the owning feature without a fabricated current actor");const auto decoded=Scene::decode(wire);expect(decoded.encode()==wire,"Before-only Scene39 source/control graph has an exact owning cycle");const auto&root=decoded.active().polygon->origin;const auto&field=root->material_bindings->polygon_surface_transfer;expect(field&&field->matches_current(*root->paint)&&root->polygon->origin==field->before(),"Before-only field restores actual current paint attachment and original-source alias");
 auto invalid=std::make_shared<PolygonOrigin>(*authority->origin);auto wrong=std::make_shared<PolygonAuthority>(*invalid->polygon);wrong->origin.reset();invalid->polygon=wrong;auto bad=carrier;auto shell=std::make_shared<PolygonAuthority>();shell->origin=invalid;bad.active().polygon=shell;expect(owning_failure([&]{bad.validate();},"complete source"),"Retained field cannot conceal mismatched current polygon/source ownership");
}
void run(bool canonical,bool reflected,bool flip,const std::filesystem::path&out){
 Document d;d.replace(source(canonical,reflected));d.shading(false);settled(d);
 if(canonical){d.convert_baked_normals();settled(d);d.edit_scene("Actual current morph evaluation",[](Scene&s,const auto&){auto df=std::make_shared<DeformationState>(*s.active().deformation);df->morph_weights.assign(df->morphs->targets.size(),.35f);s.active().deformation=df;});settled(d);}
 float_stroke(d);
 if(canonical){ShapeKeyEdit key;key.action=ShapeKeyAction::Create;key.name="Before field polygon key";d.author_shape(key);settled(d);key.action=ShapeKeyAction::Replace;key.target=d.frame().state->morph_target;key.positions=d.copy_scene().active().painted->mesh().positions;key.positions[1].z+=.003f;d.author_shape(key);settled(d);d.begin_uv();expect(d.wait_uv(),"Actual canonical UV workspace prepares");d.edit_uv(UvAction::Scale,.8f);expect(d.wait_uv(),"Actual required canonical paint/normal retention scales");d.end_uv(true);settled(d);expect(bool(d.copy_scene().active().material_bindings->canonical_paint_sampling),"Real before graph contains owning37, current key and FLOAT paint");}
 const auto before=d.copy_scene().encode();const auto rev=d.frame().state->revision;
 d.begin_polygon(true);polygon_ready(d);d.end_polygon(true);settled(d);expect(d.copy_scene().encode()==before&&d.frame().state->revision==rev,"Untouched field Accept is an exact source/history no-op");
 d.begin_polygon(true);polygon_ready(d);drag(d,true);d.end_polygon(false);settled(d);expect(d.copy_scene().encode()==before,"Actual affine gesture Cancel preserves full key/paint/normal/rig source");
 d.begin_polygon(true);polygon_ready(d);if(flip){d.edit_polygon(PolygonAction::SelectAll);polygon_ready(d);d.edit_polygon(PolygonAction::FlipFaces);polygon_ready(d);}else drag(d,false);
 d.end_polygon(true);const auto file=out/"accepted.dsculpt";d.save_async(file).get();settled(d);const auto accepted=d.copy_scene();const auto wire=accepted.encode();owning_negatives(wire);
 expect(Scene::load(file).encode()==wire&&Scene::decode(wire).encode()==wire,"Immediately queued Save and owning second-cycle replay preserve complete current graph");
 const auto field=accepted.active().material_bindings->polygon_surface_transfer;expect(field&&accepted.active().polygon->origin==field->before()&&field->matches_current(*accepted.active().painted),"Current polygon, writable Paint and original field use genuine paired owners");
 expect(field->before()->paint->has_physical_emission()&&(!canonical||field->before()->material_bindings->canonical_paint_sampling),"Full before-state retains actual FLOAT and independent37 normal/key owners");
 const auto&range=d.frame().surface->ranges.front();expect(range.textures->polygon_surface&&range.textures->polygon_surface->matches(range.material_bindings,range.paint_sampling_attachment),"Native frame publishes the actual current-field packet, not legacy normal authority");
 before_only(accepted);
 expect(d.undo()&&d.copy_scene().encode()==before&&d.redo()&&d.copy_scene().encode()==wire,"Real global Undo/Redo retains exact before/current owning nodes");
 Document reopen;reopen.replace(Scene::load(file));settled(reopen);reopen.restore_polygon_source();settled(reopen);expect(reopen.copy_scene().encode()==before,"Restarted source Restore recovers exact placement, FLOAT, key, normal and first source graph");expect(reopen.undo()&&reopen.copy_scene().encode()==wire&&reopen.redo()&&reopen.copy_scene().encode()==before,"Restarted Restore has exact native Undo/Redo");
 expect(d.copy_scene().encode()==wire,"Decode/negative/Restore controls leave the independent current Document unchanged");
}
}
int main(int argc,char**argv)try{if(argc!=2)return 2;const std::filesystem::path out=argv[1];for(bool c:{false,true})for(bool r:{false,true})for(bool f:{false,true}){const auto p=out/(std::to_string(c)+std::to_string(r)+std::to_string(f));std::filesystem::create_directories(p);run(c,r,f,p);}std::cout<<"PASS "<<native_checks<<" actual polygon field owning/publication/history controls\n";return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
