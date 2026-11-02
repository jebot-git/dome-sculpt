// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_mtoon_render_suite
#include "mtoon-render.cpp"
#undef main
#include "normal-preparation-fixtures.hpp"
#include "colored-vrm-bake-fixture.hpp"
#include "prepared-normal-consumer.hpp"

namespace {
void settled(Document &document) {
  if (!document.wait_asset() || !document.wait_mesh() ||
      !document.frame().state->error.empty())
    throw std::runtime_error("Normal UV authoring: " + document.frame().state->error);
}

Scene prepared(bool typed) {
  auto scene = typed ? colored_vrm_fixture::model(true, true, true)
                     : normal_preparation_fixture::model();
  ObjectBakeSettings settings;
  settings.prepare_normals = true;
  settings.prepare_colors = typed;
  settings.front_mm = settings.back_mm = 500;
  settings.ao_samples = 0;
  settings.padding = 4;
  settings.allow_partial = true;
  Document document;
  document.replace(scene);
  document.bake_from_object(scene.objects().back().id, settings);
  settled(document);
  scene = document.copy_scene();
  if (!typed) {
    scene.active().transform.position = {-.6f, -.6f, 0};
    scene.active().transform.scale = 1.2f;
  } else {
    // Avoid a fully saturated toon band: the sensitivity oracle must measure
    // the projected normal, rather than identical bright-band pixels.
    auto binding = std::make_shared<SourceMaterialBindings>(*scene.active().material_bindings);
    binding->material.parameters.toony = 0;
    binding->material.parameters.shift = .05f;
    scene.active().material_bindings = std::move(binding);
  }
  return scene;
}

Scene transfer(const Scene &source, const std::filesystem::path &project) {
  Document document;
  document.replace(source);
  settled(document);
  document.begin_uv();
  if (!document.wait_uv()) throw std::runtime_error("Normal UV draft timeout");
  document.edit_uv(UvAction::Scale, .7f);
  if (!document.wait_uv()) throw std::runtime_error("Normal UV scale timeout");
  document.end_uv(true);
  settled(document);
  const auto current = document.copy_scene();
  check(current.active().material_bindings->normal_preparation->uv_reprojection &&
            current.active().painted->baked(),
        "Actual UV Apply retains both intrinsic prepared planes and baked normal detail");
  check(current.active().material_bindings->normal_frames ==
            source.active().material_bindings->normal_frames,
        "Actual UV Apply changes sampling without rewriting original directional attributes");
  document.save(project);
  Document reopened;
  reopened.open(project);
  settled(reopened);
  check(reopened.copy_scene().encode() == current.encode(),
        "Independent export uses a newly reopened complete Scene19 source model");
  return reopened.copy_scene();
}

void posed(Scene &scene, int mode, float weight) {
  auto deformation = *scene.active().deformation;
  deformation.morph_weights.assign(deformation.morphs->targets.size(), weight);
  deformation.pose.clear();
  for (const auto &bone : deformation.skeleton->bones)
    deformation.pose.push_back(bone.rest);
  if (mode) {
    deformation.pose[0].scale = mode == 1 ? Vec3{1.1f, .85f, .65f}
                                        : Vec3{.85f, 1.05f, 1.4f};
    const float angle = mode == 1 ? .2f : -.25f;
    deformation.pose[1].rotation = {std::cos(angle / 2), 0, std::sin(angle / 2), 0};
    deformation.pose[1].scale = mode == 1 ? Vec3{.9f, 1.1f, 1.4f}
                                        : Vec3{1.15f, .9f, .8f};
  }
  scene.set_deformation(scene.active().id, deformation);
}

void consumers(const Scene &scene, bool typed, const std::filesystem::path &prefix,
               SurfaceRenderer &native, Fbo &framebuffer,
               prepared_normal_consumer::Renderer &independent) {
  for (int mode = 0; mode < 3; ++mode) {
    for (float weight : {0.f, .35f, 1.f}) {
      auto current = scene;
      posed(current, mode, weight);
      Document document;
      document.replace(current);
      settled(document);
      auto path = prefix;
      path += "-p" + std::to_string(mode) + "-w" +
              std::to_string(int(weight * 100)) + (typed ? ".vrm" : ".glb");
      document.export_asset(path, typed ? AssetExportFormat::Avatar
                                       : AssetExportFormat::Glb).get();
      prepared_normal_consumer::Model file(path);
      const auto expected = draw(native, document.frame().surface);
      framebuffer.bind();
      independent.render(file.data, typed ? &file.json.at("extensions").at("VRM")
                                          : nullptr);
      const auto actual = read();
      size_t covered = 0;
      for (size_t n = 3; n < actual.size(); n += 4) covered += actual[n] > 0;
      const auto error = delta(expected, actual);
      std::cout << "NORMAL_UV_CONSUMER typed=" << typed << " pose=" << mode
                << " weight=" << weight << " maxerror=" << error
                << " covered=" << covered << '\n';
      check(covered > 1000 && error <= 4,
            "Independent parsed file normal-map pixels match native UV-projected attributes through morph and anisotropic mixed skin");
      if (!mode && weight == 0 && !typed) {
        const auto texture = file.data->meshes[0].primitives[0].material->normal_texture.texture;
        file.data->meshes[0].primitives[0].material->normal_texture.texture = nullptr;
        framebuffer.bind();
        independent.render(file.data);
        check(delta(actual, read()) > 2,
              "Independent exported pixels visibly depend on actual UV-projected normal detail");
        file.data->meshes[0].primitives[0].material->normal_texture.texture = texture;
      }
      if (typed) {
        if (!mode && weight == 0) {
          auto &vrm = file.json.at("extensions").at("VRM");
          auto &strength = vrm.at("materialProperties")[0].at("floatProperties").at("_BumpScale");
          const float old = strength.get<float>();
          strength = 0.f;
          framebuffer.bind();
          independent.render(file.data, &vrm);
          check(delta(actual, read()) > 2,
                "Independent legacy MToon pixels visibly depend on the projected normal plane");
          strength = old;
        }
        auto fallback = current;
        auto binding = std::make_shared<SourceMaterialBindings>(*fallback.active().material_bindings);
        binding->has_mtoon = false;
        binding->roles = {};
        fallback.active().material_bindings = binding;
        fallback.active().material.toon = false;
        Document pbr;
        pbr.replace(fallback);
        settled(pbr);
        const auto expected_pbr = draw(native, pbr.frame().surface);
        framebuffer.bind();
        independent.render(file.data);
        const auto pbr_error = delta(expected_pbr, read());
        std::cout << "NORMAL_UV_VRM_PBR pose=" << mode << " weight=" << weight
                  << " maxerror=" << pbr_error << '\n';
        check(expected_pbr != expected && pbr_error <= 4,
              "The same UV-projected VRM preserves distinct core PBR and legacy raw-TBN normal evaluation");
      }
    }
  }
}
} // namespace

