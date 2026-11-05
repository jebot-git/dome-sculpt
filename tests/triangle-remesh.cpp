// SPDX-License-Identifier: GPL-3.0-or-later
// Reuse the genuine painted/posed fixture and unchanged voxel regression.
#define main legacy_voxel_main
#include "voxel-authoring.cpp"
#undef main
#include "dome/polygon-scene.hpp"
#include "dome/source-placement.hpp"
#include "polygon-conversion.hpp"
#include <condition_variable>
#include <mutex>
int legacy_remesh_main();
namespace triangle_gate {
std::mutex mutex;std::condition_variable changed;bool armed=false,entered=false,released=false;
void arm(){std::lock_guard lock(mutex);armed=true;entered=released=false;}
void wait(){std::unique_lock lock(mutex);if(!changed.wait_for(lock,std::chrono::seconds(10),[]{return entered;}))throw std::runtime_error("Actual remesh worker gate not reached");}
void release(){std::lock_guard lock(mutex);released=true;changed.notify_all();}
struct Release{~Release(){release();}};
}
extern "C" uint64_t real_triangle(dome::Scene&,float,const std::function<bool()>&)
 asm("__real__ZN4dome22create_triangle_remeshERNS_5SceneEfRKSt8functionIFbvEE");
extern "C" uint64_t wrapped_triangle(dome::Scene&,float,const std::function<bool()>&)
 asm("__wrap__ZN4dome22create_triangle_remeshERNS_5SceneEfRKSt8functionIFbvEE");
