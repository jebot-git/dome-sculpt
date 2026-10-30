// SPDX-License-Identifier: GPL-3.0-or-later
#define main original_mtoon_renderer_suite
#include "mtoon-render.cpp"
#undef main
#include "dome/gltf-export.hpp"
#include "dome/tangents.hpp"
#include "polygon-color-consumer.hpp"
#include "polygon-color-current-fixture.hpp"
#include <unistd.h>
namespace {
bool measuring = false;
size_t writes = 0, reads = 0, maps = 0;
} // namespace
extern "C" void __real_glBufferData(GLenum, GLsizeiptr, const void *, GLenum);
extern "C" void __wrap_glBufferData(GLenum t, GLsizeiptr n, const void *p,
                                    GLenum u) {
  if (measuring)
    writes += size_t(n);
  __real_glBufferData(t, n, p, u);
}
extern "C" void __real_glBufferSubData(GLenum, GLintptr, GLsizeiptr,
                                       const void *);
extern "C" void __wrap_glBufferSubData(GLenum t, GLintptr at, GLsizeiptr n,
                                       const void *p) {
  if (measuring)
    writes += size_t(n);
  __real_glBufferSubData(t, at, n, p);
}
extern "C" void __real_glReadPixels(GLint, GLint, GLsizei, GLsizei, GLenum,
                                    GLenum, void *);
extern "C" void __wrap_glReadPixels(GLint x, GLint y, GLsizei w, GLsizei h,
                                    GLenum f, GLenum t, void *p) {
  if (measuring)
    ++reads;
  __real_glReadPixels(x, y, w, h, f, t, p);
}
extern "C" void __real_glGetBufferSubData(GLenum, GLintptr, GLsizeiptr, void *);
extern "C" void __wrap_glGetBufferSubData(GLenum t, GLintptr a, GLsizeiptr n,
                                          void *p) {
  if (measuring)
    ++reads;
  __real_glGetBufferSubData(t, a, n, p);
}
extern "C" void *__real_glMapBuffer(GLenum, GLenum);
extern "C" void *__wrap_glMapBuffer(GLenum t, GLenum a) {
  if (measuring)
    ++maps;
  return __real_glMapBuffer(t, a);
}
extern "C" void *__real_glMapBufferRange(GLenum, GLintptr, GLsizeiptr,
                                         GLbitfield);
