// SPDX-License-Identifier: GPL-3.0-or-later
#define main original_material_renderer_suite
#include "mtoon-render.cpp"
#undef main
#include "dome/import-authoring.hpp"
#include "dome/vrm.hpp"
#include "vrm-role-consumer.hpp"
#include "vrm-rigid-fixtures.hpp"
#include "dome/rig-edit.hpp"
#include "dome/rig-rest.hpp"
#include <cstring>
#include "source-rest-graph.hpp"
#include "vrm-rigid-rest-fixtures.hpp"
#include "dome/source-placement.hpp"
#include <unistd.h>
int main(int argc, char **argv) try {
  if (argc < 4)
    return 2;
  auto home = std::filesystem::temp_directory_path() /
              ("dome-vrm-reparent-export-render-" + std::to_string(getpid()));
  std::filesystem::create_directories(home);
  setenv("HOME", home.c_str(), 1);
  setenv("XDG_STATE_HOME", home.c_str(), 1);
  setenv("XDG_CONFIG_HOME", home.c_str(), 1);
  struct Cleanup {
    std::filesystem::path p;
    ~Cleanup() {
      std::error_code ec;
      std::filesystem::remove_all(p, ec);
    }
  } cleanup{home};
  std::filesystem::create_directories(argv[3]);
  NativeGraphics graphics;
  const std::string device =
      reinterpret_cast<const char *>(glGetString(GL_RENDERER));
  std::cout << "GL_VENDOR " << glGetString(GL_VENDOR) << "\nGL_RENDERER "
            << device << std::endl;
  check(argc < 5 || std::string_view(argv[4]) != "--hardware" ||
            (device.find("Intel") != std::string::npos &&
             device.find("Arc") != std::string::npos &&
             device.find("llvmpipe")==std::string::npos &&
             device.find("UHD")==std::string::npos &&
             device.find("A770") != std::string::npos),
        "Graphics context satisfies explicit Arc A770 request when enabled");
  Fbo framebuffer;
  framebuffer.bind();
  SurfaceRenderer renderer;
  role_consumer::Renderer independent;
  for (int model = 1; model <= 2; ++model)
    for (int mode : {0, 2}) {
      auto source = std::filesystem::path(argv[3]) /
                    ("input-" + std::to_string(model) + "-" +
                     std::to_string(mode) + ".vrm");
      auto fitted = home / "placed.vrm";
      vrm_source_fixture::source_variant(argv[model], fitted, mode);
      auto badges=home/"badges.vrm";auto rigid_nodes = vrm_source_fixture::with_rigid_accessories(fitted,badges);const auto spacer=vrm_rest_fixture::with_joint_mesh(badges,source);
      ImportAuthoringOptions options;
      options.paint_resolution = 64;
      auto prepared = prepare_authoring_import(Scene{}, source, options);
      for (float weight : {0.f, .65f}) {
        auto scene = prepared.scene;
        if(weight>0) {
          auto active=prepared.objects.front();
          RigEdit edit;edit.joint=JointAdjustment{
            scene.object(active).deformation->humanoid->joints.at("head"),{0,0,17},{}};
          edit_rig(scene,active,edit);
        }
        uint64_t attached=0,head_rigid=0;
        for(auto id:prepared.objects) {
          const auto &o=scene.object(id);
          if(o.appearance->source_node==int(rigid_nodes.moving))attached=id;
          if(o.appearance->source_node==int(rigid_nodes.head)&&(!o.deformation||!o.deformation->skin))head_rigid=id;
          if(o.deformation && o.deformation->morphs) {
            auto d=*o.deformation;
            d.morph_weights.assign(d.morphs->targets.size(),0);
            d.morph_weights[0]=weight;
            scene.set_deformation(id,std::move(d));
          }
        }
        if(mode==2) {
          if(!head_rigid)throw std::runtime_error("Actual mapped-joint rigid source missing");
          const auto identity=detail::source_rest_identity(scene,scene.object(prepared.objects.front()));scene.group(identity->source_instance).affine_edit=std::make_shared<const NativeAffineEdit>(AffineTransform(std::array<double,16>{-1.1,0,0,0,.2,.9,0,0,.1,.05,1.2,0,.3,-.1,.2,1}));const auto tail=scene.add_group("Native rigid child offset",{{.04f,-.02f,.03f},{0,0,std::sin(.08f),std::cos(.08f)},1.1f},scene.object(head_rigid).group);scene.object(head_rigid).group=tail;
          auto &o=scene.object(attached);
          auto previous=o.affine_edit?o.affine_edit->matrix():AffineTransform{};
          AffineTransform edit(std::array<double,16>{-1.,.12,0,0,.25,1.1,0,0,0,0,.85,0,.04,-.02,.01,1});
          o.affine_edit=std::make_shared<const NativeAffineEdit>(previous*edit);
        }
        const auto selected=prepared.objects.front();
        const auto head=scene.object(selected).deformation->humanoid->joints.at("head");
        RigRestJointEdit rest;rest.joint=head;rest.world_length=.137f;const auto&joints=scene.object(selected).appearance->source_joint_nodes;const auto aux=std::find(joints.begin(),joints.end(),spacer);if(aux==joints.end())throw std::runtime_error("Genuine source parent spacer is missing");rest.parent=int(aux-joints.begin());
        const auto current=(scene.world_matrix(selected)*AffineTransform(scene.object(selected).deformation->skeleton->world()[head])).point({0,0,0});
        rest.world_position=Vec3{float(current[0]+.005),float(current[1]-.002),float(current[2]+.001)};
        author_rig_rest(scene,selected,{&rest,1});
        Document document;
        document.replace(scene);
        check(document.wait_mesh(std::chrono::seconds(30)) &&
                  document.frame().state->error.empty(),
              "Actual mixed source skins and rigid accessories reach native renderer");
        auto path =
            std::filesystem::path(argv[3]) /
            ("output-" + std::to_string(model) + "-" + std::to_string(mode) +
             "-" + std::to_string(weight) + ".vrm");
        export_vrm(scene.preview_surfaces(), path);
        role_consumer::Model file(path);
        auto native = draw(renderer, document.frame().surface);
        framebuffer.bind();
        independent.render(file.data, &file.json["extensions"]["VRM"]);
        int error = delta(native, read());
        std::cout << "VRM_BONE_LENGTH_EXPORT_PIXEL model=" << model << " mode=" << mode
                  << " morph=" << weight << " maxError=" << error << std::endl;
        check(error <= 2,
              "Emitted original-space skin/morph/MToon under "
              "ordinary/reflected-mesh/sheared-source chains with rigid affine edits matches actual "
              "native pixels at unchanged tolerance2");
        if(model==1&&mode==2&&weight>0){
          role_consumer::Model original(source);
          auto head_id=[](const nlohmann::json &j){for(const auto&bone:j.at("extensions").at("VRM").at("humanoid").at("humanBones"))if(bone.at("bone").get_ref<const std::string&>()=="head")return bone.at("node").get<size_t>();throw std::runtime_error("Missing actual head identity");};
          auto original_head=head_id(original.json),emitted_head=head_id(file.json);size_t replaced=0;
          for(size_t skin=0;skin<file.data->skins_count;++skin){auto &current_skin=file.data->skins[skin];for(size_t slot=0;slot<current_skin.joints_count;++slot)if(current_skin.joints[slot]==file.data->nodes+emitted_head){
            float old[16];bool found=false;
            for(size_t os=0;os<original.data->skins_count&&!found;++os)for(size_t oj=0;oj<original.data->skins[os].joints_count;++oj)if(original.data->skins[os].joints[oj]==original.data->nodes+original_head){found=cgltf_accessor_read_float(original.data->skins[os].inverse_bind_matrices,oj,old,16);break;}
            if(!found)throw std::runtime_error("Independent wrong-bind control has no original authored head IBM");
            const auto *a=current_skin.inverse_bind_matrices;
            auto *target=static_cast<unsigned char*>(a->buffer_view->buffer->data)+a->buffer_view->offset+a->offset+slot*a->stride;
            std::memcpy(target,old,sizeof(old));++replaced;
          }}
          framebuffer.bind();independent.render(file.data,&file.json["extensions"]["VRM"]);
          check(replaced>0&&delta(native,read())>2,"Independent image oracle rejects retaining the original head IBM after a genuine native REST fit");
        }
        auto without_rigid=scene;
        for(auto id:prepared.objects)
          if(!without_rigid.object(id).deformation || !without_rigid.object(id).deformation->skin)
            without_rigid.object(id).visible=false;
        Document body_only;body_only.replace(without_rigid);
        check(body_only.wait_mesh(std::chrono::seconds(30)) &&
                  delta(native,draw(renderer,body_only.frame().surface))>5,
              "New rigid source accessories independently contribute visible native pixels");
        size_t visible = 0;
        for (size_t p = 3; p < native.size(); p += 4)
          visible += native[p] != 0;
        check(visible > 100, "Independent emitted avatar comparison has "
                             "visible exterior geometry");
      }
    }
  std::cout
      << checks
      << " native reparented mapped-joint rigid/source VRM independent graphics checks passed\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
