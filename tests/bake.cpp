// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/bake.hpp"
#include <algorithm>
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
namespace {
void check(bool value,const char *message) {
  if (!value) throw std::runtime_error(message);
  std::cout<<"PASS "<<message<<'\n';
}
template<class F>bool rejects(F &&f) {try{f();return false;}catch(const std::exception&){return true;}}
Mesh quad(float left=-1,float right=1,float z=0,float u0=0,float u1=1,float v0=0,float v1=1,Vec3 normal={0,0,1}) {
  Mesh mesh;mesh.positions={{left,-1,z},{right,-1,z},{right,1,z},{left,1,z}};
  mesh.normals.assign(4,normal);mesh.indices={0,1,2,0,2,3};mesh.uv={{{u0,v0}},{{u1,v0}},{{u1,v1}},{{u0,v1}}};return mesh;
}
void append(Mesh &to,const Mesh &from) {
  auto offset=uint32_t(to.positions.size());to.positions.insert(to.positions.end(),from.positions.begin(),from.positions.end());
  to.normals.insert(to.normals.end(),from.normals.begin(),from.normals.end());to.uv.insert(to.uv.end(),from.uv.begin(),from.uv.end());
  for(auto i:from.indices)to.indices.push_back(i+offset);
}
Mesh box(float r) {
  Mesh result;
  std::array<std::array<Vec3,4>,6> faces{{
      {{{-r,-r,r},{r,-r,r},{r,r,r},{-r,r,r}}},
      {{{r,-r,-r},{-r,-r,-r},{-r,r,-r},{r,r,-r}}},
      {{{r,-r,r},{r,-r,-r},{r,r,-r},{r,r,r}}},
      {{{-r,-r,-r},{-r,-r,r},{-r,r,r},{-r,r,-r}}},
      {{{-r,r,r},{r,r,r},{r,r,-r},{-r,r,-r}}},
      {{{-r,-r,-r},{r,-r,-r},{r,-r,r},{-r,-r,r}}}
  }};
  for(auto face:faces){Mesh m;m.positions.assign(face.begin(),face.end());m.normals.assign(4,normalized(cross(face[1]-face[0],face[2]-face[0])));m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.indices={0,1,2,0,2,3};append(result,m);}return result;
}
Image constant(std::array<uint8_t,4> color) {return {1,1,{color[0],color[1],color[2],color[3]}};}
std::array<uint8_t,4> pixel(const Image &image,int x,int y) {
  auto p=image.rgba.data()+(size_t(y)*image.width+x)*4;return {p[0],p[1],p[2],p[3]};
}
bool near(int a,int b,int tolerance=1) {return std::abs(a-b)<=tolerance;}
} // namespace
int main()try {
  auto low=quad();BakeSource source;source.mesh=quad();
  BakeOptions options;options.resolution=32;options.padding=0;options.ao_samples=0;
  auto result=bake_mesh(low,source,options);
  auto normal=pixel(result.normal,16,16),orm=pixel(result.maps.metal_rough,16,16);
  check(result.projected==1024&&!result.missed&&!result.overlapping&&near(normal[0],128)&&near(normal[1],128)&&normal[2]==255,
        "UV edge ownership covers a planar quad once with a neutral tangent normal");
  check(orm[0]==255&&near(orm[1],204)&&orm[2]==0&&pixel(result.ambient_occlusion,16,16)[0]==255,
        "Default material output uses linear R=AO G=roughness B=metal conventions");
  source.mesh.normals.assign(4,{0,.6f,.8f});result=bake_mesh(low,source,options);
  check(near(pixel(result.normal,16,16)[1],204)&&near(pixel(result.normal,16,16)[2],230),
        "High interpolated normals project into the low UV tangent frame");
  options.low_tangents.assign(4,{0,1,0,1});auto explicit_tangent=bake_mesh(low,source,options);
  check(near(pixel(explicit_tangent.normal,16,16)[0],204)&&near(pixel(explicit_tangent.normal,16,16)[1],128),
        "Supplied consumer tangent frames preserve imported/MikkTSpace normal-map conventions");options.low_tangents.clear();
  auto mirrored=quad(-1,1,0,0,1,1,0);auto flipped=bake_mesh(mirrored,source,options);
  check(near(pixel(flipped.normal,16,16)[1],51),"Mirrored UV handedness reverses the tangent-space green component");
  options.normal_y_down=true;auto directx=bake_mesh(mirrored,source,options);options.normal_y_down=false;
  check(near(pixel(directx.normal,16,16)[1],204),"Explicit DirectX Y-down output changes only the normal convention");

  source.mesh=quad(-1,1,.02f);options.cage_positions=low.positions;
  for(auto &p:options.cage_positions)p.z+=.04f;
  check(bake_mesh(low,source,options).projected==1024,"A bounded explicit cage projects onto displaced high detail");
  options.cage_positions[0].z=.1f;
  check(rejects([&]{bake_mesh(low,source,options);}),"Cages outside the front ray budget are rejected");
  options.cage_positions.clear();source.mesh=quad(-1,1,.2f);
  check(rejects([&]{bake_mesh(low,source,options);}),"High surfaces beyond the projection cage are never accepted by a nearest fallback");
  source.mesh=quad();source.mesh.indices={0,2,1,0,3,2};
  check(rejects([&]{bake_mesh(low,source,options);}),"Backface projection is rejected by geometric winding even with misleading vertex normals");
  options.allow_backfaces=true;check(bake_mesh(low,source,options).projected==1024,"Backface transfer is an explicit caller choice");options.allow_backfaces=false;

  source.mesh=quad();auto maps=std::make_shared<TextureMaps>();
  maps->color={2,1,{0,0,0,255,255,255,255,64}};
  maps->metal_rough={2,1,{128,64,32,255,128,192,224,255}};
  maps->emission=constant({128,128,128,255});source.materials[0].maps=maps;
  source.materials[0].material.color={.5f,.5f,.5f,.5f};
  source.materials[0].material.roughness=.5f;source.materials[0].material.metallic=.25f;
  source.materials[0].material.emissive={.5f,.5f,.5f};
  result=bake_mesh(low,source,options);auto color=pixel(result.maps.color,15,16);orm=pixel(result.maps.metal_rough,15,16);
  check(color[0]>175&&color[0]<190&&near(color[3],165)&&orm[0]==128&&near(orm[1],124)&&near(orm[2],122)&&result.maps.transparent,
        "Composited source maps retain alpha and bilinear interpolation occurs in linear RGB");
  source.colors.assign(4,{.5f,1,1,.5f});auto vertex_color=bake_mesh(low,source,options);
  check(pixel(vertex_color.maps.color,15,16)[0]>125&&pixel(vertex_color.maps.color,15,16)[0]<140&&near(pixel(vertex_color.maps.color,15,16)[3],83),
        "Linear imported vertex colors and alpha transfer without an extra sRGB decode");source.colors.clear();
  source.materials[0].maps_are_composited=false;auto factored=bake_mesh(low,source,options);
  check(pixel(factored.maps.color,15,16)[0]>125&&pixel(factored.maps.color,15,16)[0]<140&&near(pixel(factored.maps.color,15,16)[3],83)&&
        near(pixel(factored.maps.metal_rough,15,16)[1],62)&&near(pixel(factored.maps.metal_rough,15,16)[2],31)&&pixel(factored.maps.emission,15,16)[0]<100,
        "Imported material factors apply exactly once to linear channels and sRGB emission");
  source.materials[0].normal_map=std::make_shared<Image>(constant({128,204,230}));
  auto detailed=bake_mesh(low,source,options);
  source.tangents.assign(4,{0,1,0,1});source.materials[0].normal_map=std::make_shared<Image>(constant({204,128,230}));
  auto source_tangent=bake_mesh(low,source,options);
  check(pixel(source_tangent.normal,16,16)[1]>200&&near(pixel(source_tangent.normal,16,16)[0],128),
        "Imported high tangents orient source normal maps before low tangent projection");
  check(pixel(detailed.normal,16,16)[1]>200,"Source tangent normal textures are transferred through the high tangent frame");

  source=BakeSource{};source.mesh=quad();options.ao_samples=64;options.ao_distance=.2f;
  auto open=bake_mesh(low,source,options);
  check(pixel(open.ambient_occlusion,16,16)[0]==255,"A planar open surface does not self-occlude under biased hemisphere rays");
  auto ceiling=quad(-5,5,.08f);for(auto &p:ceiling.positions)p.y*=5;ceiling.indices={0,2,1,0,3,2};ceiling.normals.assign(4,{0,0,-1});append(source.mesh,ceiling);
  auto shadow=bake_mesh(low,source,options);
  check(near(pixel(shadow.ambient_occlusion,16,16)[0],40,8)&&shadow.rays==uint64_t(1024)*65,
        "Cosine hemisphere AO detects a nearby high surface without altering projection bounds");
  options.ao_distance=.04f;auto distant=bake_mesh(low,source,options);
  check(pixel(distant.ambient_occlusion,16,16)[0]==255,"AO rays obey their explicit maximum distance");

  options.ao_samples=0;source.mesh=quad();auto island=quad(-1,1,0,.25f,.75f,.25f,.75f);options.padding=2;
  auto padded=bake_mesh(island,source,options);
  check(padded.projected==256&&padded.padded==144&&padded.coverage[16*32+6]==2&&padded.coverage[0]==0&&
        pixel(padded.normal,6,16)==pixel(padded.normal,8,16)&&pixel(padded.maps.color,0,0)[3]==0,
        "Seam padding copies nearest valid channels only within the requested texel distance");
  options.writable.assign(32*32,1);for(int y=0;y<32;++y)options.writable[y*32+6]=0;
  auto masked=bake_mesh(island,source,options);
  check(masked.coverage[16*32+6]==4&&pixel(masked.maps.color,6,16)[3]==0&&masked.coverage[16*32+7]==2,
        "Writable masks prevent padding from touching protected texture regions");options.writable.clear();
  source.mesh=quad(-1,0);options.fail_on_miss=false;auto missed=bake_mesh(low,source,options);
  check(missed.projected==512&&missed.missed==512&&!missed.padded&&missed.coverage[16*32+24]==3&&pixel(missed.maps.color,24,16)[3]==0,
        "Padding cannot conceal missed projection texels inside a UV island");
  options.fail_on_miss=true;check(rejects([&]{bake_mesh(low,source,options);}),"Incomplete projection fails before any caller can publish partial texture data");

  auto islands=quad(-1,0,0,.1f,.4f,.2f,.8f);append(islands,quad(0,1,0,.6f,.9f,.2f,.8f));
  source=BakeSource{};source.mesh=quad(-1,0);append(source.mesh,quad(0,1));
  source.materials.resize(2);source.materials[0].material.color={1,0,0,1};source.materials[1].material.color={0,0,1,1};source.triangle_materials={0,0,1,1};
  options.padding=4;auto split=bake_mesh(islands,source,options);bool pure=true;
  for(size_t i=0;i<split.coverage.size();++i)if(split.coverage[i]){auto p=split.maps.color.rgba.data()+i*4;pure&=(p[0]==255&&p[2]==0)||(p[0]==0&&p[2]==255);}
  check(pure&&split.padded>0,"Padding across adjacent material islands never blends unrelated seam colors");
  auto overlap=low;append(overlap,low);source=BakeSource{};source.mesh=quad();
  check(rejects([&]{bake_mesh(overlap,source,options);}),"Ambiguous overlapping low UV islands fail explicitly");
  options.fail_on_overlap=false;check(bake_mesh(overlap,source,options).overlapping==1024,"Preview overlap policy reports every conflicting texel");options.fail_on_overlap=true;

  options.padding=0;options.max_rays=10;
  check(rejects([&]{bake_mesh(low,source,options);}),"Bake work cannot exceed the configured ray budget");
  options.max_rays=64'000'000;options.max_raster_samples=10;
  check(rejects([&]{bake_mesh(low,source,options);}),"Even overlapping/empty UV raster work is bounded before ray sampling");options.max_raster_samples=128'000'000;options.max_texels=100;
  check(rejects([&]{bake_mesh(low,source,options);}),"Texture allocation is bounded before raster work");options.max_texels=4'194'304;
  int checks=0;options.cancelled=[&]{return ++checks>10;};options.ao_samples=64;
  check(rejects([&]{bake_mesh(low,source,options);})&&low.indices==std::vector<uint32_t>({0,1,2,0,2,3})&&low.positions[0].x==-1,
        "Cancellation interrupts native projection/AO without modifying input meshes");
  options.cancelled={};options.ao_samples=0;
  auto bad=low;bad.uv[0][0]=2;
  check(rejects([&]{bake_mesh(bad,source,options);}),"Low UV coordinates outside the atlas are rejected");
  bad=low;bad.uv[1]=bad.uv[0];
  check(rejects([&]{bake_mesh(bad,source,options);}),"Degenerate UV tangent frames are reported instead of generating NaNs");
  source.triangle_materials={3,3};check(rejects([&]{bake_mesh(low,source,options);}),"Material assignment errors are validated before sampling");
  source=BakeSource{};source.mesh=quad();auto invalid_maps=std::make_shared<TextureMaps>();
  invalid_maps->color=constant({255,255,255,255});invalid_maps->metal_rough=constant({255,128,128,255});invalid_maps->emission=constant({0,0,0,255});
  invalid_maps->color.rgba.pop_back();source.materials[0].maps=invalid_maps;
  check(rejects([&]{bake_mesh(low,source,options);}),"Malformed material image buffers fail before any texture sampling");
  invalid_maps->color=constant({255,255,255,255});source.mesh.uv.clear();
  check(rejects([&]{bake_mesh(low,source,options);}),"Textured high sources without UV coordinates are rejected explicitly");
  source=BakeSource{};source.mesh=quad();options.low_tangents.assign(4,{1,0,0,.5f});
  check(rejects([&]{bake_mesh(low,source,options);}),"Invalid tangent handedness cannot generate ambiguous normal maps");options.low_tangents.clear();
  options.writable.assign(10,1);check(rejects([&]{bake_mesh(low,source,options);}),"Writable masks must match exact output texel identities");options.writable.clear();
  auto tiny_uv=low;Mesh tiny;tiny.positions={{-.1f,-.1f,0},{.1f,-.1f,0},{0,.1f,0}};tiny.normals.assign(3,{0,0,1});tiny.indices={0,1,2};tiny.uv={{{0,0}},{{1e-7f,0}},{{0,1e-7f}}};append(tiny_uv,tiny);
  check(bake_mesh(tiny_uv,source,options).projected==1024,
        "Tiny nondegenerate subpixel UV triangles do not invalidate otherwise writable atlas data");

  Volume sculpt;sculpt.begin_stroke();Brush clay;clay.radius=.18f;sculpt.stamp(clay);sculpt.end_stroke();
  source=BakeSource{};source.mesh=surface_mesh(sculpt);
  auto dense_uv=source.mesh;unwrap(dense_uv,64,2);double minimum_uv=1;size_t collapsed_uv=0;
  for(size_t t=0;t<dense_uv.indices.size();t+=3){auto a=dense_uv.uv[dense_uv.indices[t]],b=dense_uv.uv[dense_uv.indices[t+1]],c=dense_uv.uv[dense_uv.indices[t+2]];double area=std::abs(double(b[0]-a[0])*(c[1]-a[1])-double(b[1]-a[1])*(c[0]-a[0]));minimum_uv=std::min(minimum_uv,area);collapsed_uv+=area==0;}
  std::cout<<"Dense sculpt atlas diagnostic: min UV determinant="<<minimum_uv<<", exactly collapsed="<<collapsed_uv<<"\n";
  auto sculpt_low=box(.09f);unwrap(sculpt_low,64,2);
  options.resolution=64;options.padding=2;options.front_distance=.15f;
  if(collapsed_uv)check(rejects([&]{bake_mesh(dense_uv,source,options);}),"Actual collapsed atlas triangles are reported rather than silently skipped");
  auto sculpt_bake=bake_mesh(sculpt_low,source,options);
  check(sculpt_bake.projected>1000&&!sculpt_bake.missed&&!sculpt_bake.overlapping&&sculpt_bake.normal.png().size()>100,
        "A native sculpted closed surface bakes through xatlas charts into valid normal PNG data");
  std::cout<<"Native mesh baking tests complete\n";
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
