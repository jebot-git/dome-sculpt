// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
// Observations only: one genuine family, no normal/material image rewriting.
namespace advanced_render::diagnose {
int run(const std::filesystem::path &output){
 std::ofstream stream(output);check(bool(stream),"Diagnostic creates independent observation log");
 auto family=construct(false,false,false,SourceNormalUvMethod::Rasterize);
 for(bool pbr:{true,false}){
  auto shown=display::scene(family.scene,family.source,pbr);for(const auto &o:shown.objects())shown.object(o.id).visible=o.id==family.source;shown.select(family.source);
  const auto before=shown.encode();const auto &o=shown.active();Document doc;doc.replace(shown.snapshot());if(pbr)doc.shading(false);else doc.authored_shading();check(doc.wait_mesh()&&doc.frame().state->error.empty(),"Diagnostic actual Document packet ready");
  const auto surface=doc.frame().surface;
  for(const auto &range:surface->ranges)if(range.object==family.source){
   const auto &binding=*range.material_bindings;const auto &m=binding.material.parameters;
   nlohmann::json row{{"pbr",pbr},{"range_pbr",range.pbr_view},{"source_frame",range.source_frame},{"canonical_frame",range.canonical_frame},{"normal_scale",m.normal_scale},{"normal_enabled",m.normal_enabled},{"shade",std::array<float,3>{m.shade_color.x,m.shade_color.y,m.shade_color.z}},{"toony",m.toony},{"native_color",range.material.color},{"typed_base",m.base_color},{"native_reference",binding.native_reference.color},{"first",range.first},{"count",range.count}};
   auto maps=[&](const TextureMaps &t){nlohmann::json j{{"source_only",t.source_only},{"source_maps",bool(t.source_maps)},{"overlay",bool(t.overlay)},{"current",bool(t.current_grid_preview)},{"polygon",bool(t.polygon_surface)},{"normal_projection",bool(t.normal_projection)},{"color_projection",bool(t.color_projection)},{"baked_base",bool(t.baked_base)}};
    for(const auto &[name,im]:std::array<std::pair<const char*,const Image*>,8>{{{"color",&t.color},{"coverage",&t.color_coverage},{"normal",&t.normal},{"mtoon",&t.mtoon_normal},{"emission",&t.emission},{"emission_coverage",&t.emission_coverage},{"baked_normal",&t.baked_normal},{"baked_coverage",&t.baked_coverage}}})j[name]={im->width,im->height,im->rgba.size()};return j;};
   row["actual_maps"]=maps(*range.textures);row["preview_maps"]=maps(*o.painted->preview(o.material));
   for(size_t k=0;k<3;++k){const auto v=surface->mesh.indices.at(range.first+k);nlohmann::json c{{"vertex",v},{"position",prepared_pixel_oracle::vc(surface->mesh.positions.at(v))},{"normal",prepared_pixel_oracle::vc(surface->mesh.normals.at(v))},{"uv",surface->mesh.uv.at(v)}};
    if(v<surface->source_normals.size())c["source_normal"]=prepared_pixel_oracle::vc(surface->source_normals[v]);if(v<surface->source_tangents.size())c["source_tangent"]=surface->source_tangents[v];if(v<surface->normal_coordinates.size())c["normal_coordinates"]=surface->normal_coordinates[v];if(v<surface->paint_coordinates.size())c["paint_coordinates"]=surface->paint_coordinates[v];row["corners"].push_back(c);}
   stream<<row.dump()<<'\n';
  }
  check(shown.encode()==before&&doc.copy_scene().encode()==before,"Diagnostic preserves exact displayed scene");
 }
 std::cout<<"PASS CPU advanced display packet diagnosis; no GL context or normal/material mutation\n";return 0;
}
}
