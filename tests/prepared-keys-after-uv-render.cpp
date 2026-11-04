// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_NORMAL_UV_RENDER_NO_MAIN
#include "normal-uv-render.cpp"
#undef DOME_NORMAL_UV_RENDER_NO_MAIN

namespace {
const NormalFrameTarget &key_frame(const NormalFrames &frames, uint64_t id) {
  for (const auto &target : frames.targets) if (target.id == id) return target;
  throw std::runtime_error("Missing explicit post-UV key frame");
}
Scene artist_prepared() {
  auto scene = normal_preparation_fixture::model();
  auto &object = scene.active();
  auto mesh = object.painted->mesh();
  mesh.normals.assign(mesh.positions.size(), {0, 0, 1});
  object.painted = std::make_shared<PaintedMesh>(mesh, 64);
  auto deformation = std::make_shared<DeformationState>(*object.deformation);
  const auto old_targets = deformation->morphs->targets;
  deformation->morphs = MorphSet(mesh);
  for (const auto &target : old_targets) {
    auto positions = mesh.positions;
    for (size_t v = 0; v < positions.size(); ++v) positions[v] += target.delta[v];
    deformation->morphs->set_target(mesh, target.name, positions);
  }
  object.deformation = std::move(deformation);
  ObjectBakeSettings settings;
  settings.prepare_normals = true;
  settings.front_mm = settings.back_mm = 500;
  settings.ao_samples = 0;
  settings.padding = 4;
  settings.allow_partial = true;
  Document document;
  document.replace(scene);
  document.bake_from_object(scene.objects().back().id, settings);
  settled(document);
  scene = document.copy_scene();
  scene.active().transform.position = {-.6f, -.6f, 0};
  scene.active().transform.scale = 1.2f;
  return scene;
}
void artist_offset_oracle(const Scene &scene, size_t slot) {
  const auto &object = scene.active();
  const auto &mesh = object.painted->mesh();
  const auto &frames = *object.material_bindings->normal_frames;
  const auto &attributes = *frames.pbr;
  const auto &target = key_frame(attributes, frames.current_targets.at(slot));
  auto changed = mesh.positions;
  for (size_t v = 0; v < changed.size(); ++v)
    changed[v] += object.deformation->morphs->targets.at(slot).delta[v];
  auto geometry = [&](const auto &positions) {
    std::vector<Vec3> result(positions.size());
    for (size_t c = 0; c < mesh.indices.size(); c += 3) {
      const auto a = mesh.indices[c], b = mesh.indices[c + 1], d = mesh.indices[c + 2];
      const auto n = cross(positions[b] - positions[a], positions[d] - positions[a]);
      for (auto v : {a, b, d}) result[v] += n;
    }
    for (auto &n : result) n = normalized(n);
    return result;
  };
  const auto before = geometry(mesh.positions), after = geometry(changed);
  bool retained = true;
  float offset = 0;
  for (size_t c = 0; c < mesh.indices.size(); ++c) {
    const auto vertex = mesh.indices[c];
    const auto artist = attributes.normals[c];
    const auto actual = artist + target.normals.at(c);
    retained &= std::abs(length(actual) - length(artist)) < 1e-5f &&
                std::abs(dot(actual, after[vertex]) - dot(artist, before[vertex])) < 1e-5f;
    offset = std::max(offset, length(artist - before[vertex]));
  }
  check(offset > .02f && retained,
        "Independent cross-product frames prove post-UV key authoring retains non-geometric artist-normal offsets instead of replacing them with geometry normals");
}
void geometric_oracle(const Scene &scene, size_t slot) {
  const auto &object = scene.active();
  const auto &mesh = object.painted->mesh();
  const auto &frames = *object.material_bindings->normal_frames;
  const auto &attributes = *frames.pbr;
  const auto &target = key_frame(attributes, frames.current_targets.at(slot));
  auto positions = mesh.positions;
  for (size_t v = 0; v < positions.size(); ++v)
    positions[v] += object.deformation->morphs->targets[slot].delta[v];
  std::vector<Vec3> normals(positions.size());
  std::vector<unsigned> faces(positions.size());
  for (size_t c = 0; c < mesh.indices.size(); c += 3) {
    const auto a = mesh.indices[c], b = mesh.indices[c + 1], d = mesh.indices[c + 2];
    const auto normal = cross(positions[b] - positions[a], positions[d] - positions[a]);
    for (auto v : {a, b, d}) { normals[v] += normal; ++faces[v]; }
  }
  for (auto &normal : normals) normal = normalized(normal);
  bool exact_normals = true;
  for (size_t c = 0; c < mesh.indices.size(); ++c)
    exact_normals &= length(attributes.normals[c] + target.normals.at(c) -
                            normals[mesh.indices[c]]) < 1e-5f;
  check(exact_normals,
        "Independent triangle cross products recover the actual new post-UV full NORMAL target rather than merely matching two consumers of the same array");

  // Use the retained original frame UV, not the current sampling atlas.
  auto original = object.material_bindings->normal_preparation;
  while (original->uv_reprojection)
    original = original->uv_reprojection->before_material->normal_preparation;
  const auto &uv = original->original_paint->mesh().uv;
  bool exact_tangents = true;
  size_t exclusive_corners = 0;
  for (size_t c = 0; c < mesh.indices.size(); c += 3) {
    const auto a = mesh.indices[c], b = mesh.indices[c + 1], d = mesh.indices[c + 2];
    const auto e1 = positions[b] - positions[a], e2 = positions[d] - positions[a];
    const float u1 = uv[b][0] - uv[a][0], v1 = uv[b][1] - uv[a][1];
    const float u2 = uv[d][0] - uv[a][0], v2 = uv[d][1] - uv[a][1];
    const auto tangent = (e1 * v2 - e2 * v1) / (u1 * v2 - u2 * v1);
    for (size_t k = 0; k < 3; ++k) {
      const auto vertex = mesh.indices[c + k];
      if (faces[vertex] != 1) continue;
      const auto expected = normalized(tangent - normals[vertex] * dot(normals[vertex], tangent));
      const auto &base = attributes.tangents.at(c + k);
      const auto actual = Vec3{base[0], base[1], base[2]} + target.tangents.at(c + k);
      exact_tangents &= length(actual - expected) < 1e-5f;
      ++exclusive_corners;
    }
  }
  check(exclusive_corners == 2 && exact_tangents,
        "Independent original-UV triangle derivatives recover new exclusive-corner TANGENT targets after atlas changes");
}
Scene authored(const Scene &source, const std::filesystem::path &project, bool analytic) {
  Document document;
  document.replace(source);
  settled(document);
  const size_t slot = source.active().deformation->morphs->targets.size();
  ShapeKeyEdit edit;
  edit.name = "Graphical key authored after UV";
  document.author_shape(edit);
  settled(document);
  edit.action = ShapeKeyAction::Replace;
  edit.target = slot;
  edit.positions = source.active().painted->mesh().positions;
  edit.positions[2].z += .08f;
  edit.positions[1].x += .04f;
  document.author_shape(edit);
  settled(document);
  const auto scene = document.copy_scene();
  check(scene.active().painted->encode() == source.active().painted->encode(),
        "Actual post-UV key authoring retains all current intrinsic/detail/color/coverage pixels before export");
  if (analytic) geometric_oracle(scene, slot);
  auto zero = scene, one = scene;
  auto deformation = *scene.active().deformation;
  deformation.pose.clear();
  deformation.morph_weights.assign(deformation.morphs->targets.size(), 0);
  zero.set_deformation(zero.active().id, deformation);
  deformation.morph_weights[slot] = 1;
  one.set_deformation(one.active().id, deformation);
  Document zero_document, one_document;
  zero_document.replace(zero);
  one_document.replace(one);
  settled(zero_document);
  settled(one_document);
  SurfaceRenderer renderer;
  check(delta(draw(renderer, zero_document.frame().surface),
              draw(renderer, one_document.frame().surface)) > 2,
        "The newly authored target alone visibly changes native geometry or shading after UV reprojection");
  document.save(project);
  Document reopened;
  reopened.open(project);
  settled(reopened);
  check(reopened.copy_scene().encode() == scene.encode(),
        "Independent files are exported from a newly reopened post-UV authored model");
  return reopened.copy_scene();
}
} // namespace