#ifndef DOME_NORMAL_UV_RENDER_NO_MAIN
int main(int argc, char **argv) try {
  if (argc < 2) return 2;
  const std::filesystem::path prefix = argv[1];
  std::filesystem::create_directories(prefix.parent_path());
  NativeGraphics graphics;
  const std::string renderer = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
  std::cout << "GPU " << renderer << '\n';
  check(argc < 3 || std::string(argv[2]) != "--hardware" ||
            renderer.find("A770") != std::string::npos,
        "Independent normal UV consumers use the requested actual graphics device");
  Fbo framebuffer;
  framebuffer.bind();
  SurfaceRenderer native;
  prepared_normal_consumer::Renderer independent;
  for (bool typed : {false, true}) {
    auto original = prepared(typed);
    const auto exact = original.encode();
    auto name = prefix.string() + (typed ? "-typed" : "-pbr");
    auto current = transfer(original, name + "-first.dsculpt");
    current = transfer(current, name + "-repeat.dsculpt");
    check(bool(current.active().material_bindings->normal_preparation->uv_reprojection
                  ->before_material->normal_preparation->uv_reprojection),
          "Repeated UV authoring retains both exact replay steps instead of collapsing resampling history");
    consumers(current, typed, name, native, framebuffer, independent);
    check(original.encode() == exact,
          "Native UV authoring and all independent consumers preserve the complete original source");
  }
  std::cout << "PASS " << checks << " normal UV graphical checks\n";
  return 0;
} catch (const std::exception &error) {
  std::cerr << "FAIL " << error.what() << '\n';
  return 1;
}
#endif
