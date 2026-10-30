// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/import.hpp"
#include <nlohmann/json.hpp>
#include <condition_variable>
#include <future>
#include <bit>
#include <iostream>
#include <unistd.h>
using namespace dome;
namespace {
unsigned checks=0;
void check(bool v,const char *m){++checks;if(!v)throw std::runtime_error(m);std::cout<<"PASS "<<m<<'\n';}
Volume sphere(float voxel=.04f){Volume v(voxel);v.begin_stroke();Brush b;b.radius=.237f;b.blend=0;v.stamp(b);v.end_stroke();return v.geometry_snapshot();}
nlohmann::json ordered(const Volume &v){auto j=v.encode();std::sort(j["samples"].begin(),j["samples"].end());for(auto &l:j["sculpt_layers"])std::sort(l["delta"].begin(),l["delta"].end());if(j.contains("sculpt_mask"))std::sort(j["sculpt_mask"]["values"].begin(),j["sculpt_mask"]["values"].end());return j;}
NativeStrokeSample sample(float x=0){NativeStrokeSample s;s.origin={x,0,1};s.direction={0,0,-1};s.brush.tool=Tool::Carve;s.brush.radius=.08f;s.brush.blend=0;return s;}
void ready(Document &d){check(d.wait_mesh(),"Immutable scene surface ready");}
std::atomic_bool armed{false};std::mutex gate;std::condition_variable changed;bool entered=false,released=false;
void arm(){std::lock_guard lock(gate);entered=released=false;armed=true;}
void await(){std::unique_lock lock(gate);if(!changed.wait_for(lock,std::chrono::seconds(8),[]{return entered;}))throw std::runtime_error("Native worker did not enter mutation barrier");}
void release(){std::lock_guard lock(gate);released=true;changed.notify_all();}
Mesh quad(){Mesh m;m.positions={{-.5f,-.5f,0},{.5f,-.5f,0},{.5f,.5f,0},{-.5f,.5f,0}};m.normals.assign(4,{0,0,1});m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.indices={0,1,2,0,2,3};return m;}
}
extern "C" void real_stamp(Volume*,Brush,size_t,const std::function<bool()> &) asm("__real__ZN4dome6Volume5stampENS_5BrushEmRKSt8functionIFbvEE");
extern "C" void wrap_stamp(Volume*,Brush,size_t,const std::function<bool()> &) asm("__wrap__ZN4dome6Volume5stampENS_5BrushEmRKSt8functionIFbvEE");
extern "C" void wrap_stamp(Volume *v,Brush b,size_t n,const std::function<bool()> &cancelled){if(armed.exchange(false)){std::unique_lock lock(gate);entered=true;changed.notify_all();if(!changed.wait_for(lock,std::chrono::seconds(10),[]{return released;}))throw std::runtime_error("Mutation barrier timeout");}real_stamp(v,b,n,cancelled);}
int main(int argc,char **argv){try{
 if(argc==3&&std::string(argv[1])=="--ui-fixture"){Scene s;s.active().volume=std::make_shared<Volume>(sphere(.015f));s.save(argv[2]);return 0;}
 if(argc==3&&std::string(argv[1])=="--paint-fixture"){
  Scene s;auto mesh=quad();s.active().painted=std::make_shared<PaintedMesh>(mesh,128);s.active().mesh_active=true;
  DeformationState rig(mesh);rig.skeleton=Skeleton{{{"root",-1,{},1},{"limb",0,{},.5f}}};rig.skin=SkinWeights{};rig.skin->vertices.assign(4,{{1,1}});rig.pose.resize(2);rig.pose[1].translation={1,0,0};
  rig.morphs=MorphSet(mesh);auto positions=mesh.positions;for(auto &p:positions)p.x+=2;rig.morphs->set_target(mesh,"Moved limb",positions);rig.morph_weights={1};s.set_deformation(1,rig);s.active().transform.position.x=-3;s.save(argv[2]);return 0;
 }
 if(argc==3&&std::string(argv[1])=="--inspect"){std::cout<<Scene::load(argv[2]).encode().dump()<<'\n';return 0;}
 if(argc==2&&std::string(argv[1])=="--preview-test"){
  Document live;live.replace(sphere());ready(live);auto original=ordered(live.copy_volume());auto baseline=live.frame().surface;auto revision=live.frame().state->revision;
  struct ReleasePreview{~ReleasePreview(){release();}} guard;
  auto await_preview=[&](size_t after){auto end=std::chrono::steady_clock::now()+std::chrono::seconds(8);while(live.frame().state->native_samples<=after&&std::chrono::steady_clock::now()<end)std::this_thread::yield();check(live.frame().state->native_samples>after,"Native processed prefix published");check(live.wait_mesh(),"Immutable native preview becomes ready");return live.frame().surface;};
  auto hash=[](const Mesh &mesh){uint64_t h=1469598103934665603ULL;for(auto p:mesh.positions)for(float x:{p.x,p.y,p.z}){h^=std::bit_cast<uint32_t>(x);h*=1099511628211ULL;}return h;};
  auto release_after_throttle=[] {std::unique_lock lock(gate);changed.wait_for(lock,std::chrono::milliseconds(35),[]{return released;});released=true;changed.notify_all();};
  arm();check(live.begin_native_stroke()&&live.sample_native_stroke(sample(-.05f)),"Live preview gesture starts");await();release_after_throttle();auto first=await_preview(0);auto first_hash=hash(first->mesh);auto samples=live.frame().state->native_samples;
  check(first->native_gesture==live.frame().state->native_gesture&&first->native_samples>=samples&&first_hash!=hash(baseline->mesh),"Worker publishes actual changed geometry with immutable session/sample identity");
  check(ordered(live.copy_volume())==original&&live.frame().state->revision==revision,"Visible native preview does not mutate authoritative scene/revision");
  arm();live.sample_native_stroke(sample(.08f));await();release_after_throttle();auto second=await_preview(samples);
  check(hash(first->mesh)==first_hash&&second!=first,"Later samples retain every byte of an earlier held preview");
  live.end_native_stroke(false);check(live.wait_native_stroke(),"Live preview cancellation drains");check(live.frame().surface==baseline&&ordered(live.copy_volume())==original&&!live.frame().state->can_undo,"Cancel restores exact original render snapshot/scene/history after visible previews");return 0;
 }
 struct Release{~Release(){release();}} guard;
 Document d;d.replace(sphere());ready(d);auto original=ordered(d.copy_volume());auto revision=d.frame().state->revision;
 check(d.begin_native_stroke(),"Native ray gesture starts");auto miss=sample(2);check(d.sample_native_stroke(miss),"Miss sample ordered");d.end_native_stroke();check(d.wait_native_stroke(),"Miss drains");check(ordered(d.copy_volume())==original&&d.frame().state->revision==revision&&!d.frame().state->can_undo,"Miss gesture is exact no-op with no history");
 arm();check(d.begin_native_stroke()&&d.sample_native_stroke(sample()),"Native mutation queued");await();
 auto inspect=std::async(std::launch::async,[&]{return d.copy_scene();});check(inspect.wait_for(std::chrono::milliseconds(100))==std::future_status::ready,"Expensive native mutation holds no model mutex");inspect.get();
 check(ordered(d.copy_volume())==original,"Authoritative geometry stays unchanged during detached gesture");
 d.end_native_stroke(false);check(!d.frame().state->native_pending&&!d.frame().state->can_undo,"Cancel returns immediately before mutation completes");release();check(d.wait_native_stroke(),"Cancelled worker drains");check(ordered(d.copy_volume())==original,"Late cancelled sample cannot publish geometry");
 Document oracle;oracle.replace(sphere());ready(oracle);oracle.begin_desktop();std::optional<Vec3> prior;
 check(d.begin_native_stroke(),"Ordered gesture starts");for(float x:{-.06f,-.03f,0.f,.03f,.06f}){auto s=sample(x);d.sample_native_stroke(s);auto hit=oracle.raycast(s.origin,s.direction);if(hit){auto b=s.brush;b.from=prior.value_or(*hit);b.to=*hit;b.normal=oracle.gradient(*hit);oracle.stamp_desktop(b);prior=*hit;}else prior.reset();}
 oracle.end_desktop();d.end_native_stroke();check(d.wait_native_stroke(),"All accepted samples finish");ready(d);check(ordered(d.copy_volume())==ordered(oracle.copy_volume()),"Every ray/capsule matches independent synchronous ordered stroke");check(d.undo()&&!d.frame().state->can_undo&&ordered(d.copy_volume())==original,"Accepted native stroke is one exact undo");ready(d);check(d.redo(),"Native redo");ready(d);auto result=ordered(d.copy_volume());d.undo();ready(d);
 arm();check(d.begin_native_stroke()&&d.sample_native_stroke(sample()),"Ordered save gesture queued");await();auto path=std::filesystem::temp_directory_path()/("dome-native-"+std::to_string(getpid())+".dsculpt");auto saved=d.save_async(path);check(saved.wait_for(std::chrono::milliseconds(20))!=std::future_status::ready,"Save waits for accepted endpoint without blocking caller");release();saved.get();check(d.wait_native_stroke(),"Saved native stroke drained");check(ordered(*Scene::load(path).active().volume)==ordered(d.copy_volume()),"Ordered Save captures exact accepted gesture result");
 d.undo();ready(d);arm();check(d.begin_native_stroke()&&d.sample_native_stroke(sample()),"Stale source fixture queued");await();d.replace(Volume(.07f));release();check(d.wait_native_stroke(),"Stale worker drains");check(d.copy_volume().voxel_size()==.07f&&d.copy_volume().voxels().empty()&&!d.frame().state->can_undo,"Replacement generation rejects late desktop result");
 Document overflow;overflow.replace(sphere());ready(overflow);arm();overflow.begin_native_stroke();overflow.sample_native_stroke(sample());await();bool accepted=true;for(unsigned i=1;i<=4096&&accepted;++i)accepted=overflow.sample_native_stroke(sample());check(!accepted&&!overflow.frame().state->native_pending&&!overflow.frame().state->error.empty(),"Sample overflow explicitly cancels whole transaction rather than dropping path");release();check(overflow.wait_native_stroke()&&ordered(overflow.copy_volume())==original&&!overflow.frame().state->can_undo,"Overflow retains original exact geometry and history");
 // Evaluated UV coverage has an independent analytic translated-quad oracle.
 auto rest=quad();auto paint=std::make_shared<PaintedMesh>(rest,64);auto shifted=std::make_shared<Mesh>(rest);for(auto &p:shifted->positions)p.x+=2;
 auto coverage=paint->evaluate(shifted);PaintSettings settings;settings.color={1,0,0,1};Brush b;b.from=b.to={2,0,0};b.normal={0,0,1};b.radius=.2f;b.blend=0;
 paint->begin_stroke();paint->stamp_evaluated(b,settings,*coverage);paint->end_stroke();check(paint->pixel(1,32,32).data[9]>200&&paint->pixel(1,5,5).data[9]==0,"Translated evaluated surface paints analytic original-UV center only");check(paint->mesh().positions[0].x==-.5f&&paint->mesh().uv==rest.uv,"Evaluated paint leaves original geometry/UV identities exact");
 Scene posed;posed.active().painted=std::make_shared<PaintedMesh>(rest,64);posed.active().mesh_active=true;DeformationState def(rest);def.morphs=MorphSet(rest);auto positions=rest.positions;for(auto &p:positions)p.x+=2;def.morphs->set_target(rest,"Moved limb",positions);def.morph_weights={1};posed.set_deformation(1,def);auto deformation=posed.active().deformation;
 Document morph;morph.replace(posed);ready(morph);morph.paint_settings(settings);check(morph.begin_native_stroke(),"Morphed native paint starts");auto ray=sample(2);ray.brush.radius=.2f;morph.sample_native_stroke(ray);morph.end_native_stroke();check(morph.wait_native_stroke(),"Morphed native paint completes");ready(morph);auto painted=morph.copy_scene().active().painted;
 check(painted->pixel(1,32,32).data[9]>200&&painted->pixel(1,5,5).data[9]==0,"Native evaluated ray/coverage maps moved morph to original UV layers");check(morph.copy_scene().active().deformation==deformation&&painted->mesh().positions[0].x==-.5f,"Native painting preserves original morph/rest/source identity");check(morph.undo()&&!morph.frame().state->can_undo&&morph.copy_scene().active().painted->pixel(1,32,32).data[9]==0,"Posed painting is one exact undo");
 // Cached Save must preserve active selection even when selection alone
 // does not rebuild the authoritative geometry snapshot.
 Scene multi;multi.active().volume=std::make_shared<Volume>(sphere());auto empty=multi.add("Empty target",Volume(.07f));multi.select(1);Document selection;selection.replace(multi);ready(selection);selection.select_object(empty);auto selection_path=path.string()+".selection";selection.save(selection_path);auto selected=Scene::load(selection_path);check(selected.active().id==empty&&selected.objects().size()==2&&selected.active().volume->voxel_size()==.07f,"Worker Save freezes exact current active-object selection with cached immutable geometry");std::filesystem::remove(selection_path);
 // A skinned moved limb and independent morph retain their original archives.
 Scene skin=posed;DeformationState rig=def;rig.skeleton=Skeleton{{{"root",-1,{},1},{"limb",0,{},.5f}}};rig.skin=SkinWeights{};rig.skin->vertices.assign(4,{{1,1}});rig.pose.resize(2);rig.pose[1].translation={1,0,0};skin.set_deformation(1,rig);
 Document limb;limb.replace(skin);ready(limb);limb.paint_settings(settings);auto rig_before=limb.copy_scene().active().deformation->encode(rest);
 check(limb.begin_native_stroke(),"Skinned and morphed limb paint starts");ray=sample(3);ray.brush.radius=.2f;limb.sample_native_stroke(ray);limb.end_native_stroke();check(limb.wait_native_stroke(),"Skinned and morphed limb completes");ready(limb);
 auto limb_scene=limb.copy_scene();check(limb_scene.active().painted->pixel(1,32,32).data[9]>200&&limb_scene.active().painted->pixel(1,5,5).data[9]==0,"Evaluated skin plus morph paints analytic original-UV center");
 check(limb_scene.active().deformation->encode(rest)==rig_before&&limb_scene.active().painted->mesh().uv==rest.uv,"Pose/skin/morph/native UV archives remain exact after paint");
 auto limb_path=path.string()+".limb";limb.save(limb_path);auto loaded=Scene::load(limb_path);check(loaded.active().deformation->encode(rest)==rig_before&&loaded.active().painted->pixel(1,32,32).data[9]>200,"Native save/reload preserves posed paint and original rig payload");
 auto exported=path.string()+".glb";limb.export_asset(exported).get();ImportLimits limits;limits.mode=ImportMode::PreserveGltf;auto asset=import_mesh(exported,limits);check(!asset.skins.empty()&&!asset.primitives[0].morph_targets.empty()&&!asset.primitives[0].influences.empty()&&asset.primitives[0].mesh.positions[0].x==-.5f,"glTF export retains original rest geometry, skin and morph channels after evaluated paint");
 // Rotated evaluated normals must drive front-only coverage, independently
 // of the rest normals (which are perpendicular to this brush direction).
 Scene rotated;rotated.active().painted=std::make_shared<PaintedMesh>(rest,64);rotated.active().mesh_active=true;DeformationState turned(rest);turned.skeleton=Skeleton{{{"limb",-1,{},1}}};turned.skin=SkinWeights{};turned.skin->vertices.assign(4,{{0,1}});turned.pose.resize(1);turned.pose[0].rotation={.70710678f,.70710678f,0,0};rotated.set_deformation(1,turned);
 Document front;front.replace(rotated);ready(front);front.paint_settings(settings);check(front.begin_native_stroke(),"Rotated front-only paint starts");ray=sample();ray.origin={0,-1,0};ray.direction={0,1,0};ray.brush.radius=.2f;front.sample_native_stroke(ray);front.end_native_stroke();check(front.wait_native_stroke(),"Rotated front-only paint completes");check(front.copy_scene().active().painted->pixel(1,32,32).data[9]>200,"Front-only evaluated coverage uses posed normals rather than perpendicular rest normals");
 // Preparation cancellation and source/topology mismatch are transactional.
 auto untouched=std::make_shared<PaintedMesh>(rest,64);untouched->begin_stroke();unsigned stops=0;bool refused=false;try{auto unused=untouched->evaluate(shifted,[&]{return ++stops>2;});}catch(const std::exception&){refused=true;}check(refused&&!untouched->pending_changes(),"Cancelled evaluated preparation changes no paint layers");
 auto wrong=std::make_shared<Mesh>(*shifted);std::swap(wrong->indices[0],wrong->indices[1]);refused=false;try{untouched->evaluate(wrong);}catch(const std::exception&){refused=true;}check(refused&&!untouched->pending_changes(),"Evaluated topology replacement is explicitly refused");
 auto identity=untouched->evaluate(std::make_shared<const Mesh>(rest));b.from=b.to={0,0,0};stops=0;refused=false;try{untouched->stamp_evaluated(b,settings,*identity,[&]{return ++stops>2;});}catch(const std::exception&){refused=true;}check(refused&&!untouched->pending_changes(),"Mid-tile evaluated paint cancellation restores exact previous tiles");untouched->cancel_stroke();
 // Accepted shutdown drains its worker and ordered file request.
 auto closing=std::make_unique<Document>();closing->replace(sphere());ready(*closing);arm();closing->begin_native_stroke();closing->sample_native_stroke(sample());await();auto close_path=path.string()+".close";auto close_saved=closing->save_async(close_path);auto closed=std::async(std::launch::async,[&]{closing.reset();});check(closed.wait_for(std::chrono::milliseconds(20))!=std::future_status::ready,"Shutdown waits for accepted mutation rather than abandoning it");release();closed.get();close_saved.get();check(ordered(*Scene::load(close_path).active().volume)!=original,"Final shutdown drains accepted endpoint through ordered Save");
 std::filesystem::remove(limb_path);std::filesystem::remove(exported);std::filesystem::remove(close_path);
 std::filesystem::remove(path);std::cout<<checks<<" native stroke checks passed\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