int main(int argc, char **argv) try {
  if (argc < 2) return 2;
  const std::filesystem::path prefix = argv[1];
  std::filesystem::create_directories(prefix.parent_path());
  NativeGraphics graphics;
  const std::string renderer = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
  std::cout << "GPU " << renderer << '\n';
  check(argc < 3 || std::string(argv[2]) != "--hardware" ||
            renderer.find("A770") != std::string::npos,
        "Post-UV authored key consumers run on the requested actual graphics device");
  Fbo framebuffer;
  framebuffer.bind();
  SurfaceRenderer native;
  prepared_normal_consumer::Renderer independent;
  for (bool typed : {false, true}) {
    const auto original = prepared(typed);
    const auto exact = original.encode();
    const auto name = prefix.string() + (typed ? "-typed" : "-pbr");
    auto current = transfer(original, name + "-first.dsculpt");
    current = transfer(current, name + "-repeat.dsculpt");
    current = authored(current, name + "-authored.dsculpt", !typed);
    consumers(current, typed, name, native, framebuffer, independent);
    check(original.encode() == exact,
          "Post-UV authored normal keys and all graphical consumers retain the complete original source");
  }
  const auto artist = artist_prepared();
  const auto artist_exact = artist.encode();
  const auto name = prefix.string() + "-artist";
  auto changed = transfer(artist, name + "-first.dsculpt");
  changed = transfer(changed, name + "-repeat.dsculpt");
  const auto slot = changed.active().deformation->morphs->targets.size();
  changed = authored(changed, name + "-authored.dsculpt", false);
  artist_offset_oracle(changed, slot);
  consumers(changed, false, name, native, framebuffer, independent);
  check(artist.encode() == artist_exact,
        "Authored-normal post-UV consumer checks leave the complete artist source unchanged");
  std::cout << "PASS " << checks << " post-UV authored key graphical checks\n";
  return 0;
} catch (const std::exception &error) {
  std::cerr << "FAIL " << error.what() << '\n';
  return 1;
}
