// SPDX-License-Identifier: GPL-3.0-or-later
#define main previous_material_graphics_suite
#include "mtoon-render.cpp"
#undef main
#define main previous_controller_controls_suite
#include "vr-controls.cpp"
#undef main
#include "prepared-normal-consumer.hpp"
#include "dome/material-bindings.hpp"
#include "dome/normal-frames.hpp"
#include "dome/material-visibility.hpp"
#include "dome/creation.hpp"
#include <nlohmann/json.hpp>
namespace preview_fault {
thread_local bool enabled=false,hit=false;
thread_local size_t calls=0,fail=SIZE_MAX;
}
void *operator new(size_t n){if(preview_fault::enabled&&++preview_fault::calls>=preview_fault::fail){preview_fault::hit=true;throw std::bad_alloc();}if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void *p)noexcept{std::free(p);}void operator delete(void *p,size_t)noexcept{std::free(p);}
void *operator new(size_t n,const std::nothrow_t&)noexcept{try{return ::operator new(n);}catch(...){return nullptr;}}
void operator delete(void *p,const std::nothrow_t&)noexcept{std::free(p);}
namespace {
Scene preview_model(bool alias=false){
 auto mesh=quad();auto maps=std::make_shared<PaintSourceMaps>();maps->color.image=image({180,210,240,255});maps->normal.image=image({230,153,204,255});
 maps->normal_scale=1;for(auto *view:{&maps->color,&maps->normal}){view->wrap_s=view->wrap_t=33071;view->min_filter=view->mag_filter=9729;}
 auto paint=PaintedMesh::from_maps(mesh,maps,64);auto frame=std::make_shared<NormalFrames>();frame->origin_vertex_count=mesh.positions.size();frame->origin_vertices=mesh.indices;
 frame->normals.assign(mesh.indices.size(),{0,0,1});frame->tangents.assign(mesh.indices.size(),{1,0,0,1});auto mt=std::make_shared<NormalFrames>(*frame);mt->normals.assign(mesh.indices.size(),{.6f,0,.8f});mt->tangents.assign(mesh.indices.size(),{.8f,0,-.6f,1});
 auto frames=std::make_shared<NormalFrameSet>();frames->pbr=frame;frames->mtoon=alias?frame:mt;
 auto material=MToonMaterial::from_vrm1({{"alphaMode","MASK"},{"normalTexture",{{"index",1}}},{"pbrMetallicRoughness",{{"baseColorTexture",{{"index",0}}}}},{"extensions",{{"VRMC_materials_mtoon",{{"specVersion","1.0"}}}}}});
 auto binding=std::make_shared<SourceMaterialBindings>();binding->has_mtoon=true;binding->material=material;binding->normal_frames=frames;binding->normal_frame=SourceMaterialNormalFrame::OriginalAttributes;
 binding->roles[0].image=std::make_shared<const Image>(Image{2,1,{255,255,255,0,255,255,255,255}});binding->roles[0].wrap_s=binding->roles[0].wrap_t=33071;binding->roles[0].min_filter=binding->roles[0].mag_filter=9728;binding->roles[2]=maps->normal;
 auto origin=std::make_shared<SourceUvOrigin>();origin->paint=paint->snapshot();origin->material=material;origin->roles=binding->roles;binding->uv_origin=origin;
 Scene scene;auto &object=scene.active();object.mesh_active=true;object.painted=paint;object.material_bindings=binding;object.material.color={1,1,1,1};object.material.alpha=Material::AlphaMode::Automatic;scene.validate();return scene;
}
void ready(Document &d){check(d.wait_mesh()&&d.frame().state->error.empty(),"Preview worker publishes the requested material-role endpoint");}
bool direction(const std::vector<Vec3> &values,Vec3 expected){return !values.empty()&&std::all_of(values.begin(),values.end(),[&](auto v){return length(v-expected)<1e-6f;});}
void device(bool hardware){std::string name=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GPU "<<name<<'\n';check(!hardware||(name.find("Intel")!=std::string::npos&&name.find("Arc")!=std::string::npos&&name.find("A770")!=std::string::npos),"Requested strict Intel Arc A770 device identity is present");}
struct AssetGate {std::mutex mutex;std::condition_variable changed;bool entered=false,released=false;void stop(){std::unique_lock lock(mutex);entered=true;changed.notify_all();changed.wait(lock,[&]{return released;});}void wait(){std::unique_lock lock(mutex);if(!changed.wait_for(lock,std::chrono::seconds(30),[&]{return entered;}))throw std::runtime_error("Accepted asset did not enter its barrier");}void release(){std::lock_guard lock(mutex);released=true;changed.notify_all();}};
struct AssetRelease {std::shared_ptr<AssetGate> gate;~AssetRelease(){gate->release();}};
std::atomic<std::shared_ptr<AssetGate>> mesh_gate;
void open_view(Driver &hand,bool left=false){hand.open();auto state=hand.doc.frame().state;auto &tracked=hand.frame.hands[left?0:1].tracked;tracked.valid=tracked.aimValid=true;tracked.aim={ob::transform(state->panel,{0,(.5f-.06f)*.6f,.25f}),state->panel.orientation,1};if(left){tracked.trigger=0;hand.send();tracked.trigger=.8f;hand.send();tracked.trigger=0;hand.send();}else{hand.send(0,0,false,.8f);hand.send();}check(hand.doc.frame().state->menu_page==26,"Either controller opens the root View header while preserving the joystick owner's shared preview page");}
}
extern "C" std::vector<ObjectSurface> surfaces_real(const Scene*,const std::function<bool()>&,const Scene::SculptMesher&) asm("__real__ZNK4dome5Scene16preview_surfacesERKSt8functionIFbvEERKS1_IFNS_4MeshEmRKNS_6VolumeEEE");
extern "C" std::vector<ObjectSurface> surfaces_wrap(const Scene*,const std::function<bool()>&,const Scene::SculptMesher&) asm("__wrap__ZNK4dome5Scene16preview_surfacesERKSt8functionIFbvEERKS1_IFNS_4MeshEmRKNS_6VolumeEEE");
extern "C" std::vector<ObjectSurface> surfaces_wrap(const Scene *scene,const std::function<bool()> &cancelled,const Scene::SculptMesher &mesher){auto result=surfaces_real(scene,cancelled,mesher);if(auto gate=mesh_gate.exchange({}))gate->stop();return result;}
int main(int argc,char **argv)try{
 if(argc==3&&std::string_view(argv[1])=="--fixture"){preview_model().save(argv[2]);return 0;}
 if(argc==3&&std::string_view(argv[1])=="--inspect"){std::cout<<Scene::load(argv[2]).encode().dump()<<'\n';return 0;}
 if(argc>=3&&std::string_view(argv[1])=="--avatars"){
  const std::filesystem::path prefix=argv[2];std::filesystem::create_directories(prefix.parent_path());NativeGraphics graphics;Fbo fbo;fbo.bind();SurfaceRenderer renderer;prepared_normal_consumer::Renderer consumer;
  device(argc==4&&std::string_view(argv[3])=="--hardware");for(const auto *baseline:{"vroid-beta-female","vroid-beta-male"}){
   Scene original;auto group=append_humanoid(original,humanoid_baseline_defaults(baseline),"Actual PBR preview avatar",{},256);original.group(group).transform.scale=.8f;original.group(group).transform.position.y=-.7f;
   const auto exact=original.encode();
   for(int pose=0;pose<3;++pose)for(float weight:{0.f,.35f,1.f}){
    auto scene=original;for(const auto &item:scene.objects())if(item.group==group&&item.deformation){auto d=*item.deformation;d.pose.clear();for(const auto &bone:d.skeleton->bones)d.pose.push_back(bone.rest);if(pose){auto hips=d.humanoid->joints.at("hips"),spine=d.humanoid->joints.at("spine");d.pose[hips].scale={1.15f,.9f,.7f};d.pose[spine].scale={.85f,1.05f,1.2f};if(pose==2)d.pose[spine].rotation={std::cos(.08f),0,0,std::sin(.08f)};}if(d.morphs&&!d.morphs->targets.empty()){d.morph_weights.assign(d.morphs->targets.size(),0);d.morph_weights[0]=weight;}scene.set_deformation(item.id,std::move(d));}
    const auto before=scene.encode();Document d;d.replace(scene);ready(d);d.shading(false);ready(d);auto snapshot=d.frame().surface;auto actual=draw(renderer,snapshot);auto file=prefix.string()+"-"+baseline+"-p"+std::to_string(pose)+"-w"+std::to_string(int(weight*100))+".glb";d.export_asset(file,AssetExportFormat::Glb).get();prepared_normal_consumer::Model parsed(file);fbo.bind();consumer.render(parsed.data);auto expected=read();size_t covered=0;for(size_t at=3;at<expected.size();at+=4)covered+=expected[at]>0;auto error=delta(actual,expected);std::cout<<"PBR_PREVIEW_AVATAR "<<baseline<<" pose="<<pose<<" morph="<<weight<<" parts="<<parsed.data->meshes_count<<" covered="<<covered<<" maxerror="<<error<<'\n';check(parsed.data->meshes_count==19&&covered>300&&error<=4,"Real public PBR command matches independent exported nineteen-part avatar pixels under current mixed pose and morph");
    auto uploads=renderer.buffer_stats();auto images=renderer.texture_stats();check(actual==draw(renderer,snapshot)&&uploads.geometry_uploads==renderer.buffer_stats().geometry_uploads&&images.uploads==renderer.texture_stats().uploads,"Actual avatar second eye reuses geometry and material images");d.authored_shading();ready(d);check(d.copy_scene().encode()==before&&!d.frame().state->can_undo,"Actual avatar authored restoration retains every source, current pose, weights and material");
   }
   check(original.encode()==exact,"Actual CC0 female/male baseline remains exact through all material preview cases");
  }
  std::cout<<"PASS "<<checks<<" actual avatar PBR preview checks\n";return 0;
 }
 if(argc<2||argc>3)return 2;const std::filesystem::path path=argv[1];std::filesystem::create_directories(path.parent_path());NativeGraphics graphics;device(argc==3&&std::string_view(argv[2])=="--hardware");Fbo fbo;fbo.bind();SurfaceRenderer renderer;
 auto scene=preview_model();const auto original=scene.encode();Document doc;doc.replace(scene);doc.configure_selection(true);ready(doc);auto authored=doc.frame().surface;
 check(!doc.frame().state->pbr_view&&!authored->pbr_view&&direction(authored->source_normals,{.6f,0,.8f}),"Default authored mode preserves the material and its distinct MToon normal frame");
 check(!authored->picking->raycast({-.3f,0,2},{0,0,-1}),"Authored picking respects the actual MToon alpha hole");
 const auto authored_pixels=draw(renderer,authored);const auto revision=doc.frame().state->revision;doc.shading(false);ready(doc);auto pbr=doc.frame().surface;
 check(doc.frame().state->revision==revision&&pbr->view_epoch==doc.frame().state->view_epoch,"View endpoint changes independently of the authoring revision and reaches its exact worker policy");
 check(pbr->pbr_view&&doc.frame().state->pbr_view&&direction(pbr->source_normals,{0,0,1}),"Existing native PBR command selects the genuine PBR source-normal role");
 check(pbr->geometry_identity!=authored->geometry_identity&&pbr->ranges[0].material_bindings==scene.active().material_bindings,"Distinct frame roles invalidate geometry once without replacing the stored material binding");
 check(pbr->picking->raycast({-.3f,0,2},{0,0,-1}).has_value(),"PBR picking uses the PBR alpha/material policy rather than the dormant MToon hole");
 const auto pixels=draw(renderer,pbr);check(delta(authored_pixels,pixels)>10,"User-visible authored and PBR views produce meaningfully different pixels");
 auto buffers=renderer.buffer_stats();auto textures=renderer.texture_stats();check(pixels==draw(renderer,pbr)&&renderer.buffer_stats().geometry_uploads==buffers.geometry_uploads&&renderer.texture_stats().uploads==textures.uploads,"Second-eye PBR frames reuse immutable geometry and images without new uploads");
 doc.export_asset(path,AssetExportFormat::Glb).get();prepared_normal_consumer::Model file(path);prepared_normal_consumer::Renderer consumer;fbo.bind();consumer.render(file.data);
 check(delta(pixels,read())<=3,"Actual PBR command output agrees with the independently parsed core-glTF consumer");
 doc.shading(false);check(doc.frame().surface==pbr,"No-op PBR selection schedules no extraction or packed upload");
 doc.authored_shading();ready(doc);check(direction(doc.frame().surface->source_normals,{.6f,0,.8f})&&!doc.frame().state->pbr_view,"Authored command restores the exact original role without changing any source data");
 check(doc.copy_scene().encode()==original&&!doc.frame().state->dirty&&!doc.frame().state->can_undo,"Preview commands change neither scene data, dirty state nor Undo history");
 Driver hand(doc);open_view(hand,true);hand.sector(1);ready(doc);check(doc.frame().state->pbr_view&&direction(doc.frame().surface->source_normals,{0,0,1}),"Actual joystick PBR action shares the native command and PBR frame endpoint");
 open_view(hand);hand.sector(0);ready(doc);check(!doc.frame().state->pbr_view&&doc.copy_scene().encode()==original,"Controller authored action restores preserved materials and source data");
 auto aliased=preview_model(true);aliased.active().material.toon=true;Document shared;shared.replace(aliased);ready(shared);auto before=shared.frame().surface;draw(renderer,before);buffers=renderer.buffer_stats();shared.shading(false);ready(shared);
 check(shared.frame().surface->geometry_identity==before->geometry_identity,"Exactly shared full attribute owners reuse the geometry identity across preview modes");draw(renderer,shared.frame().surface);check(renderer.buffer_stats().geometry_uploads==buffers.geometry_uploads,"Shared-frame material-only switching avoids static geometry uploads");
 auto shared_pixels=draw(renderer,shared.frame().surface);auto shared_file=path.string()+"-stored-toon.glb";shared.export_asset(shared_file,AssetExportFormat::Glb).get();prepared_normal_consumer::Model stored_toon(shared_file);fbo.bind();consumer.render(stored_toon.data);check(delta(shared_pixels,read())<=3&&shared.copy_scene().active().material.toon,"Core-PBR view ignores stored toon lighting without rewriting that authored style");
 shared.configure_selection(false);ready(shared);check(shared.begin_native_stroke(),"Real native stroke captures the accepted PBR preview policy");check(rejects([&]{shared.authored_shading();})&&shared.frame().state->pbr_view,"An active gesture rejects mode changes without mutating its captured material policy");shared.end_native_stroke(false);check(shared.wait_native_stroke(),"Cancelled native preview-policy lease retires normally");
 {
  Document asset;asset.replace(scene);ready(asset);auto gate=std::make_shared<AssetGate>();AssetRelease release{gate};const auto lease=asset.draft_scene().source;
  asset.edit_scene("Mode-safe accepted asset",[gate](Scene &next,const auto &cancelled){gate->stop();if(cancelled())throw std::runtime_error("Preview retired an unchanged authoring source");next.active().name="Mode-safe accepted asset";},lease);gate->wait();
  auto saved=asset.save_async(path.string()+"-accepted.dsculpt");const auto source_revision=asset.frame().state->revision;asset.shading(false);ready(asset);
  check(asset.frame().state->asset_busy&&asset.frame().state->revision==source_revision&&asset.frame().state->pbr_view,"Actual PBR view leaves a queued asset and dependent Save on the same authoring lease");
  gate->release();saved.get();check(asset.wait_asset()&&asset.frame().state->error.empty()&&Scene::load(path.string()+"-accepted.dsculpt").active().name=="Mode-safe accepted asset","Accepted asset and queued Save succeed after a real view-mode change");
  check(asset.undo()&&asset.copy_scene().encode()==original,"View change adds no hidden history around the asset's exact single Undo");
 }
 {
  auto png=Image{1,1,{255,0,0,255}}.png();const auto file=path.string()+"-projection.png";{std::ofstream out(file,std::ios::binary);out.write(reinterpret_cast<const char*>(png.data()),png.size());}
  auto load=[&](Document &d){ProjectionCamera c;c.pose.position={-.3f,0,2};c.width=c.height=2;d.load_projection_image(file,c);if(!d.wait_projection_image()||!d.frame().state->projection->image)throw std::runtime_error("Real projection image did not load");};
  auto stroke=[&](Document &d){if(!d.begin_native_stroke())throw std::runtime_error("Projection role stroke admission failed");NativeStrokeSample s;s.origin={-.3f,0,1};s.direction={0,0,-1};s.brush.radius=.18f;s.brush.blend=0;d.sample_native_stroke(s);d.end_native_stroke(true);if(!d.wait_native_stroke()||!d.wait_mesh()||!d.frame().state->error.empty())throw std::runtime_error("Projection role stroke failed");};
  for(bool pbr:{false,true}){
   Document d;d.replace(scene);ready(d);load(d);if(pbr){d.shading(false);ready(d);}stroke(d);const auto coverage=d.copy_scene().active().painted->pixel(1,19,32).data[9];check((coverage>200)==pbr,"Active projected target query captures the actual authored/PBR alpha role");
  }
  for(bool cached:{false,true})for(bool pbr:{false,true}){
   auto occluded=scene;occluded.active().transform.position.z=.5f;auto id=occluded.add("Plain projected target",Volume{});auto &target=occluded.object(id);target.mesh_active=true;target.painted=std::make_shared<PaintedMesh>(quad(),64);occluded.select(id);Document d;d.replace(occluded);ready(d);if(cached){d.configure_object_grab(true,false);ready(d);}load(d);if(pbr){d.shading(false);ready(d);}check(bool(d.frame().surface->picking)==cached,"Actual projection test exercises both immutable picking cache and fallback query construction");stroke(d);const auto coverage=d.copy_scene().active().painted->pixel(1,19,32).data[9];check((coverage>200)==!pbr,"Other-object projected occlusion captures the actual immutable authored/PBR alpha role");
  }
  Document placed;placed.replace(scene);ready(placed);load(placed);placed.projection_action(SpatialCommand::ProjectionPlace);const auto before=placed.copy_scene().encode();check(placed.frame().state->projection->placing&&!placed.frame().state->can_preview&&rejects([&]{placed.shading(false);})&&!placed.frame().state->pbr_view,"Physical projector placement owns its gesture and blocks preview changes atomically");placed.projection_action(SpatialCommand::ProjectionPlace);check(!placed.frame().state->projection->placing&&placed.frame().state->can_preview&&placed.copy_scene().encode()==before,"Finishing placement restores material preview availability without source mutation");
 }
 {
  Document raced;raced.replace(scene);ready(raced);auto gate=std::make_shared<AssetGate>();AssetRelease release{gate};mesh_gate=gate;
  raced.shading(false);gate->wait();raced.authored_shading();
  check(!raced.wait_mesh(std::chrono::milliseconds(10)),"A previous mesh endpoint cannot satisfy a newer view request while the real worker is gated");
  check(!raced.begin_native_stroke(),"Fresh native paint refuses a published-role and displayed-epoch mismatch");
  Driver hand(raced);hand.frame.hands[1].tracked.grip.position=ob::transform(raced.frame().state->canvas,{0,0,0});hand.send(0,0,false,.8f);
  check(raced.copy_scene().encode()==original,"Actual spatial pressure cannot paint through a pending material-role endpoint");
  gate->release();ready(raced);check(!raced.frame().surface->pbr_view&&raced.frame().surface->view_epoch==raced.frame().state->view_epoch&&direction(raced.frame().surface->source_normals,{.6f,0,.8f}),"Late old-role worker completion cannot overwrite the newest authored frame endpoint");
  hand.send(0,0,false,.8f);check(raced.copy_scene().encode()==original,"Held pressure stays fenced when the new endpoint arrives");
  hand.send();hand.send(0,0,false,.8f);hand.send();check(raced.copy_scene().encode()!=original&&raced.undo()&&raced.copy_scene().encode()==original,"A real physical release rearms spatial paint and retains its exact one-step Undo");
 }
 {
  Document measured;measured.replace(scene);ready(measured);preview_fault::calls=0;preview_fault::fail=SIZE_MAX;preview_fault::enabled=true;measured.shading(false);preview_fault::enabled=false;const auto count=preview_fault::calls;ready(measured);
  check(count>10,"Actual preview admission and publication allocations are observed");
  for(size_t at=1;at<=count;++at){
   Document denied;denied.replace(scene);ready(denied);auto before=denied.frame();preview_fault::calls=0;preview_fault::fail=at;preview_fault::hit=false;preview_fault::enabled=true;bool caught=false;
   try{denied.shading(false);}catch(const std::bad_alloc&){caught=true;}preview_fault::enabled=false;
   if(!caught||!preview_fault::hit||denied.frame().state!=before.state||denied.frame().surface!=before.surface||denied.copy_scene().encode()!=original)throw std::runtime_error("Preview allocation failure did not restore the exact publication/source endpoint");
   denied.shading(false);if(!denied.wait_mesh()||!denied.frame().surface->pbr_view)throw std::runtime_error("Preview retry after allocation failure failed");
  }
  std::cout<<"PASS "<<count<<" persistent preview allocation boundaries preserve publication, history, source and retry\n";++checks;
 }
 std::cout<<"PASS "<<checks<<" authored/core-PBR preview checks\n";return 0;
}catch(const std::exception &e){preview_fault::enabled=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