extern "C" uint64_t wrapped_triangle(dome::Scene &scene,float edge,const std::function<bool()> &cancelled){
 {std::unique_lock lock(triangle_gate::mutex);if(triangle_gate::armed){triangle_gate::armed=false;triangle_gate::entered=true;triangle_gate::changed.notify_all();triangle_gate::changed.wait(lock,[]{return triangle_gate::released;});}}
 return real_triangle(scene,edge,cancelled);
}
namespace {
size_t triangle_checks=0,triangle_failures=0;
void expect(bool ok,const char *message){++triangle_checks;check(ok,message);}
template<class F>bool rejected(F f){try{f();return false;}catch(const std::exception&){return true;}}
Scene shaped(int kind){
 auto s=source_scene();auto &o=s.active();o.name="Původní — 元の形";
 auto d=std::make_shared<DeformationState>(*o.deformation);d->morphs=MorphSet(o.painted->mesh());auto target=o.painted->mesh().positions;
 for(auto &p:target)p.z+=.35f*p.x+.035f;
 d->morphs->set_target(o.painted->mesh(),"slant",target);d->morph_weights={.4f};o.deformation=std::move(d);
 if(kind){std::array<double,16> m{kind==2?-1.15:1.15,.1,.05,0,.2,.8,.12,0,0,.15,1.1,0,0,0,0,1};o.affine_edit=std::make_shared<const NativeAffineEdit>(AffineTransform(m));}
 s.validate();return s;
}
void retained(const Scene &before,const Scene &after,uint64_t source){
 const auto &a=before.object(source),&b=after.object(source);
 expect(!b.visible&&a.name==b.name&&a.group==b.group&&a.affine_edit==b.affine_edit,"New polygon retains the genuine hidden source and placement identity");
 expect(a.painted->encode()==b.painted->encode()&&a.volume->encode()==b.volume->encode()&&a.deformation->encode(a.painted->mesh())==b.deformation->encode(b.painted->mesh()),"All original paint layers, rig, pose, morphs and volume remain exact");
 const auto &out=after.active();expect(out.id!=source&&out.mesh_active&&out.painted&&out.polygon&&out.polygon->active&&!out.deformation&&!out.appearance&&!out.material_bindings,"Actual isotropic kernel produces an independent editable unpainted polygon object");
 expect(out.transform.position.x==a.transform.position.x&&out.transform.position.y==a.transform.position.y&&out.transform.position.z==a.transform.position.z&&out.transform.scale==a.transform.scale&&out.group==a.group&&out.affine_edit==a.affine_edit,"New authority keeps the real original placement rather than a world-baked identity");
 expect(out.polygon->origin&&out.polygon->origin->source_object==source&&out.polygon->origin->paint->encode()==a.painted->encode(),"Restorable origin is the authentic complete before source");
 auto restored=after.snapshot();restore_polygon_origin(restored,out.id);
 const auto &r=restored.active();expect(r.painted->encode()==a.painted->encode()&&r.deformation->encode(r.painted->mesh())==a.deformation->encode(a.painted->mesh())&&r.group==a.group&&r.affine_edit==a.affine_edit,"Immediate Restore preserves source content, pose and actual affine placement");
 // The deformation and placement are independently invertible affine maps of
 // the original cube. Verify every remeshed corner stays on its analytic shell.
 auto mesh=out.painted->mesh();
 for(auto p:mesh.positions){p.x-=.15f;p.z-=.14f*p.x+.014f;
  const auto boundary=std::max({std::abs(p.x),std::abs(p.y),std::abs(p.z)});
  expect(std::abs(boundary-.1f)<2e-5f,"Remeshed vertices stay on the independent posed/morphed cube surface");}
 for(size_t i=0;i<mesh.indices.size();i+=3){auto a=mesh.positions[mesh.indices[i]],b=mesh.positions[mesh.indices[i+1]],c=mesh.positions[mesh.indices[i+2]];auto n=cross(b-a,c-a);auto center=(a+b+c)/3.f-Vec3{.15f,0,.014f};
  expect(dot(n,center)>0,"Derived local topology keeps outward winding after reflection and inverse placement");}
}
}
int main(int argc,char **argv)try{
 const std::filesystem::path out=argc>1?argv[1]:std::filesystem::temp_directory_path()/"dome-triangle-remesh-native";std::filesystem::create_directories(out);
 for(int kind=0;kind<3;++kind){auto source=shaped(kind);auto exact=source.encode();auto source_id=source.active().id;auto result=source.snapshot();auto id=create_triangle_remesh(result,140);expect(result.active().id==id,"Pure native remesh publishes the new editable object");retained(source,result,source_id);expect(source.encode()==exact,"Detached remesh never mutates its caller source owners");
  auto path=out/("roundtrip-"+std::to_string(kind)+".dsculpt");result.save(path);expect(Scene::load(path).encode()==result.encode(),"Actual new polygon/origin/placement survives native Save/Open exactly");
  auto cancelled=source.snapshot();size_t polls=0;expect(rejected([&]{create_triangle_remesh(cancelled,140,[&]{return ++polls==8;});})&&cancelled.encode()==exact,"Cancellation during actual world extraction preserves the complete caller");
 }
 expect(legacy_remesh_main()==0,"Unchanged complete triangle kernel regression still passes");
 std::cout<<"STAGE shaped"<<std::endl;auto source=shaped(1);auto exact=source.encode();std::cout<<"STAGE Document ctor"<<std::endl;Document doc;std::cout<<"STAGE replace"<<std::endl;doc.replace(source.snapshot());std::cout<<"STAGE controls"<<std::endl;
 doc.begin_triangle_remesh();doc.edit_triangle_edge(140);expect(doc.frame().state->controls.parameter==VrParameter::TriangleEdge&&doc.copy_scene().encode()==exact&&!doc.frame().state->dirty,"Continuous native controls stage physical edge length without editing the scene");doc.end_triangle_remesh(false);expect(doc.copy_scene().encode()==exact,"Native Cancel leaves exact original source/history");
 doc.begin_triangle_remesh();doc.edit_triangle_edge(140);doc.duplicate_object();expect(doc.copy_scene().objects().size()==2&&!doc.frame().state->asset_busy,"An unrelated command cancels remesh options without implicitly running them");expect(doc.undo()&&doc.copy_scene().encode()==exact,"Only the unrelated command enters history");
 doc.begin_triangle_remesh();doc.edit_triangle_edge(140);doc.end_triangle_remesh(true);auto saved=doc.save_async(out/"immediate-save.dsculpt");saved.get();expect(doc.wait_asset()&&doc.frame().state->error.empty(),"Explicit native Apply queues real remeshing and immediate ordered Save");auto accepted=doc.copy_scene().encode();expect(Scene::load(out/"immediate-save.dsculpt").encode()==accepted,"Following Save contains the accepted polygon rather than the before scene");expect(doc.undo()&&doc.copy_scene().encode()==exact&&doc.redo()&&doc.copy_scene().encode()==accepted,"One Undo/Redo restores exact source visibility, identities, original paint and new topology");
 try {doc.begin_polygon();expect(doc.wait_polygon()&&doc.frame().state->polygon,"Remeshed authority opens the real polygon editor immediately");doc.edit_polygon(PolygonAction::SelectAll);expect(doc.wait_polygon()&&doc.frame().state->polygon->selection->ids.size()>0,"Actual polygon selection operates on the remeshed logical vertices");doc.end_polygon(false);expect(doc.copy_scene().encode()==accepted,"Cancelling real polygon editing preserves accepted remesh exactly");}
 catch(const std::exception &e){++triangle_failures;std::cerr<<"FAIL affine polygon workflow: "<<e.what()<<std::endl;doc.end_polygon(false);}
 doc.replace(source.snapshot());{triangle_gate::Release release;triangle_gate::arm();doc.remesh_to_triangles(140);triangle_gate::wait();auto following=doc.save_async(out/"cancel-save.dsculpt");doc.cancel_background_work();triangle_gate::release();expect(rejected([&]{following.get();})&&doc.wait_asset()&&doc.copy_scene().encode()==exact,"Actual gated worker Cancel rejects dependent Save and rolls back all source state");}
 doc.remesh_to_triangles(140);expect(doc.wait_asset()&&doc.frame().state->error.empty(),"Native remesh recovers after genuine asynchronous cancellation");
 doc.replace(source.snapshot());Driver controller(doc);controller.send();controller.send(0,0,true);controller.send();controller.sector(8);controller.sector(9);controller.sector(5);expect(doc.frame().state->controls.parameter==VrParameter::TriangleEdge,"Controller Mesh tools directly opens isotropic edge controls");controller.send(1);expect(doc.frame().state->controls.value[0]>50,"Joystick changes world edge length continuously");controller.f.focused=false;controller.send();controller.f.focused=true;controller.send();expect(doc.copy_scene().encode()==exact&&doc.frame().state->controls.parameter==VrParameter::None,"Tracking focus loss cancels controller remesh with no unintended geometry");
 for(float bad:{0.f,-1.f,1001.f,std::numeric_limits<float>::quiet_NaN()}){auto copy=source.snapshot();expect(rejected([&]{create_triangle_remesh(copy,bad);})&&copy.encode()==exact,"Invalid edge length refuses before changing original source");}
 std::cout<<"PASS "<<triangle_checks<<" triangle-remesh checks\n";
 expect(legacy_voxel_main()==0,"Unchanged complete voxel remeshing and controller regression still passes");
 expect(triangle_failures==0,"All polygon integration workflows pass");return 0;
}catch(const std::exception &e){triangle_gate::release();std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
