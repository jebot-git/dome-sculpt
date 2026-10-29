// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/material-visibility.hpp"
#include "dome/document.hpp"
#include "dome/humanoid-creator.hpp"
#include "dome/source-maps.hpp"
#include <iostream>
using namespace dome;
namespace {
int checks = 0;
void check(bool value, const char *label) { if (!value) throw std::runtime_error(label); ++checks; std::cout << "PASS " << label << '\n'; }
template<class F> bool rejects(F f) { try { f(); return false; } catch (...) { return true; } }
Mesh quad() {
  Mesh m; m.positions={{-1,-1,0},{1,-1,0},{1,1,0},{-1,1,0}};
  m.indices={0,1,2,0,2,3}; m.normals.assign(4,{0,0,1});
  m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}}; return m;
}
std::shared_ptr<const Image> split_alpha() {
  return std::make_shared<const Image>(Image{2,1,{255,255,255,0,255,255,255,255}});
}
std::shared_ptr<CreatorAppearance> appearance(MToonMaterial m, bool secondary, bool frozen) {
  auto a=std::make_shared<CreatorAppearance>(); auto asset=std::make_shared<ImportedAsset>();
  asset->materials.emplace_back(); asset->materials[0].double_sided=true;
  asset->images.resize(frozen?2:1);
  ImportedTexture texture; texture.image=0; texture.mag_filter=texture.min_filter=9728;
  asset->textures.push_back(texture); if(frozen){texture.image=1;asset->textures.push_back(texture);}
  ImportedPrimitive p; p.mesh=quad(); p.source_mesh=p.source_primitive=0;
  p.uv1.assign(4,{.75f,.5f}); asset->primitives.push_back(p);
  a->asset=asset; a->material=a->source_mesh=a->source_primitive=0; a->source_vertices={0,1,2,3};
  std::vector<std::shared_ptr<const Image>> images{split_alpha()};
  if(frozen)images.push_back(std::make_shared<const Image>(Image{1,1,{0,0,0,255}}));
  a->decoded_images=std::make_shared<const std::vector<std::shared_ptr<const Image>>>(images);
  m.textures[0].index=0;m.textures[0].texcoord=secondary?1:0;
  if(frozen)m.textures[size_t(MToonSlot::UvMask)].index=1;
  if(m.version()==MToonVersion::V0)a->mtoon_json=m.source_json().dump();
  else a->material_json=m.source_json().dump(); return a;
}
}
int main() try {
  auto m=quad(); Material material; material.alpha=Material::AlphaMode::Mask;
  auto source=std::make_shared<PaintSourceMaps>(); source->color.image=split_alpha();
  source->color.mag_filter=source->color.min_filter=9728;
  auto paint=PaintedMesh::from_maps(m,source,64);
  SurfaceRange range{1,0,6,material,paint->preview(material)};
  auto hit=[&](const SurfaceRange &r, Vec3 origin, double seconds=0) {
    MaterialSurfaceQuery query(m,std::span<const SurfaceRange>(&r,1));return query.raycast(origin,{0,0,-1},4,seconds);
  };
  check(!hit(range,{-.5f,0,2})&&hit(range,{.5f,0,2}),
        "Original nearest-sampled alpha holes are absent from material picking");
  auto affine=std::make_shared<PaintSourceMaps>(*source);affine->color.uv_transform={-1,0,0,1,1,0};
  range.textures=PaintedMesh::from_maps(m,affine,64)->preview(material);
  check(hit(range,{-.5f,0,2})&&!hit(range,{.5f,0,2}),
        "Picking follows authored affine UV transforms and wrapping");
  range.material.alpha=Material::AlphaMode::Opaque;
  check(hit(range,{.5f,0,2}).has_value(),"Explicit Opaque retains coverage independently of transparent source texels");
  range.material.alpha=Material::AlphaMode::Mask;range.textures=paint->composite(material);
  check(!hit(range,{-.5f,0,2})&&hit(range,{.5f,0,2}),"Derived material coverage retains protected source alpha holes");
  range.material.color[3]=.4f;range.textures=paint->preview(range.material);
  check(!hit(range,{.5f,0,2}),"Material opacity participates in alpha-mask cutoff");
  range.material.alpha=Material::AlphaMode::Blend;range.material.color[3]=0;
  check(!hit(range,{.5f,0,2}),"Fully transparent blended faces do not occlude picking");
  range.material.color[3]=1;range.textures=paint->preview(range.material);
  MaterialSurfaceQuery front(m,std::span<const SurfaceRange>(&range,1));
  check(!front.raycast({.5f,0,-2},{0,0,1},4),"Back-face culling uses displayed triangle winding");
  range.material.double_sided=true;MaterialSurfaceQuery both(m,std::span<const SurfaceRange>(&range,1));
  check(both.raycast({.5f,0,-2},{0,0,1},4).has_value(),"Double-sided materials are pickable from both sides");
  auto stacked=m;
  for(auto p:m.positions){p.z=-.0000001f;stacked.positions.push_back(p);}
  stacked.normals.insert(stacked.normals.end(),m.normals.begin(),m.normals.end());
  stacked.uv.insert(stacked.uv.end(),m.uv.begin(),m.uv.end());
  for(auto id:m.indices)stacked.indices.push_back(id+4);
  Material opaque; opaque.double_sided=true;opaque.alpha=Material::AlphaMode::Opaque;
  range.material=material;range.textures=paint->preview(material);
  std::vector<SurfaceRange> ranges{range,{2,6,6,opaque}};
  MaterialSurfaceQuery layered(stacked,ranges);
  auto behind=layered.raycast({-.5f,0,2},{0,0,-1},4);
  check(behind&&behind->triangle>=2&&layered.raycast({.5f,0,2},{0,0,-1},4)->triangle<2,
        "Filtered BVH traversal finds the next visible face without epsilon-skipping nearby geometry");
  SurfaceQuery geometric(stacked);SurfaceQueryOptions filter;
  filter.ray_filter=[](uint32_t triangle,const auto &){return triangle>=2;};
  check(geometric.raycast({0,0,2},{0,0,-1},4,filter)->triangle>=2,"Surface-ray filters retain exact original triangle identities");
  filter.max_triangle_tests=1;
  check(rejects([&]{geometric.raycast({0,0,2},{0,0,-1},4,filter);}),"Filtered rays enforce their triangle budget");
  filter.max_triangle_tests=10;filter.cancelled=[]{return true;};
  check(rejects([&]{geometric.raycast({0,0,2},{0,0,-1},4,filter);}),"Filtered ray cancellation fails before publishing a hit");
  for(bool legacy:{false,true}) {
    auto toon=legacy?MToonMaterial::from_vrm0({{"shader","VRM/MToon"},{"floatProperties",{{"_BlendMode",1},{"_CullMode",0}}}}):
        MToonMaterial::from_vrm1({{"alphaMode","MASK"},{"doubleSided",true},{"extensions",{{"VRMC_materials_mtoon",{{"specVersion","1.0"}}}}}});
    range={1,0,6,opaque};range.appearance=appearance(toon,!legacy,false);
    Vec3 origin{legacy?.5f:-.5f,0,2};
    check(hit(range,origin).has_value(),"V0 UV0 and V1 authored UV1 retain their actual base-alpha coordinates");
    toon.parameters.scroll={.5f,0};range.appearance=appearance(toon,!legacy,false);
    check(hit(range,origin,0)&&!hit(range,origin,1),"V0/V1 texture animation moves alpha coverage at the shared frame time");
    range.appearance=appearance(toon,!legacy,true);
    check(hit(range,origin,1).has_value(),"The authored MToon UV-animation mask freezes alpha coverage correctly");
    range.material.alpha=Material::AlphaMode::Blend;range.material.color[3]=0;
    check(!hit(range,origin,0),"Native opacity overrides participate in retained MToon alpha visibility");
  }
  {
    auto toon=MToonMaterial::from_vrm1({{"alphaMode","MASK"},{"doubleSided",true},{"extensions",{{"VRMC_materials_mtoon",{{"specVersion","1.0"}}}}}});
    ranges[0].appearance=appearance(toon,true,false);ranges[0].first_vertex=4;
    check(rejects([&]{MaterialSurfaceQuery invalid(stacked,ranges);}),
          "Invalid packed UV1 ranges fail on the worker before any input query");
  }
  check(rejects([&]{MaterialSurfaceQuery invalid(m,{});}),"Incomplete picking surface ranges fail before publication");
  std::cout<<"CHECKS "<<checks<<'\n';return 0;
} catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
