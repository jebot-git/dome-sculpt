// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "texture-set-field-source.hpp"
#include "dome/source-paint.hpp"
#include <nlohmann/json.hpp>
namespace dome::detail {
struct DecodedTextureSetField {
  std::shared_ptr<PaintedMesh>paint;
  std::shared_ptr<const TextureSetFieldSource>source;
  std::shared_ptr<const SourceMaterialBindings>binding;
  std::shared_ptr<const CreatorAppearance>appearance;
  std::shared_ptr<const ObjectBakeRecipe>recipe;
};
class TextureSetFieldCodec {
public:
  using MaterialWriter=std::function<uint32_t(std::shared_ptr<const SourceMaterialBindings>)>;
  using MaterialReader=std::function<std::shared_ptr<const SourceMaterialBindings>(uint32_t)>;
  using GeometryReader=std::function<std::shared_ptr<const Mesh>(uint32_t)>;
  explicit TextureSetFieldCodec(TextureSetLimits = {});
  TextureSetFieldCodec(const nlohmann::json&records,GeometryReader,size_t geometry_count,
      size_t node_count,TextureSetLimits = {});
  nlohmann::json encode(const TextureSetView&,uint32_t geometry,PaintImageArchive&,
      NormalSamplingArchive*,PaintSamplingArchive*,const MaterialWriter&);
  DecodedTextureSetField decode(const nlohmann::json&,
      const std::shared_ptr<const TextureSetFaceDomain>&,uint32_t geometry,const Material&,
      const PaintImageArchive&,const NormalSamplingArchive*,const PaintSamplingArchive*,
      const MaterialReader&,bool allow_polygon,const PaintedMesh*raster_template=nullptr,
      const SourcePaintDecodeContext& = {});
  nlohmann::json records()const;
  std::shared_ptr<const Mesh>geometry_for_node(uint32_t)const;
  void require_complete()const;
private:
  struct Written {
    std::shared_ptr<const TextureSetFieldSource>source;
    uint32_t geometry=0,node=0,binding=0;
    nlohmann::json record;
  };
  struct Read {
    TextureSetView seed;
    std::shared_ptr<const PolygonSurfacePreview>preview;
    uint32_t node=0,geometry=0;
  };
  void preflight()const;
  TextureSetLimits limits_;
  const nlohmann::json*input_=nullptr;
  GeometryReader geometry_;
  size_t geometry_count_=0,node_count_=0;
  std::vector<Written>written_;
  std::vector<std::optional<Read>>read_;
  mutable std::vector<uint8_t>used_;
};
}