extern "C" void *__wrap_glMapBufferRange(GLenum t, GLintptr at, GLsizeiptr n,
                                         GLbitfield f) {
  if (measuring)
    ++maps;
  return __real_glMapBufferRange(t, at, n, f);
}
namespace {
std::shared_ptr<MeshSnapshot> packed(const ObjectSurface &surface) {
  auto s = std::make_shared<MeshSnapshot>();
  s->mesh = surface.preview_mesh();
  s->source_colors = detail::current_vertex_colors(surface);
  auto corners = tangent_corners(surface.mesh);
  s->tangents.assign(surface.mesh.positions.size(), {1, 0, 0, 1});
  std::vector<bool> assigned(surface.mesh.positions.size());
  for (size_t c = 0; c < surface.mesh.indices.size(); ++c) {
    auto v = surface.mesh.indices[c];
    if (assigned[v] && s->tangents[v] != corners[c])
      throw std::runtime_error(
          "Fixture needs explicit tangent-corner expansion");
    s->tangents[v] = corners[c];
    assigned[v] = true;
  }
  for (auto &p : s->mesh.positions) {
    auto q = ob::transform(surface.transform, {p.x, p.y, p.z});
    p = {q.x, q.y, q.z};
  }
  for (auto &n : s->mesh.normals) {
    auto q = ob::rotate(surface.transform.orientation, {n.x, n.y, n.z});
    n = {q.x, q.y, q.z};
  }
  s->ranges.push_back({surface.id,
                       0,
                       s->mesh.indices.size(),
                       surface.material,
                       surface.textures,
                       {},
                       0,
                       surface.material_bindings});
  s->ranges[0].object_world = surface.transform;
  s->ranges[0].world_frame_valid = true;
  s->geometry_identity = std::make_shared<const int>(1);
  return s;
}
Pixels native_draw(SurfaceRenderer &renderer,
                   std::shared_ptr<const MeshSnapshot> mesh,
                   std::span<const ObjectInfo> display = {}) {
  writes = reads = maps = 0;
  measuring = true;
  renderer.draw(std::move(mesh), {}, identity, {0, 0, 2}, 96, 96, true, false,
                false, display);
  measuring = false;
  return read();
}
void compare_picks(const MeshSnapshot &s, const Pixels &pixels) {
  MaterialSurfaceQuery query(s.mesh, s.ranges, {}, s.source_colors);
  size_t compared = 0;
  for (int y = 17; y < 84; y += 13)
    for (int x = 19; x < 84; x += 13) {
      if (x == y)
        continue;
      float wx = 2.f * (x + .5f) / 96 - 1, wy = 2.f * (y + .5f) / 96 - 1;
      auto hit = query.raycast({wx, wy, 2}, {0, 0, -1});
      auto pixel = at(pixels, x, y);
      if (pixel[3] > 5 || !hit) {
        check(bool(hit) == (pixel[3] > 0),
              "Actual GPU coverage and packed RGBA picking agree at "
              "world-space samples");
        ++compared;
      }
    }
  check(compared > 10, "Opaque/Mask/Blend parity covers independent visible "
                       "and transparent samples");
}
} // namespace
int main(int argc, char **argv) try {
  NativeGraphics graphics;
  std::string gpu = reinterpret_cast<const char *>(glGetString(GL_RENDERER));
  std::cout << "GPU " << gpu << '\n';
  check(argc < 2 || std::string(argv[1]) != "--hardware" ||
            gpu.find("llvmpipe") == std::string::npos,
        "Current polygon color consumers execute on requested device");
  Fbo framebuffer;
  framebuffer.bind();
  SurfaceRenderer renderer;
  polygon_color_consumer::Renderer independent;
  auto dir = std::filesystem::temp_directory_path() /
             ("dome-polygon-color-gl-" + std::to_string(getpid()));
  std::filesystem::create_directories(dir);
  struct Cleanup {
    std::filesystem::path dir;
    ~Cleanup() {
      std::error_code e;
      std::filesystem::remove_all(dir, e);
    }
  } cleanup{dir};
  for (bool seam : {false, true})
    for (auto alpha : {Material::AlphaMode::Automatic, Material::AlphaMode::Opaque,
                       Material::AlphaMode::Mask, Material::AlphaMode::Blend}) {
      auto surface = polygon_current_fixture::surface(seam);
      auto maps = std::make_shared<TextureMaps>();
      maps->color = {1, 1, {255, 255, 255, 255}};
      maps->metal_rough = {1, 1, {255, 255, 0, 255}};
      maps->emission = {1, 1, {0, 0, 0, 255}};
      maps->normal = {1, 1, {128, 128, 255, 255}};
      surface.textures = std::move(maps);
      surface.material.alpha = alpha;
      surface.material.alpha_cutoff = .35f;
      surface.transform.position = {.08f, -.03f, 0};
      surface.transform.scale = .85f;
      if (!seam) {
        auto deform = std::make_shared<DeformationState>(surface.mesh);
        deform->morphs = MorphSet(surface.mesh);
        auto target = surface.mesh.positions;
        for (auto &p : target)
          p.x += .1f;
        deform->morphs->set_target(surface.mesh, "shift", target);
        deform->morph_weights = {.5f};
        deform->skeleton = Skeleton{{{"root", -1, {}, 1}}};
        deform->skin = SkinWeights{};
        deform->skin->vertices.assign(surface.mesh.positions.size(), {{0, 1}});
        deform->pose = {{{.02f, -.01f, 0}, {}, {1, 1, 1}}};
        surface.deformation = deform;
      }
      auto snapshot = packed(surface);
      auto native = native_draw(renderer, snapshot);
      check(reads == 0 && maps == 0,
            "Production current-color draw performs no CPU readback or map");
      auto bytes = renderer.buffer_stats().vertex_bytes;
      check(bytes == snapshot->mesh.positions.size() * (56 + 16),
            "Current corner RGBA uses the existing exact16-byte optional GPU "
            "stream");
      auto path = dir / (std::to_string(seam) + "-" +
                         std::to_string(int(alpha)) + ".glb");
      export_glb({surface}, path);
      color_consumer::Model parsed(path);
      auto &primitive = parsed.data->meshes[0].primitives[0];
      auto accessor =
          color_consumer::attr(primitive.attributes, primitive.attributes_count,
                               cgltf_attribute_type_color);
      auto expected_colors = detail::current_vertex_colors(surface);
      bool exact = accessor && accessor->count == expected_colors.size();
      for (size_t i = 0; exact && i < expected_colors.size(); ++i) {
        float c[4];
        exact = cgltf_accessor_read_float(accessor, i, c, 4);
        for (size_t k = 0; k < 4; ++k)
          exact &= c[k] == expected_colors[i][k];
      }
      check(exact, "Actual GLB retains exact float RGBA including nonzero RGB "
                   "on fully transparent seam corners");
      if (!seam)
        check(parsed.data->skins_count == 1 && primitive.targets_count == 1 &&
                  color_consumer::attr(primitive.attributes,
                                       primitive.attributes_count,
                                       cgltf_attribute_type_joints),
              "Independent exported model retains real native skin and morph "
              "channels alongside current colors");
      framebuffer.bind();
      independent.render(parsed.data);
      auto error = delta(native, read());
      std::cout << "CURRENT_COLOR seam=" << seam << " alpha=" << int(alpha)
                << " maxerror=" << error << '\n';
      check(error <= 2, "Independent cgltf/GLSL export consumer matches actual "
                        "linear polygon RGB/alpha interpolation");
      compare_picks(*snapshot, native);
      auto eye = native_draw(renderer, snapshot);
      check(eye == native && writes == 0 && reads == 0 && maps == 0,
            "Second-eye draw reuses geometry and RGBA with zero transfers");
      ObjectInfo info;
      info.id = 1;
      info.visible = info.editable = info.world_transform_valid = true;
      info.world_transform = surface.transform;
      for (int frame = 0; frame < 20; ++frame) {
        info.world_transform.position.x =
            surface.transform.position.x + float(frame) * .002f;
        native_draw(renderer, snapshot, std::span<const ObjectInfo>(&info, 1));
        if (writes || reads || maps)
          throw std::runtime_error(
              "Held color motion performed a CPU transfer");
      }
      check(true, "Twenty held GPU world transforms retain immutable current "
                  "color buffers without upload/read/map");
      surface.transform = info.world_transform;
      auto moved = native_draw(renderer, snapshot,
                               std::span<const ObjectInfo>(&info, 1));
      auto release = packed(surface);
      check(delta(moved, native_draw(renderer, release)) <= 1,
            "Accepted world transform matches held GPU preview without a "
            "current-field rest-identity jump");
      compare_picks(*release, draw(renderer, release));
    }
  check(glGetError() == GL_NO_ERROR,
        "Current polygon color/picking/export consumers leave no GL errors");
  std::cout << checks << " current polygon GPU/GLB checks\n";
  return 0;
} catch (const std::exception &e) {
  measuring = false;
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
