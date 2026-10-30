// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include <nlohmann/json.hpp>
#include <atomic>
#include <condition_variable>
#include <iostream>
#include <unistd.h>
using namespace dome;
namespace {
std::atomic<unsigned> extractions{0};
std::atomic_bool gate_sync{false};std::mutex sync_mutex;std::condition_variable sync_changed;
bool sync_entered=false,sync_released=false;
void arm_sync() {std::lock_guard lock(sync_mutex);sync_entered=sync_released=false;gate_sync=true;}
void await_sync() {std::unique_lock lock(sync_mutex);if(!sync_changed.wait_for(lock,std::chrono::seconds(5),[]{return sync_entered;}))throw std::runtime_error("Save worker did not reach fsync gate");}
void release_sync() {std::lock_guard lock(sync_mutex);sync_released=true;sync_changed.notify_all();}

void require(bool value,const char *message) { if(!value) throw std::runtime_error(message); }
template<class F> void refuses(F call,const char *message) { bool rejected=false;try{call();}catch(const std::exception &){rejected=true;}require(rejected,message); }
AssetSource lease(const Document &d) { auto s=d.frame().state;return {s->generation,s->revision,s->active_object}; }
void mesh(Document &d) { require(d.wait_mesh(),"current immutable surface ready");require(d.frame().state->error.empty(),"no asynchronous surface error"); }
Volume sphere() { Volume v(.05f);v.begin_stroke();Brush b;b.radius=.23f;b.blend=0;v.stamp(b);v.end_stroke();return v.geometry_snapshot(); }
}
extern "C" Mesh real_surface(const Volume &,const std::function<bool()> &) asm("__real__ZN4dome12surface_meshERKNS_6VolumeERKSt8functionIFbvEE");
extern "C" Mesh wrapped_surface(const Volume &,const std::function<bool()> &) asm("__wrap__ZN4dome12surface_meshERKNS_6VolumeERKSt8functionIFbvEE");
extern "C" Mesh wrapped_surface(const Volume &v,const std::function<bool()> &cancelled) { ++extractions;return real_surface(v,cancelled); }
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd) {
  if(gate_sync.exchange(false)) {
    std::unique_lock lock(sync_mutex);sync_entered=true;sync_changed.notify_all();
    if(!sync_changed.wait_for(lock,std::chrono::seconds(8),[]{return sync_released;}))return -1;
  }
  return __real_fsync(fd);
}
int main(int argc,char **argv) {
 try {
  if(argc==3 && std::string(argv[1])=="--ui-fixture") {
    Scene scene;scene.active().volume=std::make_shared<Volume>(sphere());
    auto settings=scene.active().volume->sculpt_settings();settings.symmetry_origin={.01234567f,-.02345678f,.03456789f};
    scene.active().volume->sculpt_settings(settings);scene.save(argv[2]);return 0;
  }
  if(argc==3 && std::string(argv[1])=="--inspect") {std::cout<<Scene::load(argv[2]).encode().dump()<<'\n';return 0;}

  Document d;d.replace(sphere());mesh(d);auto clean=d.frame().state;
  auto settings=clean->sculpt_settings;
  d.sculpt_settings(settings,lease(d));
  require(d.frame().state->revision==clean->revision && !d.frame().state->dirty && !d.frame().state->can_undo,"identical controls are exact no-op");
  const auto original=d.copy_volume().voxels();const auto baseline_extractions=extractions.load();
  settings.symmetry_axes=5;settings.symmetry_origin={.01f,-.02f,.03f};settings.mask_edit=true;
  const auto stale=lease(d);d.sculpt_settings(settings,stale);mesh(d);
  require(extractions==baseline_extractions,"controls retain cached COW source geometry");
  require(d.frame().state->can_undo && d.frame().state->sculpt_settings.mask_edit,"native controls one history record");
  refuses([&]{d.sculpt_settings(settings,stale);},"stale native control lease refused");
  refuses([&]{d.sculpt_mask_fill(0,stale);},"stale bulk mask lease refused before queueing");
  require(d.undo(),"undo controls");mesh(d);require(d.frame().state->sculpt_settings.symmetry_axes==0 && !d.frame().state->can_undo,"controls undo as single transaction");
  require(d.redo(),"redo controls");mesh(d);
  require(d.begin_desktop(),"mask stroke starts");
  refuses([&]{d.sculpt_settings(settings);},"controls cannot mutate live stroke");
  Brush b;b.radius=.18f;b.from=b.to={0,0,.19f};d.stamp_desktop(b);
  require(d.copy_volume().pending_changes()==false,"detached snapshot drops pending bookkeeping");
  d.cancel_desktop();mesh(d);require(d.copy_volume().mask_values().empty(),"cancel retains exact sparse protection");
  require(d.begin_desktop(),"mask stroke restart");d.stamp_desktop(b);d.end_desktop();mesh(d);
  require(!d.copy_volume().mask_values().empty() && d.copy_volume().voxels()==original,"mask stroke changes protection only");
  require(extractions==baseline_extractions,"mask stroke does not extract source geometry again");
  auto snapshot=d.frame().surface;require(snapshot->sculpt_masks.size()==snapshot->mesh.positions.size(),"immutable overlay matches packed vertices");
  bool protected_vertex=false;
  auto volume=d.copy_volume();for(size_t i=0;i<snapshot->mesh.positions.size();++i) {
    float expected=volume.mask_sample(snapshot->mesh.positions[i]);
    require(std::abs(snapshot->sculpt_masks[i]-expected)<1e-6f,"overlay samples local persisted protection");protected_vertex|=expected>0;
  }
  require(protected_vertex,"visible surface protection feedback");
  auto before_fill=d.copy_volume();d.sculpt_mask_fill(1,lease(d));require(d.wait_asset(),"background Fill completes");mesh(d);
  require(d.copy_volume().mask_default()==1 && d.copy_volume().mask_values().empty(),"global Fill protection persists without empty-space enumeration");
  require(d.undo(),"Fill undo");mesh(d);require(d.copy_volume().mask_values()==before_fill.mask_values() && d.copy_volume().mask_default()==0,"Fill is one exact undo");
  require(d.redo(),"Fill redo");mesh(d);
  const auto no_op=d.frame().state->revision;d.sculpt_mask_fill(1,lease(d));require(d.wait_asset(),"no-op Fill completes");mesh(d);
  require(d.frame().state->revision==no_op,"background exact no-op does not advance content revision");
  settings=d.frame().state->sculpt_settings;settings.mask_edit=false;settings.symmetry_axes=0;d.sculpt_settings(settings);mesh(d);
  auto protected_scene=d.copy_volume();require(d.begin_desktop(),"protected geometry stroke starts");b.tool=Tool::Carve;d.stamp_desktop(b);d.end_desktop();mesh(d);
  require(d.copy_volume().voxels()==protected_scene.voxels(),"all-protected geometry remains exact");
  settings.mask_visible=false;d.sculpt_settings(settings);mesh(d);require(d.frame().surface->sculpt_masks.empty(),"hidden protection emits no overlay attributes");
  auto object=d.frame().state->active_object;auto info=d.frame().state->objects.front();
  d.sculpt_layer_properties(object,d.copy_volume().active_layer(),"Protected layer",true,true,1);mesh(d);
  settings.mask_edit=true;d.sculpt_settings(settings);mesh(d);require(d.begin_desktop(),"mask editing remains available on locked geometry layer");d.cancel_desktop();
  settings.mask_edit=false;d.sculpt_settings(settings);require(!d.begin_desktop(),"geometry still respects locked sculpt layer");
  d.object_properties(object,info.name,true,true,info.transform,info.material);mesh(d);
  refuses([&]{d.sculpt_settings(settings);},"object lock protects controls");refuses([&]{d.sculpt_mask_invert();},"object lock protects bulk mask operation");require(!d.begin_desktop(),"object lock protects mask strokes");
  // Hold a real ordered save at fsync so cancellation/generation boundaries
  // precede the mask worker deterministically, independent of machine speed.
  {
    struct Release {~Release(){release_sync();}} release;
    Document work;auto v=sphere();v.fill_mask(.37f);work.replace(std::move(v));mesh(work);
    auto save_path=std::filesystem::temp_directory_path()/("dome-mask-worker-"+std::to_string(getpid())+".dsculpt");
    arm_sync();auto saved=work.save_async(save_path);await_sync();
    work.sculpt_mask_fill(0,lease(work));require(work.asset_status().cancellable,"mask worker exposes cancellation");
    work.cancel_asset_work();release_sync();saved.get();require(work.wait_asset(),"cancelled queued mask operation drains");
    require(work.copy_volume().mask_default()==.37f && !work.frame().state->can_undo,"cancelled mask publishes no data or history");
    arm_sync();saved=work.save_async(save_path);await_sync();
    work.sculpt_mask_fill(0,lease(work));work.replace(sphere());
    auto replacement=work.save_async(save_path.string()+".new");
    release_sync();saved.get();replacement.get();
    require(work.copy_volume().mask_default()==0 && !work.frame().state->can_undo,"old-generation mask work cannot publish into a replacement document");
    std::filesystem::remove(save_path);std::filesystem::remove(save_path.string()+".new");
  }
  // Flat Volume3 is accepted through the real asynchronous native project loader.
  auto path=std::filesystem::temp_directory_path()/("dome-mask-flat-"+std::to_string(getpid())+".dsculpt");
  auto flat=before_fill.geometry_snapshot();flat.save(path);Document reopened;reopened.open(path);mesh(reopened);std::filesystem::remove(path);
  require(reopened.copy_volume().mask_values()==flat.mask_values() && reopened.copy_volume().sculpt_settings().symmetry_axes==5,"flat Volume3 restores through native Open");
  auto nested=d.copy_scene().encode();require(nested.at("version")==5 && nested.at("objects")[0].at("volume").at("version")==3,"ordinary Scene preserves independent nested Volume3 without unnecessary schema promotion");
  auto decoded=Scene::decode(nested);require(decoded.active().volume->mask_default()==1,"nested mask persistence");
  nested["version"]=8;
  nested["source_materials"]={{"version",1},{"paints",nlohmann::json::array()},{"origins",nlohmann::json::array()},{"bindings",nlohmann::json::array()}};
  for(auto &o:nested["objects"]) {o["material_bindings"]=nullptr;o["retopology"]=nullptr;}
  require(Scene::decode(nested).active().volume->mask_default()==1,"combined Scene8 accepts independent nested Volume3");
  nested["version"]=7;for(auto &o:nested["objects"])o.erase("retopology");
  require(Scene::decode(nested).active().volume->mask_default()==1,"legacy material Scene7 accepts nested Volume3");
  // Null entries deliberately remain invalid; the aggregate storage rejection
  // must happen before decoding either object's entries or allocating lattices.
  auto oversized=nested;oversized["objects"].push_back(oversized["objects"][0]);
  oversized["objects"][1]["id"]=2;oversized["next_id"]=3;
  for(auto &o:oversized["objects"]) {
    auto &a=o["volume"]["sculpt_mask"]["values"];a=nlohmann::json::array();
    a.get_ref<nlohmann::json::array_t&>().resize(2'000'001);
  }
  bool quota=false;try{Scene::decode(oversized);}catch(const std::length_error &e){quota=std::string(e.what())=="Scene sculpt storage limit exceeded";}
  require(quota,"aggregate mask storage preflight precedes per-object decoding");
  std::cout<<"PASS native masks, exact history/no-op/leases, locked-layer policy, immutable overlay, COW extraction reuse and flat/nested persistence\n";
 } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
