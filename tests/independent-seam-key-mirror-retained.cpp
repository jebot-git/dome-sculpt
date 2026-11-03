// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_mirror_suite
#include "independent-seam-key-mirror.cpp"
#undef main
#include "dome/bake-authoring.hpp"
#include "dome/polygon-color.hpp"
#include "dome/polygon-scene.hpp"
#include "dome/normal-sampling.hpp"
#include "dome/paint-sampling.hpp"
#include <map>
namespace retained_mirror {
nlohmann::json paint_bytes(const PaintedMesh &paint) {
 PaintImageArchive images; NormalSamplingArchive normals; PaintSamplingArchive writable;
 auto result=paint.encode(&images,&normals,&writable);
 result["normals"]=normals.encode(images); result["writable"]=writable.encode(); result["images"]=images.encode();
 return result;
}
void endpoint(Document &d) {
 if(!d.wait_asset(std::chrono::seconds(120))||!d.wait_mesh(std::chrono::seconds(120))||!d.frame().state->error.empty())
  throw std::runtime_error("Retained Mirror endpoint: "+d.frame().state->error);
}
void oracle(const Scene &before,const Scene &after) {
 const auto &a=before.active(),&b=after.active(); const auto &mesh=a.painted->mesh();
 constexpr uint32_t opposite[]={0,2,1,9,11,10,6,8,7,3,5,4};
 check(mesh.indices.size()==12&&mesh.positions.size()==12&&b.painted->mesh().indices==mesh.indices,"This composition preserves its real original/current twelve-corner topology");
 const auto &from=a.deformation->morphs->targets[0].delta,&to=b.deformation->morphs->targets.back().delta;
 for(size_t c=0;c<12;++c)check(same(to[mesh.indices[c]],reflected(from[mesh.indices[opposite[c]]])),"Independent enumerated triangle-corner POSITION reflection");
 const auto old=a.material_bindings->normal_frames,now=b.material_bindings->normal_frames; const auto id=now->current_targets.back();
 for(auto pair:{std::pair{old->pbr,now->pbr},std::pair{old->mtoon,now->mtoon}}) {
  const auto &source=target(*pair.first,old->current_targets[0]),&result=target(*pair.second,id);
  for(size_t c=0;c<12;++c) {
   check(same(result.normals[c],reflected(source.normals[opposite[c]])),"Independent retained-role mirrored N endpoint");
   check(same(result.tangents[c],reflected(source.tangents[opposite[c]])*-1.f),"Independent retained-role UV parity and artist T endpoint");
  }
  check(pair.first->tangents==pair.second->tangents,"Retained Mirror preserves base tangent/handedness");
 }
}
Scene retained_source(){
 Mesh mesh;mesh.positions={{0,0,0},{-.5f,-.5f,0},{.5f,-.5f,0},{.5f,.5f,0},{-.5f,.5f,0}};mesh.normals.assign(5,{0,0,1});for(auto p:mesh.positions)mesh.uv.push_back({p.x+.5f,p.y+.5f});mesh.indices={0,1,2,0,2,3,0,3,4,0,4,1};auto authority=std::make_shared<PolygonAuthority>();authority->active=true;authority->topology=PolygonMesh::from_triangles(mesh);std::map<PolygonId,Vec3> positions;for(const auto&v:authority->topology.vertices())positions[v.id]=v.position;std::vector<PolygonCornerRgba> rows;for(const auto&f:authority->topology.faces())for(const auto&c:f.corners){const auto p=positions.at(c.vertex);rows.push_back({c.id,{.15f+.6f*(p.x+.5f),.2f+.5f*(p.y+.5f),.65f,1}});}authority->colors=std::make_shared<const PolygonColorField>(PolygonColorField::from_corners(authority->topology,rows));Scene s;auto&o=s.active();o.mesh_active=true;o.polygon=authority;o.painted=std::make_shared<PaintedMesh>(authority->topology.triangulate().mesh,64);o.material.color={1,1,1,1};o.material.unlit=false;DeformationState d(o.painted->mesh());d.skeleton=Skeleton{{{"root",-1,{},1},{"bend",0,{},1}}};d.skin=SkinWeights{};for(auto p:o.painted->mesh().positions){const float w=.2f+.6f*(p.x+.5f);d.skin->vertices.push_back({{0,w},{1,1-w}});}d.morphs=MorphSet(o.painted->mesh());s.set_deformation(o.id,d);s.validate();std::cout<<"INITIAL_VERTICES "<<o.painted->mesh().positions.size()<<std::endl;const auto target=o.id;auto high=o.painted->mesh();for(auto&p:high.positions){p.x=p.x*.7f-.35f;p.y*=2;p.z=.15f;}for(size_t v=0;v<high.normals.size();++v)high.normals[v]=normalized(Vec3{high.positions[v].x*.2f,high.positions[v].y*.15f,1});const auto id=s.add("Actual partial high source",Volume{});auto&h=s.object(id);h.mesh_active=true;h.visible=false;h.locked=true;h.painted=std::make_shared<PaintedMesh>(high,64);s.select(target);ObjectBakeSettings settings;settings.prepare_colors=settings.prepare_normals=true;settings.allow_partial=true;settings.front_mm=500;settings.back_mm=50;settings.ao_samples=1;settings.padding=2;const auto report=bake_object(s,id,settings);std::cout<<"BAKE_REPORT color="<<report.color_prepared<<" normal="<<report.normal_prepared<<" projected="<<report.projected<<" missed="<<report.missed<<" preserved_prior="<<report.preserved_misses<<std::endl;check(report.color_prepared&&report.normal_prepared&&report.projected&&report.missed,"Actual native corner field obtains partial colour/normal Bake and preserves uncovered gradient");ShapeKeyEdit first;first.name="Zero before retained paint";author_shapekey(s,target,first);return s;
}
}
int main(int argc,char **argv)try {
 if(argc!=2)return 2; const std::filesystem::path out=argv[1]; std::filesystem::create_directories(out);
 using namespace retained_mirror;
 auto source=retained_source(); auto pose=*source.active().deformation;
 pose.pose.resize(2);pose.pose[0].scale={1.2f,.8f,.7f};pose.pose[1].scale={.8f,1.1f,1.3f};pose.morph_weights={.35f};source.set_deformation(source.active().id,pose);
 const auto original=source.encode();source.save(out/"before-uv.dsculpt");
 Document doc;doc.replace(source);endpoint(doc);doc.begin_uv();check(doc.wait_uv(),"Actual UV workspace initializes native current-corner colour and normal owners");doc.edit_uv(UvAction::RetainPaint);
 for(const auto &uv:{std::array<float,2>{.25f,.25f},std::array<float,2>{.75f,.25f}}) {doc.edit_uv(UvAction::Pick,uv[0],uv[1]);doc.edit_uv(UvAction::Seam);check(doc.wait_uv(),"Actual symmetric radial seam worker completes");}
 doc.edit_uv(UvAction::Scale,.7f);check(doc.wait_uv()&&doc.frame().state->uv->valid,"Actual retained UV scale draft is valid");doc.end_uv(true);endpoint(doc);
 auto current=doc.copy_scene(); const auto retained=current.encode();doc.save(out/"retained.dsculpt");check(Scene::load(out/"retained.dsculpt").encode()==retained&&doc.undo()&&doc.copy_scene().encode()==original&&doc.redo()&&doc.copy_scene().encode()==retained,"Actual retained UV Save/reopen and one Undo/Redo own complete before-state");endpoint(doc);
 const auto &input=current.active();const auto paint=input.painted;const auto paint_data=paint_bytes(*paint);const auto event=paint->baked()->normal_authority;const auto projection=paint->normal_projection();
 check(paint->mesh().positions.size()==12&&paint->paint_sampling_domain()&&input.material_bindings->normal_preparation->sampling_domain->metadata_sampling_domain==paint->paint_sampling_domain()&&event,"Actual twelve-corner source owns independent writable domain and authenticated normal event");
 ShapeKeyEdit edit;edit.action=ShapeKeyAction::Replace;edit.target=0;edit.positions=paint->mesh().positions;edit.positions[0].z+=.045f;edit.positions[3].z-=.055f;doc.author_shape(edit);endpoint(doc);current=doc.copy_scene();const auto before=current.encode();current.save(out/"before-mirror.dsculpt");
 check(current.active().painted->mesh_owner()==paint->mesh_owner()&&paint_bytes(*current.active().painted)==paint_data&&current.active().painted->baked()->normal_authority==event,"Actual independent-corner Replace retains every Paint byte, event and exact geometry owner");
 ShapeKeyEdit mirror;mirror.action=ShapeKeyAction::Mirror;mirror.target=0;mirror.name="Actual retained dual-owner Mirror";doc.author_shape(mirror);endpoint(doc);auto after=doc.copy_scene();const auto encoded=after.encode();oracle(current,after);
 check(after.active().painted->mesh_owner()==paint->mesh_owner()&&after.active().painted->paint_sampling_attachment()==paint->paint_sampling_attachment()&&paint_bytes(*after.active().painted)==paint_data&&after.active().painted->baked()->normal_authority==event&&after.active().painted->normal_projection()==projection,"Mirror leaves all current Paint bytes, writable seal, intrinsic/resolved planes and normal event exact");
 auto old_rig=current.active().deformation->encode(current.active().painted->mesh()),new_rig=after.active().deformation->encode(after.active().painted->mesh());for(const char *key:{"morphs","morph_weights","expressions"}){old_rig.erase(key);new_rig.erase(key);}check(after.active().polygon==current.active().polygon&&old_rig==new_rig&&after.active().deformation->morph_weights.front()==current.active().deformation->morph_weights.front()&&after.active().deformation->morph_weights.back()==0,"Mirror keeps actual current corner authority, skin and saved nonuniform pose/mix");
 check(encoded.at("version")==24&&encoded.at("source_materials").at("version")==10&&encoded.at("source_materials").at("key_derivations").back().at("version")==3,"Writer composes actual Paint24 and owning key/Mirror capabilities");
 doc.save(out/"native-after.dsculpt");auto read=Scene::load(out/"native-after.dsculpt");check(read.encode()==encoded,"Actual accepted native Mirror Save/reopen authenticates every owner before-edge");
 check(read.active().material_bindings->color_preparation->current_descriptor()->field==read.active().polygon->colors&&read.active().material_bindings->normal_preparation->sampling_domain->metadata_sampling_domain==read.active().painted->paint_sampling_domain(),"Owning decode restores actual shared colour/geometry and independent normal metadata owners");
 check(doc.undo()&&doc.copy_scene().encode()==before&&doc.redo()&&doc.copy_scene().encode()==encoded,"Retained Mirror is one exact global Undo/Redo");endpoint(doc);
 ShapeKeyEdit restore;restore.action=ShapeKeyAction::Restore;author_shapekey(read,read.active().id,restore);check(read.encode()==before,"Restore recovers the entire retained before-Mirror state");
 auto foreign=Scene::decode(before);auto bad=current.snapshot();auto binding=std::make_shared<SourceMaterialBindings>(*bad.active().material_bindings);auto normal=std::make_shared<SourceNormalPreparation>(*binding->normal_preparation);auto sampling=std::make_shared<SourceNormalSampling>(*normal->sampling_domain);sampling->metadata_sampling_domain=foreign.active().painted->paint_sampling_domain();normal->sampling_domain=sampling;binding->normal_preparation=normal;bad.active().material_bindings=binding;const auto old_deform=bad.active().deformation;const auto old_shape=bad.active().shape_source;
 bool rejected=false;try{author_shapekey(bad,bad.active().id,mirror);}catch(const std::exception&e){std::cout<<"FOREIGN_OWNER_REFUSAL "<<e.what()<<std::endl;rejected=true;}
 check(rejected&&bad.active().material_bindings==binding&&bad.active().deformation==old_deform&&bad.active().shape_source==old_shape,"Foreign equal-valued metadata domain cannot borrow current owner; failed authoring publishes nothing");
 doc.replace(current);endpoint(doc);const auto file=out/"late-cancel.dsculpt";doc.save(file);const auto undo=doc.frame().state->can_undo,redo=doc.frame().state->can_redo;auto gate=std::make_shared<mirror_gate::Gate>();mirror_gate::Release release{gate};auto ready=gate->computed.get_future();{std::lock_guard lock(mirror_gate::mutex);mirror_gate::pending=gate;}doc.author_shape(mirror);check(ready.wait_for(std::chrono::seconds(120))==std::future_status::ready&&gate->changed,"Actual retained Mirror finishes before test-only publication gate");auto queued=doc.save_async(file);check(queued.wait_for(std::chrono::milliseconds(20))!=std::future_status::ready,"Queued Save remains ordered behind retained Mirror");doc.cancel_background_work();release.release();check(queued.wait_for(std::chrono::seconds(120))==std::future_status::ready&&refuses([&]{queued.get();},"cancel"),"Late retained Mirror cancellation propagates actual dependent Save failure");
 check(doc.wait_asset(std::chrono::seconds(120))&&doc.wait_mesh(std::chrono::seconds(120))&&doc.copy_scene().encode()==before&&Scene::load(file).encode()==before&&doc.frame().state->can_undo==undo&&doc.frame().state->can_redo==redo,"Cancellation preserves exact Scene, native file and global history");doc.author_shape(mirror);endpoint(doc);check(doc.copy_scene().encode()==encoded,"Retained Mirror retries exactly after real cancellation");check(source.encode()==original,"All retained key attempts preserve their complete original source");
 std::cout<<"PASS "<<checks<<" retained Paint24/normal/key Mirror checks\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<"\n";return 1;}
