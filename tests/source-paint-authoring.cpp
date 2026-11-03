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
namespace {
std::vector<uint8_t> file_bytes(const std::filesystem::path &path) {
  std::ifstream stream(path,std::ios::binary);
  return {std::istreambuf_iterator<char>(stream),{}};
}
void complete(std::future<void> work) { work.get(); }
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
    }
    check(rejects([&]{imported.scene.export_surfaces();}) && imported.scene.encode()==before,
          "Strict ordinary export retains its explicit painted-source boundary");
  }
  std::cout<<"PASS "<<checks<<" actual source-painted authoring container checks\n";
  return 0;
} catch(const std::exception &error) {
  std::cerr<<"FAIL "<<error.what()<<'\n';return 1;
}
