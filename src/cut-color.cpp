// SPDX-License-Identifier: GPL-3.0-or-later
#include "cut-color.hpp"
#include "affine-surface.hpp"
#include "current-colors.hpp"
#include "dome/source-maps.hpp"
#include "polygon-conversion.hpp"
#include <atomic>
#include <map>
#include <set>
#include <tuple>
namespace dome {
namespace {
void stop(const std::function<bool()> &cancelled) {
  if (cancelled && cancelled())
    throw std::runtime_error("Plane cutting cancelled");
}
struct Work {
  const detail::CutColorLimits &limits;
  const std::function<bool()> &cancelled;
  std::atomic_size_t used{0};
  std::function<bool()> poll;
  Work(const detail::CutColorLimits &bounds,
       const std::function<bool()> &cancel)
      : limits(bounds), cancelled(cancel), poll([this] {
          visit();
          return false;
        }) {
    if (!limits.maximum_work || limits.maximum_work > 64'000'000 ||
        !limits.maximum_vertices || limits.maximum_vertices > 500'000 ||
        !limits.maximum_faces || limits.maximum_faces > 500'000 ||
        !limits.maximum_corners || limits.maximum_corners > 2'000'000 ||
        !limits.maximum_provenance || limits.maximum_provenance > 128 ||
        !limits.maximum_raster_samples ||
        limits.maximum_raster_samples > 128'000'000)
      throw std::invalid_argument("Invalid colored cut resource limits");
  }
  void visit(size_t count = 1) {
    auto before = used.load(std::memory_order_relaxed);
    do {
      if (count > limits.maximum_work - before)
        throw std::length_error("Colored cut preparation work budget exceeded");
    } while (!used.compare_exchange_weak(before, before + count,
                                         std::memory_order_relaxed));
    stop(cancelled);
  }
  PolygonLimits topology() const {
    PolygonLimits out;
    out.maximum_vertices = limits.maximum_vertices;
    out.maximum_faces = limits.maximum_faces;
    out.maximum_corners = limits.maximum_corners;
    out.maximum_provenance = limits.maximum_provenance;
    out.maximum_work = limits.maximum_work;
    out.cancelled = poll;
    return out;
  }
};
bool same(Vec3 a, Vec3 b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
bool same_material(const Material &a, const Material &b) {
  return a.name == b.name && a.color == b.color && a.metallic == b.metallic &&
         a.roughness == b.roughness && a.emissive == b.emissive &&
         a.toon == b.toon && a.double_sided == b.double_sided &&
         a.alpha == b.alpha && a.alpha_cutoff == b.alpha_cutoff &&
         a.unlit == b.unlit;
}
bool same_image(const Image &a, const Image &b, Work &work) {
  work.visit();
  if (a.width != b.width || a.height != b.height ||
      a.rgba.size() != b.rgba.size())
    return false;
  for (size_t at = 0; at < a.rgba.size(); at += 4096) {
    auto count = std::min(size_t(4096), a.rgba.size() - at);
    work.visit(count);
    if (!std::equal(a.rgba.begin() + at, a.rgba.begin() + at + count,
                    b.rgba.begin() + at))
      return false;
  }
  return true;
}
bool same_maps(const std::shared_ptr<const TextureMaps> &a,
               const std::shared_ptr<const TextureMaps> &b, Work &work) {
  work.visit();
  if (a == b)
    return true;
  if (!a || !b || a->source_maps != b->source_maps ||
      a->source_only != b->source_only || a->transparent != b->transparent ||
      a->overlay != b->overlay ||
      a->color_projection || b->color_projection || a->normal_projection ||
      b->normal_projection)
    return false;
  for (auto pair : {std::pair{&a->color, &b->color},
                    std::pair{&a->metal_rough, &b->metal_rough},
                    std::pair{&a->emission, &b->emission},
                    std::pair{&a->normal, &b->normal}})
    if (!same_image(*pair.first, *pair.second, work))
      return false;
  return a->pbr_color.rgba.empty() && b->pbr_color.rgba.empty();
}
std::array<double, 3> convex(std::array<float, 3> input) {
  double sum = 0;
  for (auto w : input) {
    if (!std::isfinite(w) || w < 0 || w > 1)
      throw std::invalid_argument("Invalid cut color barycentrics");
    sum += w;
  }
  if (std::abs(sum - 1) > 1e-6)
    throw std::invalid_argument(
        "Cut color barycentrics are not a convex partition");
  return {input[0] / sum, input[1] / sum, input[2] / sum};
}
struct Shell {
  Mesh mesh;
  std::vector<uint32_t> triangles;
  std::vector<std::array<float, 3>> barycentrics;
};
Shell part(const PlaneCutResult &cut, bool cap, Work &work) {
  Shell out;
  for (size_t t = 0; t < cut.source_triangles.size(); ++t) {
    work.visit(3);
    if ((cut.source_triangles[t] == UINT32_MAX) != cap)
      continue;
    out.triangles.push_back(cut.source_triangles[t]);
    for (size_t k = 0; k < 3; ++k) {
      auto at = t * 3 + k;
      auto v = cut.mesh.indices.at(at);
      out.mesh.indices.push_back(uint32_t(out.mesh.positions.size()));
      out.mesh.positions.push_back(cut.mesh.positions.at(v));
      out.mesh.normals.push_back(cut.mesh.normals.at(v));
      if (!cut.mesh.uv.empty())
        out.mesh.uv.push_back(cut.mesh.uv.at(v));
      out.barycentrics.push_back(cut.source_corner_barycentrics.at(at));
    }
  }
  return out;
}
void atlas(Mesh &mesh, uint32_t resolution, Work &work) {
  const auto &cancelled = work.poll;
  work.visit(mesh.indices.size());
  auto before = mesh;
  unwrap(mesh, resolution, 4, cancelled);
  if (mesh.indices.size() != before.indices.size())
    throw std::runtime_error("Cut atlas changed face order");
  for (size_t k = 0; k < mesh.indices.size(); ++k) {
    work.visit();
    if (!same(mesh.positions.at(mesh.indices[k]),
              before.positions.at(before.indices[k])))
      throw std::runtime_error(
          "Cut atlas changed an authenticated geometry corner");
  }
}
void localize(Shell &shell, const detail::AffineInteractionFrame &frame,
              Work &work) {
  for (size_t i = 0; i < shell.mesh.positions.size(); ++i) {
    work.visit();
    shell.mesh.positions[i] = detail::affine_position(
        frame.inverse().point(detail::affine_vector(shell.mesh.positions[i])));
    shell.mesh.normals[i] = detail::affine_unit(
        frame.inverse().normal(detail::affine_vector(shell.mesh.normals[i])));
  }
  if (frame.orientation() == AffineOrientation::Reversing)
    for (size_t i = 0; i < shell.mesh.indices.size(); i += 3) {
      work.visit();
      std::swap(shell.mesh.indices[i + 1], shell.mesh.indices[i + 2]);
      std::swap(shell.barycentrics[i + 1], shell.barycentrics[i + 2]);
    }
}
struct Source {
  BakeSource common;
  std::vector<PolygonId> corners, vertices;
};
Source authenticate(const Scene &scene, const BakeSource &display, Work &work) {
  const auto &cancelled = work.poll;
  work.visit();
  const auto &object = scene.active();
  const auto &authority = *object.polygon;
  auto limits = work.topology();
  if (display.mesh.indices.size() > limits.maximum_corners ||
      display.mesh.positions.size() > limits.maximum_corners)
    throw std::length_error("Colored cut source geometry budget exceeded");
  auto render = authority.topology.triangulate(limits);
  PolygonColorSurface current{
      std::shared_ptr<const PolygonMesh>(object.polygon, &authority.topology),
      authority.colors};
  auto colors = detail::current_vertex_colors(
      object.painted->mesh(), &current, object.appearance.get(),
      object.material_bindings.get(), nullptr, cancelled);
  Mesh mesh = object.painted->mesh();
  if (object.deformation) {
    work.visit(mesh.positions.size() + object.deformation->resource_entries());
    // DeformationState::preview is the same bounded CPU evaluation used by
    // the immutable mesh worker. PaintedMesh::evaluate only builds paint
    // queries for an already evaluated mesh; it does not apply skin/morphs.
    mesh = object.deformation->preview(mesh);
    work.visit();
  }
  auto frame = detail::placement_frame(scene, object.id);
  const bool affine = scene.has_affine_placement(object.id),
             reflected = frame.orientation() == AffineOrientation::Reversing;
  const auto similarity =
      affine ? CanvasTransform{} : scene.world_transform(object.id);
  if (display.mesh.indices.size() != mesh.indices.size() ||
      display.colors.size() != display.mesh.positions.size() ||
      display.materials.size() != 1 || !display.triangle_materials.empty())
    throw std::invalid_argument(
        "The cut source no longer matches its complete colored surface");
  display.mesh.validate();
  auto maps = object.painted->preview(object.material);
  const auto &material = display.materials.front();
  if (!same_material(material.material, object.material) ||
      !same_maps(material.maps, maps, work) || material.normal_map ||
      material.normal_strength != 1 || !material.maps_are_composited ||
      material.repeat_uv)
    throw std::invalid_argument(
        "The cut material or paint source changed; prepare a new preview");
  Source out;
  out.common.materials = {BakeMaterial{object.material, maps}};
  const bool canonical = maps && !maps->normal.rgba.empty();
  auto tangents =
      canonical ? tangent_corners(mesh, cancelled) : std::vector<Tangent>{};
  std::vector<uint8_t> used(display.mesh.positions.size());
  for (size_t at = 0; at < mesh.indices.size(); ++at) {
    work.visit();
    size_t rest = at;
    if (reflected && at % 3)
      rest = at / 3 * 3 + (at % 3 == 1 ? 2 : 1);
    auto v = mesh.indices.at(rest), shown = display.mesh.indices.at(at);
    used.at(shown) = 1;
    auto world_position =
        ob::transform(similarity, {mesh.positions[v].x, mesh.positions[v].y,
                                   mesh.positions[v].z});
    auto position =
        affine ? detail::placed_position(frame, mesh.positions[v])
               : Vec3{world_position.x, world_position.y, world_position.z};
    auto rotated =
        ob::rotate(similarity.orientation,
                   {mesh.normals[v].x, mesh.normals[v].y, mesh.normals[v].z});
    auto normal = affine ? detail::placed_normal(frame, mesh.normals[v])
                         : Vec3{rotated.x, rotated.y, rotated.z};
    if (!same(position, display.mesh.positions.at(shown)) ||
        !same(normal, display.mesh.normals.at(shown)) ||
        mesh.uv.at(v) != display.mesh.uv.at(shown) ||
        colors.at(v) != display.colors.at(shown))
      throw std::invalid_argument("The cut preview differs from the ordered "
                                  "colored source; prepare it again");
    out.corners.push_back(render.corners.at(v));
    out.vertices.push_back(render.vertices.at(v));
    out.common.mesh.indices.push_back(
        uint32_t(out.common.mesh.positions.size()));
    out.common.mesh.positions.push_back(position);
    out.common.mesh.normals.push_back(normal);
    out.common.mesh.uv.push_back(mesh.uv.at(v));
    if (canonical) {
      auto t = tangents.at(rest);
      if (affine)
        t = detail::placed_tangent(frame, t);
      else {
        auto p = ob::rotate(similarity.orientation, {t[0], t[1], t[2]});
        t = {p.x, p.y, p.z, t[3]};
      }
      out.common.tangents.push_back(t);
    }
  }
  for (auto present : used) {
    work.visit();
    if (!present)
      throw std::invalid_argument(
          "The cut source contains unwitnessed vertices");
  }
  return out;
}
bool same_mesh(const Mesh &a, const Mesh &b, Work &work) {
  work.visit();
  if (a.indices.size() > work.limits.maximum_corners ||
      a.positions.size() > work.limits.maximum_corners ||
      a.uv.size() > work.limits.maximum_corners ||
      a.uv_pins.size() > work.limits.maximum_corners ||
      a.uv_seams.size() > work.limits.maximum_corners)
    return false;
  work.visit(a.indices.size() + a.positions.size() + a.uv.size() +
             a.uv_pins.size() + a.uv_seams.size());
  if (a.indices != b.indices || a.uv != b.uv || a.uv_pins != b.uv_pins ||
      a.uv_seams != b.uv_seams || a.positions.size() != b.positions.size() ||
      a.normals.size() != b.normals.size())
    return false;
  for (size_t i = 0; i < a.positions.size(); ++i) {
    work.visit();
    if (!same(a.positions[i], b.positions[i]) ||
        !same(a.normals[i], b.normals[i]))
      return false;
  }
  return true;
}
void same_preview(const CutPreview &provided, const CutPreview &actual,
                  Work &work) {
  work.visit();
  if (!provided.result || !actual.result || provided.changed != actual.changed)
    throw std::invalid_argument("The cut preview is incomplete or stale");
  if (!same(provided.plane.normal, actual.plane.normal) ||
      provided.plane.offset != actual.plane.offset ||
      !same_mesh(provided.cap_mesh, actual.cap_mesh, work))
    throw std::invalid_argument(
        "The displayed cut plane or cap changed; prepare it again");
  const auto &a = *provided.result, &b = *actual.result;
  if (a.source_triangles.size() > work.limits.maximum_corners / 3 ||
      a.source_corner_barycentrics.size() > work.limits.maximum_corners ||
      a.materials.size() > work.limits.maximum_corners / 3)
    throw std::length_error("Cut preview provenance budget exceeded");
  work.visit(a.source_triangles.size() + a.source_corner_barycentrics.size() +
             a.materials.size());
  if (a.source_triangles != b.source_triangles ||
      a.source_corner_barycentrics != b.source_corner_barycentrics ||
      a.materials != b.materials || a.cap_triangles != b.cap_triangles ||
      !same_mesh(a.mesh, b.mesh, work))
    throw std::invalid_argument(
        "The cut preview lost its authenticated clipping correspondence");
}
} // namespace

// Source topology remains native and connected across a clipped shared edge.
// Position alone cannot weld unrelated coincident source vertices/shells.
struct PolygonCutAccess {
  static PolygonMesh build(const Shell &shell, const PolygonMesh &input,
                           const Source *source, const PolygonLimits &limits,
                           Work &work) {
    using Key = std::pair<std::array<float, 3>, std::vector<PolygonId>>;
    std::map<Key, uint32_t> vertices;
    std::map<PolygonId, const PolygonVertex *> originals;
    if (source)
      for (const auto &v : input.vertices()) {
        work.visit();
        originals.emplace(v.id, &v);
      }
    std::vector<Vec3> positions;
    std::vector<std::vector<uint32_t>> faces;
    std::vector<std::vector<std::pair<PolygonId, double>>> provenance;
    for (size_t t = 0; t < shell.triangles.size(); ++t) {
      work.visit();
      std::vector<uint32_t> face;
      for (size_t k = 0; k < 3; ++k) {
        const auto at = t * 3 + k;
        auto p = shell.mesh.positions.at(shell.mesh.indices.at(at));
        auto bary = source ? convex(shell.barycentrics.at(at))
                           : std::array<double, 3>{};
        std::vector<PolygonId> support;
        std::map<PolygonId, double> weights;
        for (size_t c = 0; c < 3; ++c)
          if (bary[c] > 0) {
            const auto id =
                source->vertices.at(size_t(shell.triangles[t]) * 3 + c);
            support.push_back(id);
            for (auto [original, weight] : originals.at(id)->provenance) {
              work.visit();
              weights[original] += bary[c] * weight;
            }
          }
        std::sort(support.begin(), support.end());
        support.erase(std::unique(support.begin(), support.end()),
                      support.end());
        if (weights.size() > limits.maximum_provenance)
          throw std::length_error("Cut vertex source budget exceeded");
        Key key{{p.x, p.y, p.z}, std::move(support)};
        auto found = vertices.find(key);
        if (found == vertices.end()) {
          if (positions.size() >= limits.maximum_vertices)
            throw std::length_error(
                "Colored cut native vertex budget exceeded");
          found = vertices.emplace(std::move(key), uint32_t(positions.size()))
                      .first;
          positions.push_back(p);
          std::vector<std::pair<PolygonId, double>> row;
          if (source) {
            double sum = 0;
            for (auto [id, w] : weights) {
              work.visit();
              (void)id;
              sum += w;
            }
            for (auto [id, w] : weights)
              if (w > 0)
                row.emplace_back(id, w / sum);
          }
          provenance.push_back(std::move(row));
        }
        face.push_back(found->second);
      }
      faces.push_back(std::move(face));
    }
    auto out = PolygonMesh::from_faces(positions, faces, limits);
    if (source)
      for (size_t i = 0; i < out.vertices_.size(); ++i) {
        work.visit();
        out.vertices_[i].provenance = std::move(provenance[i]);
      }
    for (size_t t = 0; t < out.faces_.size(); ++t)
      for (size_t k = 0; k < 3; ++k) {
        work.visit();
        auto &corner = out.faces_[t].corners[k];
        corner.uv = shell.mesh.uv.at(shell.mesh.indices.at(t * 3 + k));
        corner.generated_uv = false;
      }
    out.validate(limits);
    return out;
  }
};
namespace detail {
bool cut_color_supported(const SculptObject &object) noexcept {
  if (!object.polygon || !object.polygon->active || !object.polygon->colors)
    return true;
  if (!object.mesh_active || !object.painted || object.appearance)
    return false;
  if (object.material_bindings &&
      (object.material_bindings->has_mtoon ||
       object.material_bindings->normal_frames ||
       object.material_bindings->normal_preparation ||
       object.material_bindings->color_preparation))
    return false;
  if (object.painted->normal_projection() || object.painted->color_projection())
    return false;
  if (auto maps = object.painted->source_maps())
    for (const auto *slot : {&maps->color, &maps->metal_rough, &maps->emission,
                             &maps->normal, &maps->occlusion})
      if (slot->image && slot->texcoord != 0)
        return false;
  if (auto baked = object.painted->baked();
      baked && (baked->color_interpretation !=
                    BakedColorInterpretation::VertexComponent ||
                baked->normal_interpretation !=
                    BakedNormalInterpretation::CanonicalMikk))
    return false;
  return true;
}
void require_cut_color_supported(const SculptObject &object) {
  if (!cut_color_supported(object))
    throw std::logic_error(
        "This colored mesh has source shading that plane cuts cannot yet "
        "preserve; the original is unchanged");
}
CutPublishReport publish_colored_cut(
    Scene &scene, const CutPreview &preview, const BakeSource &display,
    uint32_t resolution, const std::function<bool()> &requested_cancel,
    const std::function<void(float)> &progress, const CutColorLimits &bounds) {
  Work work(bounds, requested_cancel);
  const auto &cancelled = work.poll;
  work.visit();
  scene.validate();
  require_cut_color_supported(scene.active());
  const auto original = scene.active();
  if (!original.polygon || !original.polygon->active ||
      !original.polygon->colors)
    throw std::logic_error("A colored polygon source is required");
  if (original.locked || !original.visible)
    throw std::logic_error("Show and unlock the cut source");
  if (resolution < 64 || resolution > 2048 || (resolution & (resolution - 1)))
    throw std::invalid_argument("Invalid cut texture resolution");
  if (scene.objects().size() > 254 || scene.groups().size() >= 128)
    throw std::length_error("No room for cut outputs");
  auto source = authenticate(scene, display, work);
  auto actual = preview_cut(source.common.mesh, preview.settings, cancelled);
  same_preview(preview, actual, work);
  if (!actual.changed)
    return {};
  auto shell = part(*actual.result, false, work),
       cap = part(*actual.result, true, work);
  if (shell.mesh.indices.size() > bounds.maximum_corners ||
      cap.mesh.indices.size() > bounds.maximum_corners ||
      shell.triangles.size() > bounds.maximum_faces ||
      cap.triangles.size() > bounds.maximum_faces)
    throw std::length_error("Colored cut output component budget exceeded");
  if (progress)
    progress(.05f);
  atlas(shell.mesh, resolution, work);
  if (!cap.mesh.indices.empty())
    atlas(cap.mesh, resolution, work);
  auto frame = placement_frame(scene, original.id);
  localize(shell, frame, work);
  localize(cap, frame, work);
  auto limits = work.topology();
  auto topology = PolygonCutAccess::build(shell, original.polygon->topology,
                                          &source, limits, work);
  auto derivative = topology.triangulate(limits);
  if (derivative.mesh.indices.size() != shell.barycentrics.size() ||
      topology.faces().size() != shell.triangles.size())
    throw std::runtime_error(
        "Cut topology changed the authenticated triangle order");
  std::vector<PolygonCornerSource> transfers;
  for (size_t t = 0; t < topology.faces().size(); ++t)
    for (size_t k = 0; k < 3; ++k) {
      stop(cancelled);
      auto bary = convex(shell.barycentrics.at(t * 3 + k));
      PolygonCornerSource row;
      row.corner = topology.faces()[t].corners[k].id;
      for (size_t c = 0; c < 3; ++c)
        if (bary[c] > 0)
          row.weights.emplace_back(
              source.corners.at(size_t(shell.triangles[t]) * 3 + c), bary[c]);
      transfers.push_back(std::move(row));
    }
  auto colors =
      std::make_shared<const PolygonColorField>(transfer_polygon_colors(
          original.polygon->topology, *original.polygon->colors, topology,
          transfers, limits));
  Mesh low = derivative.mesh;
  auto low_tangents = tangent_corners(low, cancelled);
  place_mesh(low, frame, cancelled);
  for (auto &t : low_tangents) {
    work.visit();
    t = placed_tangent(frame, t);
  }
  auto barycentrics = shell.barycentrics;
  if (frame.orientation() == AffineOrientation::Reversing)
    for (size_t k = 0; k < low.indices.size(); k += 3) {
      work.visit();
      std::swap(low.indices[k + 1], low.indices[k + 2]);
      std::swap(barycentrics[k + 1], barycentrics[k + 2]);
    }
  // Target normals/tangents are in the exact renderer world basis. The source
  // carries common PBR/paint only: current RGBA is not flattened or multiplied
  // into the bake a second time.
  BakeOptions options;
  options.resolution = resolution;
  options.padding = 8;
  options.ao_samples = 0;
  options.max_raster_samples = bounds.maximum_raster_samples;
  options.source_triangles = shell.triangles;
  options.source_corner_barycentrics = std::move(barycentrics);
  options.cancelled = cancelled;
  // low_tangents is per index, while the optional bake input is per vertex.
  options.low_tangents.resize(low.positions.size());
  for (size_t k = 0; k < derivative.mesh.indices.size(); ++k)
    options.low_tangents[derivative.mesh.indices[k]] = low_tangents[k];
  if (progress)
    options.progress = [&](float p) { progress(.15f + p * .65f); };
  auto baked = bake_mesh(low, source.common, options);
  stop(cancelled);
  BakedTextures textures;
  textures.maps = std::move(baked.maps);
  textures.maps.normal = std::move(baked.normal);
  for (auto coverage : baked.coverage) {
    work.visit();
    textures.coverage.push_back(coverage == 1 || coverage == 2 ? 255 : 0);
  }
  auto paint =
      std::make_shared<PaintedMesh>(std::move(derivative.mesh), resolution);
  paint->install_bake(std::move(textures));
  auto origin = capture_polygon_origin(scene, original.id);
  auto authority = std::make_shared<PolygonAuthority>();
  authority->topology = std::move(topology);
  authority->colors = std::move(colors);
  authority->origin = origin;
  std::shared_ptr<PaintedMesh> cap_paint;
  std::shared_ptr<PolygonAuthority> cap_authority;
  if (!cap.mesh.indices.empty()) {
    cap_authority = std::make_shared<PolygonAuthority>();
    cap_authority->topology = PolygonCutAccess::build(
        cap, original.polygon->topology, nullptr, limits, work);
    cap_authority->origin = origin;
    cap_paint = std::make_shared<PaintedMesh>(
        cap_authority->topology.triangulate(limits).mesh, resolution);
  }
  stop(cancelled);
  if (progress)
    progress(.9f);
  auto next = scene.snapshot();
  auto name = original.name.size() <= 252 ? original.name + " cut"
                                          : std::string("Cut result");
  auto group = next.add_group(std::move(name), {}, original.group);
  CutPublishReport report;
  report.changed = true;
  report.projected = baked.projected;
  report.shell = next.add("Cut shell", Volume{});
  auto &side = next.object(report.shell);
  side.group = group;
  side.transform = original.transform;
  side.affine_edit = original.affine_edit;
  side.material = original.material;
  side.mesh_active = true;
  side.painted = std::move(paint);
  side.polygon = std::move(authority);
  if (cap_paint) {
    report.cap = next.add("Cut cap", Volume{});
    auto &end = next.object(report.cap);
    end.group = group;
    end.transform = original.transform;
    end.affine_edit = original.affine_edit;
    end.material = preview.settings.cap_material;
    end.mesh_active = true;
    end.painted = std::move(cap_paint);
    end.polygon = std::move(cap_authority);
  }
  next.object(original.id).visible = false;
  next.select(report.shell);
  next.validate();
  stop(cancelled);
  if (progress)
    progress(1);
  stop(cancelled);
  scene = std::move(next);
  return report;
}
} // namespace detail
} // namespace dome
