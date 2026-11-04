// SPDX-License-Identifier: GPL-3.0-or-later
#include "prepared-normal-shapes-fixtures.hpp"
#include "dome/document.hpp"
#include "dome/source-uv-bake.hpp"
#include "dome/polygon-scene.hpp"
#include <iostream>

using namespace dome;
namespace {
unsigned checks = 0;
void check(bool value, const char *message) {
  if (!value) throw std::runtime_error(message);
  ++checks;
  std::cout << "PASS " << message << '\n';
}
void ready(Document &document) {
  if (!document.wait_asset() || !document.wait_mesh() ||
      !document.frame().state->error.empty())
    throw std::runtime_error("Post-UV key worker: " + document.frame().state->error);
}
void atlas(Document &document) {
  document.begin_uv();
  if (!document.wait_uv()) throw std::runtime_error("UV initialization timeout");
  document.edit_uv(UvAction::Scale, .8f);
  if (!document.wait_uv()) throw std::runtime_error("UV scale timeout");
  document.end_uv(true);
  ready(document);
}
auto rig(const Scene &scene) {
  const auto &object = scene.active();
  auto data = object.deformation->encode(object.painted->mesh());
  data.erase("morphs");
  data.erase("morph_weights");
  data.erase("expressions");
  return data;
}
bool vectors_equal(const std::vector<Vec3> &a, const std::vector<Vec3> &b) {
  if (a.size() != b.size()) return false;
  for (size_t v = 0; v < a.size(); ++v)
    if (a[v].x != b[v].x || a[v].y != b[v].y || a[v].z != b[v].z) return false;
  return true;
}
const NormalFrameTarget &frame_target(const NormalFrames &frames, uint64_t id) {
  for (const auto &target : frames.targets) if (target.id == id) return target;
  throw std::runtime_error("Expected explicit target directions");
}
Scene prepared(bool seam) {
  Document document;
  document.replace(prepared_shape_fixture::model(true));
  ready(document);
  atlas(document);
  atlas(document);
  auto scene = document.copy_scene();
  if (seam) {
    SourceUvBakeInput input;
    input.paint = scene.active().painted;
    input.current_material = scene.active().material_bindings;
    input.current_deformation = scene.active().deformation;
    input.source_vertices = input.paint->mesh().indices;
    for (auto original : input.source_vertices) {
      input.target.positions.push_back(input.paint->mesh().positions[original]);
      input.target.normals.push_back(input.paint->mesh().normals[original]);
      input.target.uv.push_back(input.paint->mesh().uv[original]);
      input.target.indices.push_back(uint32_t(input.target.indices.size()));
    }
    auto adopted = adopt_source_uv_bake(bake_source_uv(input));
    scene.active().painted = adopted.paint;
    scene.active().material_bindings = adopted.material_bindings;
    scene.active().deformation = adopted.deformation;
    scene.active().appearance = adopted.appearance;
  }
  auto &object = scene.active();
  object.painted = object.painted->snapshot();
  object.painted->add_layer("Paint after final UV Apply");
  object.painted->clear_bake();
  auto deformation = *object.deformation;
  deformation.skin->vertices[0] = {{0, .15f}, {1, .85f}};
  deformation.pose[0].scale = {1.1f, .75f, 1.3f};
  scene.set_deformation(object.id, deformation);
  scene.validate();
  return scene;
}
void exercise(bool seam, const std::filesystem::path &prefix) {
  const auto before = prepared(seam);
  const auto original = before.encode();
  const auto paint = before.active().painted->encode();
  const auto before_rig = rig(before);
  const auto old_frames = before.active().material_bindings->normal_frames;
  Document document;
  document.replace(before);
  ready(document);
  ShapeKeyEdit edit;
  edit.name = "Native key after UV";
  const auto slot = before.active().deformation->morphs->targets.size();
  document.author_shape(edit);
  ready(document);
  const auto created = document.copy_scene();
  check(created.active().deformation->morphs->targets.size() == slot + 1,
        "Actual native worker creates a prepared key after repeated normalUV authoring");
  check(created.active().painted->encode() == paint && !created.active().painted->baked(),
        "Post-UV key creation retains current paint layers and Clear-bake state instead of reinstalling historical pixels");
  check(rig(created) == before_rig,
        "Post-UV key creation retains current edited skin, skeleton and anisotropic pose");
  const auto frames = created.active().material_bindings->normal_frames;
  check(frames->current_targets.size() == old_frames->current_targets.size() + 1 &&
            std::equal(old_frames->current_targets.begin(), old_frames->current_targets.end(), frames->current_targets.begin()),
        "New post-UV target appends a stable ID without rewriting existing target slots");
  check(created.active().shape_source &&
            created.active().shape_source->paint->encode() == paint &&
            created.active().shape_source->material_bindings == before.active().material_bindings,
        "The actual complete old UV model is retained as the source capsule");

  edit.action = ShapeKeyAction::Replace;
  edit.target = slot;
  edit.positions = before.active().painted->mesh().positions;
  for (auto &position : edit.positions)
    if (position.x > 0 && position.y > 0) position.z += .06f;
  document.author_shape(edit);
  ready(document);
  const auto replaced = document.copy_scene();
  check(replaced.active().painted->encode() == paint && rig(replaced) == before_rig,
        "Replacing a post-UV target preserves every current paint/detail and rig property");
  check(replaced.active().material_bindings->normal_preparation->uv_reprojection
            ->before_material->normal_frames == replaced.active().material_bindings->normal_frames,
        "Reauthored normal targets are owned by the genuine updated UV before-model");
  auto project = prefix;
  project += seam ? "-seam.dsculpt" : "-native.dsculpt";
  document.save(project);
  Document reopened;
  reopened.open(project);
  ready(reopened);
  check(reopened.copy_scene().encode() == replaced.encode(),
        "New Document restart retains reauthored keys and the complete normalUV receipt chain");
  check(document.undo() && document.copy_scene().encode() == created.encode() &&
            document.redo() && document.copy_scene().encode() == replaced.encode(),
        "A post-UV key replacement remains one exact Undo/Redo transaction");

  if (seam) {
    auto independent = replaced;
    edit.positions = independent.active().painted->mesh().positions;
    edit.positions[0].z += .01f;
    author_shapekey(independent, independent.active().id, edit);
    const auto &new_delta = independent.active().deformation->morphs->targets[slot].delta;
    bool exact = length(new_delta[0]-Vec3{0,0,.01f}) < 1e-6f;
    for (size_t v=1;v<new_delta.size();++v) exact &= length(new_delta[v]) < 1e-6f;
    check(exact && independent.active().material_bindings->key_derivation &&
              independent.active().material_bindings->normal_frames->position_domain &&
              independent.active().painted->encode() == paint && rig(independent) == before_rig &&
              Scene::decode(independent.encode()).encode() == independent.encode(),
          "Independent seam displacement owns its distinct field and restarts without averaging or changing paint/rig");
    ShapeKeyEdit restore; restore.action=ShapeKeyAction::Restore;
    author_shapekey(independent, independent.active().id, restore);
    check(independent.encode() == replaced.encode(),
          "Independent seam Restore recovers its exact actual before-model");
  }
  edit = {};
  edit.action = ShapeKeyAction::Duplicate;
  edit.target = slot;
  edit.name = "Duplicated after UV";
  document.author_shape(edit);
  ready(document);
  const auto duplicated = document.copy_scene();
  const auto duplicate_slot = duplicated.active().deformation->morphs->targets.size() - 1;
  const auto duplicate_frames = duplicated.active().material_bindings->normal_frames;
  const auto &old_direction = frame_target(*duplicate_frames->pbr, duplicate_frames->current_targets[slot]);
  const auto &new_direction = frame_target(*duplicate_frames->pbr, duplicate_frames->current_targets[duplicate_slot]);
  check(vectors_equal(duplicated.active().deformation->morphs->targets[slot].delta,
                      duplicated.active().deformation->morphs->targets[duplicate_slot].delta) &&
            vectors_equal(old_direction.normals, new_direction.normals) &&
            vectors_equal(old_direction.tangents, new_direction.tangents),
        "Post-UV Duplicate preserves exact authored POSITION/NORMAL/TANGENT channels under a new stable ID");

  edit.action = ShapeKeyAction::Mirror;
  edit.mirror_frames = ShapeKeyMirrorFrames::VertexPositions;
  edit.name = "Mirrored after UV";
  document.author_shape(edit);
  ready(document);
  const auto mirrored = document.copy_scene();
  const auto mirror_slot = mirrored.active().deformation->morphs->targets.size() - 1;
  const auto &mesh = mirrored.active().painted->mesh();
  bool mirrored_positions = true;
  for (size_t v = 0; v < mesh.positions.size(); ++v) {
    const auto p = mesh.positions[v];
    size_t match = 0;
    while (match < mesh.positions.size() &&
        (mesh.positions[match].x != -p.x || mesh.positions[match].y != p.y || mesh.positions[match].z != p.z)) ++match;
    if (match == mesh.positions.size()) throw std::runtime_error("Independent mirror fixture has no exact partner");
    const auto original_delta = mirrored.active().deformation->morphs->targets[slot].delta[match];
    const auto actual = mirrored.active().deformation->morphs->targets[mirror_slot].delta[v];
    mirrored_positions &= actual.x == -original_delta.x && actual.y == original_delta.y && actual.z == original_delta.z;
  }
  check(mirrored_positions,
        "Post-UV Mirror follows an independently reflected current local position field through seam correspondence");

  auto mix = mirrored;
  auto mixed_deform = *mix.active().deformation;
  mixed_deform.morph_weights.assign(mixed_deform.morphs->targets.size(), 0);
  mixed_deform.morph_weights[0] = .35f;
  mixed_deform.morph_weights[slot] = .4f;
  mixed_deform.morph_weights[duplicate_slot] = .1f;
  mix.set_deformation(mix.active().id, mixed_deform);
  document.replace(mix);
  ready(document);
  edit = {};
  edit.action = ShapeKeyAction::CaptureMix;
  edit.name = "Captured current post-UV mix";
  document.author_shape(edit);
  ready(document);
  const auto captured = document.copy_scene();
  const auto capture_slot = captured.active().deformation->morphs->targets.size() - 1;
  bool exact_mix = true;
  for (size_t v = 0; v < mesh.positions.size(); ++v) {
    Vec3 expected;
    for (size_t k = 0; k < mixed_deform.morphs->targets.size(); ++k)
      expected += mixed_deform.morphs->targets[k].delta[v] * mixed_deform.morph_weights[k];
    exact_mix &= length(expected - captured.active().deformation->morphs->targets[capture_slot].delta[v]) < 1e-6f;
  }
  const auto mixed_frames = mix.active().material_bindings->normal_frames;
  const auto captured_frames = captured.active().material_bindings->normal_frames;
  const auto &captured_direction = frame_target(*captured_frames->pbr, captured_frames->current_targets[capture_slot]);
  for (size_t c = 0; c < mixed_frames->pbr->normals.size(); ++c) {
    Vec3 expected_n, expected_t;
    for (size_t k = 0; k < mixed_deform.morphs->targets.size(); ++k) {
      const auto &target = frame_target(*mixed_frames->pbr, mixed_frames->current_targets[k]);
      if (!target.normals.empty()) expected_n += target.normals[c] * mixed_deform.morph_weights[k];
      if (!target.tangents.empty()) expected_t += target.tangents[c] * mixed_deform.morph_weights[k];
    }
    exact_mix &= length(expected_n - captured_direction.normals.at(c)) < 1e-6f &&
                 length(expected_t - captured_direction.tangents.at(c)) < 1e-6f;
  }
  check(exact_mix,
        "Post-UV Capture Mix preserves the independent raw linear POSITION/NORMAL/TANGENT sum without normalizing or regenerating input directions");
  edit.action = ShapeKeyAction::Rename;
  edit.target = capture_slot;
  edit.name = "UV 張 capture";
  document.author_shape(edit);
  ready(document);
  check(document.copy_scene().active().deformation->morphs->targets[capture_slot].name == edit.name &&
            document.copy_scene().active().material_bindings->normal_frames->current_targets == captured_frames->current_targets,
        "Post-UV Unicode Rename retains every stable target identity");
  const auto named = document.copy_scene();
  auto cancelled = named;
  size_t polls = 0;
  bool stopped = false;
  try { author_shapekey(cancelled, cancelled.active().id, ShapeKeyEdit{}, [&] { return ++polls > 8; }); }
  catch (const std::exception &) { stopped = true; }
  check(stopped && polls > 8 && cancelled.encode() == named.encode(),
        "Late post-UV key cancellation publishes no partial target, receipt or source history");
  document.save(project);
  reopened.open(project);
  ready(reopened);
  check(reopened.copy_scene().encode() == named.encode(),
        "Save/restart retains all post-UV key actions and the genuine source capsule");
  edit = {};
  edit.action = ShapeKeyAction::Restore;
  document.author_shape(edit);
  ready(document);
  check(document.copy_scene().encode() == original,
        "Restore source recovers the exact complete pre-key UV model including paint and pose");
}
} // namespace
int main(int argc, char **argv) try {
  if (argc != 2) return 2;
  const std::filesystem::path prefix = argv[1];
  std::filesystem::create_directories(prefix.parent_path());
  exercise(false, prefix);
  exercise(true, prefix);
  std::cout << "PASS " << checks << " prepared keys after UV checks\n";
  return 0;
} catch (const std::exception &error) {
  std::cerr << "FAIL " << error.what() << '\n';
  return 1;
}
