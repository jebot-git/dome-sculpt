// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/creator-archive.hpp"
#include "dome/texture-sets.hpp"
#include <functional>
#include <nlohmann/json.hpp>
#include <set>
namespace dome {
struct PolygonOrigin;
}
namespace dome::detail {
// Decoding returns actual fields; the archive never invents an inline holder or
// recovers an original/source owner from equal-valued current pixels.
struct TextureSetArchiveField {
  std::shared_ptr<PaintedMesh> paint;
  std::shared_ptr<const TextureSetFieldSource> source;
  std::shared_ptr<const SourceMaterialBindings> binding;
  std::shared_ptr<const CreatorAppearance> appearance;
  std::shared_ptr<const ObjectBakeRecipe> recipe;
};
class TextureSetArchive {
public:
  using FieldEncoder =
      std::function<nlohmann::json(const TextureSetView &, uint32_t)>;
  using FieldDecoder = std::function<TextureSetArchiveField(
      const nlohmann::json &,
      const std::shared_ptr<const TextureSetFaceDomain> &, uint32_t,
      const Material &, const PaintedMesh *)>;
  explicit TextureSetArchive(TextureSetLimits limits = {});
  // Enable before emitting any holder. Ordinary current-only v1 is unchanged.
  void shared_records(bool = true);
  bool uses_shared_records() const noexcept { return shared_; }
  void field_encoder(FieldEncoder, std::function<nlohmann::json()> records);
  void field_decoder(FieldDecoder, std::function<void()> complete);
  const nlohmann::json &field_source_records() const noexcept {
    return field_sources_;
  }
  const std::shared_ptr<const Mesh> &geometry_at(uint32_t) const;
  nlohmann::json encode(const SculptObject &, PaintImageArchive &,
                        NormalSamplingArchive *, PaintSamplingArchive *);
  nlohmann::json encode(const PolygonOrigin &, PaintImageArchive &,
                        NormalSamplingArchive *, PaintSamplingArchive *);
  nlohmann::json geometry_records() const;
  static TextureSetArchive decode_geometry(const nlohmann::json &,
                                           TextureSetLimits limits = {});
  void decode(SculptObject &, const nlohmann::json &, const PaintImageArchive &,
              const NormalSamplingArchive *, const PaintSamplingArchive *,
              const SourcePaintDecodeContext &);
  void decode(PolygonOrigin &, const nlohmann::json &,
              const PaintImageArchive &, const NormalSamplingArchive *,
              const PaintSamplingArchive *, const SourcePaintDecodeContext &);
  void preflight_holder(const nlohmann::json &) const;
  static void preflight_reference(const nlohmann::json &,
                                  const nlohmann::json &);
  static std::vector<const nlohmann::json *>
  referenced_paints(const nlohmann::json &, const nlohmann::json &);
  void require_reachable() const;

private:
  struct DomainRecord {
    std::shared_ptr<const TextureSetFaceDomain> owner;
    uint32_t topology = 0, assignment = 0;
  };
  struct RasterRecord {
    std::shared_ptr<const void> owner;
    uint32_t domain = 0, resolution = 0;
    std::shared_ptr<PaintedMesh> decoded;
  };
  struct PaintRecord {
    std::shared_ptr<const PaintedMesh> owner;
    std::shared_ptr<const TextureSetFieldSource> source;
    uint32_t raster = 0;
    nlohmann::json payload;
    TextureSetArchiveField decoded;
    uint8_t state = 0;
  };
  struct CollectionRecord {
    std::shared_ptr<const TextureSetCollection> owner;
    std::shared_ptr<TextureSetCollection> decoded;
    nlohmann::json wire;
    uint8_t state = 0;
    bool primary_source_ready = false;
  };
  struct Holder {
    std::shared_ptr<TextureSetCollection> collection;
    TextureSetArchiveField primary;
  };
  TextureSetLimits limits_;
  size_t bytes_ = 0, work_ = 0, sets_ = 0, legacy_holders_ = 0;
  bool shared_ = false;
  FieldEncoder encode_field_;
  FieldDecoder decode_field_;
  std::function<nlohmann::json()> field_records_;
  std::function<void()> field_complete_;
  nlohmann::json field_sources_ = nlohmann::json::array();
  std::vector<std::shared_ptr<const Mesh>> geometry_;
  std::vector<std::shared_ptr<const TextureSetTopology>> topology_;
  std::vector<uint32_t> topology_geometry_;
  std::vector<std::shared_ptr<const std::vector<TextureSetId>>> assignments_;
  std::vector<DomainRecord> domains_;
  std::vector<RasterRecord> rasters_;
  std::vector<PaintRecord> paints_;
  std::vector<CollectionRecord> collections_;
  mutable std::set<uint32_t> reached_, used_assignments_, used_domains_,
      used_rasters_, used_paints_, used_collections_;
  void charge(size_t, size_t);
  void step(size_t = 1);
  void charge_json(const nlohmann::json &, size_t depth = 0);
  uint32_t add(const std::shared_ptr<const TextureSetTopology> &);
  uint32_t
  add_assignment(const std::shared_ptr<const std::vector<TextureSetId>> &);
  uint32_t add_domain(const std::shared_ptr<const TextureSetFaceDomain> &);
  uint32_t add_paint(const TextureSetView &, PaintImageArchive &,
                     NormalSamplingArchive *, PaintSamplingArchive *);
  uint32_t add_collection(const std::shared_ptr<const TextureSetCollection> &,
                          PaintImageArchive &, NormalSamplingArchive *,
                          PaintSamplingArchive *);
  nlohmann::json
  encode_shared(const std::shared_ptr<const TextureSetCollection> &,
                const TextureSetView &, PaintImageArchive &,
                NormalSamplingArchive *, PaintSamplingArchive *);
  void decode_shared_tables(const nlohmann::json &);
  const std::shared_ptr<const TextureSetFaceDomain> &domain_at(uint32_t);
  TextureSetArchiveField paint_at(uint32_t, const Material &, bool,
                                  const PaintImageArchive &,
                                  const NormalSamplingArchive *,
                                  const PaintSamplingArchive *,
                                  const SourcePaintDecodeContext &);
  std::shared_ptr<TextureSetCollection>
  collection_at(uint32_t, bool polygon, const PaintImageArchive &,
                const NormalSamplingArchive *, const PaintSamplingArchive *,
                const SourcePaintDecodeContext &);
  Holder decode_holder(const nlohmann::json &, const Material &, bool,
                       const PaintImageArchive &, const NormalSamplingArchive *,
                       const PaintSamplingArchive *,
                       const SourcePaintDecodeContext &);
};
} // namespace dome::detail
