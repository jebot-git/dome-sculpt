// SPDX-License-Identifier: GPL-3.0-or-later
#include "polygon-color-uv.hpp"
#include "polygon-color-target.hpp"
#include "dome/uv.hpp"
#include <type_traits>
namespace dome::detail {
void reparameterize_raw_polygon_colors(Scene &scene,uint64_t id,const Mesh &target,
    std::span<const uint32_t> source_vertices,const SourceUvBakeOptions &options){
 auto cancelled=[&]{if(options.cancelled&&options.cancelled())throw std::runtime_error("Current corner UV transfer cancelled");};cancelled();
 const auto &source=scene.object(id);
 if(!source.polygon||!source.polygon->active||!source.polygon->colors||!source.painted||!source.mesh_active||source.appearance||source.material_bindings||source.painted->color_projection()||source.painted->normal_projection()||!corner_bake_target_supported(source))
  throw std::invalid_argument("Raw corner UV transfer needs its own current field; prepared/source normal and colour interpretations require their own adapters");
 auto derivative=prepare_polygon_uv_derivative(source,target,source_vertices,options.cancelled);
 if(derivative.authority->colors!=source.polygon->colors)
  throw std::invalid_argument("UV-only editing cannot replace or average independent corner colours");
 if(!uv_changed(source.painted->mesh(),derivative.mesh))return;
 auto origin=capture_polygon_origin(scene,id);
 SourceUvBakeInput input;input.paint=source.painted;input.polygon_authority=source.polygon;input.current_deformation=source.deformation;
 input.native_material=SourceNativeMaterial{source.material.color,source.material.emissive,int(source.material.alpha),source.material.alpha_cutoff,source.material.double_sided};
 input.target=std::move(derivative.mesh);input.source_vertices=std::move(derivative.source_vertices);
 auto adopted=adopt_source_uv_bake(bake_source_uv(input,options),options.cancelled);cancelled();
 auto authority=std::make_shared<PolygonAuthority>(*derivative.authority);authority->origin=std::move(origin);
 Scene prepared=scene;auto &object=prepared.object(id);object.painted=std::move(adopted.paint);object.deformation=std::move(adopted.deformation);object.appearance=std::move(adopted.appearance);object.material_bindings=std::move(adopted.material_bindings);object.polygon=std::move(authority);
 prepared.validate();cancelled();static_assert(std::is_nothrow_move_assignable_v<Scene>);scene=std::move(prepared);
}
}
