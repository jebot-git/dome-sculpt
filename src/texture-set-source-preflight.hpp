// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "texture-set-archive.hpp"
#include "polygon-surface-paint.hpp"
#include "material-archive-version.hpp"
namespace dome::detail {
inline size_t texture_wire_ref(const nlohmann::json&v,size_t count){
 if((!v.is_number_unsigned()&&(!v.is_number_integer()||v.get<int64_t>()<0))||v.get<uint64_t>()>=count)throw std::invalid_argument("Retained texture field reference exceeds its actual table");return size_t(v.get<uint64_t>());
}
// Borrow actual shared-table records. This checks source/node/material pairing,
// never mints a field, domain, geometry owner or replay permission.
inline const nlohmann::json*texture_source_seed(const nlohmann::json&paint,
 const nlohmann::json&geometry,const nlohmann::json&materials,const nlohmann::json&pool){
 if(!paint.is_object()||!paint.contains("field_source"))return nullptr;
 if(!geometry.is_object()||geometry.at("version")!=2||!polygon_surface_material_archive(materials)||!pool.is_object()||pool.at("version")!=1||!pool.at("nodes").is_array()||pool.at("nodes").empty()||pool.at("nodes").size()>128)throw std::invalid_argument("Source Paint7 requires its actual geometry2/material21/polygon node pools");
 const auto&sources=geometry.at("field_sources");if(!sources.is_array()||sources.empty()||sources.size()>512||!paint.at("version").is_number_integer()||paint.at("version")!=7||paint.size()!=5||!paint.at("physical_paint").is_boolean()||paint.at("payload").contains("baked"))throw std::invalid_argument("Incomplete actual source Paint7 envelope");
 const auto&seed=sources.at(texture_wire_ref(paint.at("field_source"),sources.size()));
 if(!seed.is_object()||seed.size()!=10||seed.at("version")!=1||seed.at("kind")!="polygon"||!seed.at("physical_paint").is_boolean()||!seed.at("name").is_string()||seed.at("name").get_ref<const std::string&>().size()>256||!seed.at("material").is_object()||seed.at("material").size()!=10||(!seed.at("bake_recipe").is_null()&&!seed.at("bake_recipe").is_object()))throw std::invalid_argument("Incomplete retained source seed");
 (void)texture_wire_ref(seed.at("geometry"),geometry.at("meshes").size());const auto node=texture_wire_ref(seed.at("node"),pool.at("nodes").size());const auto binding=texture_wire_ref(seed.at("binding"),materials.at("bindings").size());
 const auto&actual=materials.at("bindings").at(binding).at("polygon_surface_transfer");if(actual.is_null()||texture_wire_ref(actual,pool.at("nodes").size())!=node)throw std::invalid_argument("Source Paint7 seed and actual material field node differ");
 (void)PolygonSurfacePaintCodec::preflight_grid(seed.at("original"),seed.at("physical_paint").get<bool>(),true);
 (void)PolygonSurfacePaintCodec::preflight_grid(paint.at("payload"),paint.at("physical_paint").get<bool>(),true);
 return &seed;
}
}
