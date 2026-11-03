// SPDX-License-Identifier: GPL-3.0-or-later
#define main original_mtoon_fixture
#include "mtoon-render.cpp"
#undef main
#include "dome/native-authoring.hpp"
#include "dome/scene-file.hpp"
#include "dome/source-paint-bake.hpp"
#include "source-color-consumer.hpp"
#include "prepared-normal-consumer.hpp"
#include "source-paint-bake-fixtures.hpp"
#include <fstream>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <sys/stat.h>
namespace file_gate {
std::mutex mutex;std::condition_variable changed;
bool entered=false,released=false;std::atomic<bool> armed=false;
void arm(){std::lock_guard lock(mutex);entered=released=false;armed=true;}
void release(){std::lock_guard lock(mutex);released=true;changed.notify_all();}
bool wait(){std::unique_lock lock(mutex);return changed.wait_for(lock,std::chrono::seconds(30),[]{return entered;});}
struct Guard {~Guard(){release();}};
}
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd) {
  const auto result=__real_fsync(fd);struct stat status{};
  if(!result && !fstat(fd,&status) && S_ISREG(status.st_mode) && file_gate::armed.exchange(false)) {
    std::unique_lock lock(file_gate::mutex);file_gate::entered=true;file_gate::changed.notify_all();
    file_gate::changed.wait(lock,[]{return file_gate::released;});
  }
  return result;
}
namespace {
std::vector<uint8_t> file_bytes(const std::filesystem::path &path) {
  std::ifstream stream(path,std::ios::binary);
  return {std::istreambuf_iterator<char>(stream),{}};
}
void complete(std::future<void> work) { work.get(); }
bool cancelled(std::future<void> work) {
  if(work.wait_for(std::chrono::seconds(120))!=std::future_status::ready)
    throw std::runtime_error("Native painted export cancellation timed out");
  try{work.get();}catch(const std::exception &error){return std::string_view(error.what()).find("cancelled")!=std::string_view::npos;}
  return false;
}
void seed(const std::filesystem::path &path){std::ofstream stream(path,std::ios::binary);stream<<"Original retained output";}
}
int main(int argc,char **argv) try {
  if(argc<2 || argc>3)return 2;
  const std::filesystem::path out=argv[1];
  std::filesystem::create_directories(out);
  NativeGraphics graphics;
  const std::string vendor=reinterpret_cast<const char*>(glGetString(GL_VENDOR));
  const std::string device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));
  std::cout<<"GL_VENDOR="<<vendor<<" GL_RENDERER="<<device<<'\n';
  check(argc<3 || std::string_view(argv[2])!="--hardware" ||
        (vendor.find("Intel")!=std::string::npos && device.find("Arc")!=std::string::npos && device.find("A770")!=std::string::npos),
        "Requested native painted-container proof requires actual A770");
  Fbo fbo;fbo.bind();SurfaceRenderer native;
  native.set_frame_time(0);
  color_consumer::Renderer independent;
  prepared_normal_consumer::Renderer all_roles;
  Scene initial;initial.active().visible=false;
  ImportAuthoringOptions import;
  import.prepare_texture_atlas=true;import.paint_resolution=64;
  for(int kind=0;kind<8;++kind) {
    auto fixture=portable_material_fixture(kind);
    const auto input_path=out/("input"+std::to_string(kind)+".glb");
    fixture.save(input_path);
    auto imported=prepare_authoring_import(initial,input_path,import);
    ink(imported.scene);
    const auto before=imported.scene.encode();
    const auto active=imported.scene.active().id;
    const auto paint=imported.scene.active().painted;
    Document document;
    document.replace(imported.scene.snapshot());
    document.shading(false);settle(document);
    const auto reference=draw(native,document.frame().surface);
    const auto package=out/("editable"+std::to_string(kind)+".glb");
    complete(document.export_asset(package,AssetExportFormat::EditableGlb512));
    const auto bytes=file_bytes(package);
    auto recovered=extract_native_authoring(bytes);
    check(recovered && recovered->encode()==before &&
          document.copy_scene().encode()==before && paint==imported.scene.active().painted,
          "Actual native file worker embeds the exact original source, editable layers and complete project");
    check(Scene::load(package).encode()==before &&
          load_scene_file(package).kind==SceneFileKind::AuthoringPackage,
          "Painted-source container uses the real native project Open path");
    color_consumer::Model model(package);
    fbo.bind();
    if(kind==6)all_roles.render(model.data);else independent.render(model.data,nullptr,{},true);
    const auto quality=fidelity(reference,read());
    std::cout<<"SOURCE_PAINT_AUTHORING kind="<<kind<<" mean="<<quality.mean
             <<" max="<<quality.maximum<<" interior_max="<<quality.interior_maximum<<'\n';
    check(quality.visible>20 && quality.mean<=2 && quality.interior_maximum<=2,
          "Actual painted-source file output retains the existing nominal-view mean/interior quality bounds");
    Document reopened;
    complete(reopened.open_async(package));
    check(reopened.copy_scene().encode()==before && reopened.frame().state->dirty &&
          reopened.frame().state->project_path.empty(),
          "Actual package Open restores all editable source data without adopting an interchange save path");
    reopened.shading(false);settle(reopened);
    check(delta(reference,draw(native,reopened.frame().surface))==0,
          "Fresh native recovery restores exact original preview pixels independently of the portable view");
    const auto saved=out/("recovered"+std::to_string(kind)+".dsculpt");
    complete(reopened.save_async(saved));
    check(Scene::load(saved).encode()==before && file_bytes(package)==bytes,
          "Actual subsequent Save preserves the source package and exact recovered project");
    const auto cycle=out/("second"+std::to_string(kind)+".glb");
    complete(reopened.export_asset(cycle,AssetExportFormat::EditableGlb512));
    check(file_bytes(cycle)==bytes,"Native painted-package reopen/export is byte-identical on its second cycle");
    if(kind==4) {
      NativeAuthoringExport owner;
      owner.scene=std::make_shared<const Scene>(imported.scene.snapshot());
      owner.allow_portable_fallback=true;
      SourceUvBakeOptions policy;policy.resolution=512;
      for(int limit=0;limit<4;++limit) {
        auto bounded=policy;
        if(limit==0)bounded.max_working_bytes=4;
        if(limit==1)bounded.max_output_bytes=4;
        if(limit==2)bounded.max_sample_operations=4;
        if(limit==3)bounded.max_raster_samples=4;
        check(rejects([&]{export_glb_authoring(owner,package,{},bounded);}) &&
              file_bytes(package)==bytes && imported.scene.encode()==before,
              "Cumulative portable compilation quota refusal preserves the prior file and exact native owner");
      }
      const auto running_path=out/"cancel-running.glb",queued_path=out/"cancel-queued.glb",ordered_path=out/"cancel-ordered.dsculpt";
      seed(running_path);seed(queued_path);const auto prior=file_bytes(running_path);
      file_gate::Guard guard;file_gate::arm();
      auto running=document.export_asset(running_path,AssetExportFormat::EditableGlb512);
      check(file_gate::wait(),"Actual painted-package worker reaches durable temporary output before publication");
      auto queued=document.export_asset(queued_path,AssetExportFormat::EditableGlb512);
      auto saved_after=document.save_async(ordered_path);
      check(document.frame().state->pending_exports==2 && document.frame().state->can_cancel_asset,
            "Both real native painted exports expose the shared cancellation command");
      document.cancel_background_work();file_gate::release();
      check(cancelled(std::move(running)) && cancelled(std::move(queued)),
            "Shared native Cancel reaches running and queued painted-package file workers");
      complete(std::move(saved_after));
      check(file_bytes(running_path)==prior && file_bytes(queued_path)==prior &&
            Scene::load(ordered_path).encode()==before && document.copy_scene().encode()==before,
            "Painted export cancellation preserves both old files and the exact following native Save");
      complete(document.export_asset(running_path,AssetExportFormat::EditableGlb512));
      check(Scene::load(running_path).encode()==before && !document.frame().state->pending_exports,
            "Actual painted-package retry restores exact data and retires export status");
    }
    check(rejects([&]{imported.scene.export_surfaces();}) && imported.scene.encode()==before,
          "Strict ordinary export retains its explicit painted-source boundary");
  }
  {
    auto fixture=source_paint_fixture::input(0);
    Image checker{128,128,std::vector<uint8_t>(128*128*4)};
    for(size_t y=0;y<128;++y)for(size_t x=0;x<128;++x){const auto at=(y*128+x)*4;for(size_t k=0;k<3;++k)checker.rgba[at+k]=((x+y)&1)?255:0;checker.rgba[at+3]=255;}
    auto png=checker.png();while(fixture.bin.size()%4)fixture.bin.push_back(0);
    const auto view=fixture.j["bufferViews"].size();
    fixture.j["bufferViews"].push_back({{"buffer",0},{"byteOffset",fixture.bin.size()},{"byteLength",png.size()}});
    fixture.bin.insert(fixture.bin.end(),png.begin(),png.end());fixture.j["images"][0]["bufferView"]=view;
    fixture.j["nodes"][1]["scale"]={.6f,.6f,.6f};fixture.j["materials"][0]["alphaMode"]="OPAQUE";
    const auto input_path=out/"minification-input.glb";fixture.save(input_path);
    auto imported=prepare_authoring_import(initial,input_path,import);ink(imported.scene);
    const auto before=imported.scene.encode();Document document;document.replace(imported.scene.snapshot());document.shading(false);settle(document);
    const auto reference=draw(native,document.frame().surface);
    const auto package=out/"minification-editable.glb";
    complete(document.export_asset(package,AssetExportFormat::EditableGlb512));
    color_consumer::Model model(package);fbo.bind();independent.render(model.data,nullptr,{},true);
    const auto quality=fidelity(reference,read());
    std::cout<<"AUTHORING_MINIFICATION mean="<<quality.mean<<" max="<<quality.maximum<<'\n';
    check(quality.visible>0 && (quality.mean>2 || quality.interior_maximum>2),
          "Actual minified checker retains the counterexample to lossless portable sampling");
    auto restored=extract_native_authoring(file_bytes(package));
    check(restored && restored->encode()==before && document.copy_scene().encode()==before,
          "Exact native source survives even when the explicit portable view loses minified detail");
    Document original;complete(original.open_async(package));original.shading(false);settle(original);
    check(delta(reference,draw(native,original.frame().surface))==0,
          "Opening the lossy portable container restores exact original native minification pixels");
  }
  std::cout<<"PASS "<<checks<<" actual source-painted authoring container checks\n";
  return 0;
} catch(const std::exception &error) {
  std::cerr<<"FAIL "<<error.what()<<'\n';return 1;
}
