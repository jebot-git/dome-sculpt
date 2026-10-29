// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/bake.hpp"
#include "dome/document.hpp"
#include "dome/tangents.hpp"
#include "dome/source-maps.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
#include <unistd.h>
using namespace dome;
namespace {
void check(bool condition, const char *message) {
  if (!condition) throw std::runtime_error(message);
  std::cout << "PASS " << message << '\n';
}
template<class F> bool rejects(F f) {
  try { f(); return false; } catch (const std::exception &) { return true; }
}
Mesh quad() {
  Mesh m;
  m.positions = {{-1,-1,0},{1,-1,0},{1,1,0},{-1,1,0}};
  m.normals.assign(4,{0,0,1}); m.indices = {0,1,2,0,2,3};
  m.uv = {{{.125f,.125f}},{{.875f,.125f}},{{.875f,.875f}},{{.125f,.875f}}};
  return m;
}
Image constant(std::array<uint8_t,4> value) {
  Image image{64,64,std::vector<uint8_t>(64*64*4)};
  for (size_t i=0;i<image.rgba.size();++i) image.rgba[i]=value[i%4];
  return image;
}
BakedTextures textures() {
  BakedTextures b;
  b.maps.color=constant({25,50,220,128});
  b.maps.metal_rough=constant({80,60,140,255});
  b.maps.emission=constant({90,0,0,255});
  b.maps.normal=constant({128,204,230,255});
  b.coverage.assign(64*64,255);
  return b;
}
std::array<uint8_t,4> pixel(const Image &i,int x=32,int y=32) {
  auto at=(size_t(y)*i.width+x)*4;
  return {i.rgba[at],i.rgba[at+1],i.rgba[at+2],i.rgba[at+3]};
}
Vec3 normal(const Image &image) {
  auto p=pixel(image);
  return normalized(Vec3{p[0]/127.5f-1,p[1]/127.5f-1,p[2]/127.5f-1});
}
} // namespace
int main() try {
  auto mesh=quad(); auto frames=tangent_corners(mesh);
  check(frames.size()==mesh.indices.size() && frames[0][0]>.999f && frames[0][3]==1,
        "Native MikkTSpace keeps per-corner canonical tangent frames");
  auto curved=mesh;curved.normals={{0,0,1},normalized({.6f,0,1}),normalized({.2f,.5f,1}),normalized({0,.4f,1})};
  auto frame=tangent_frame(curved,tangent_corners(curved),0,{.2f,.3f,.5f});
  auto detail=normalized(Vec3{.25f,.4f,.8f});
  check(length(frame.to_tangent(frame.to_world(detail))-detail)<1e-5f,
        "Baking exactly inverts the interpolated shader frame on curved normals");
  auto mirrored=mesh;for(auto &uv:mirrored.uv)uv[0]=1-uv[0];
  auto mirror=tangent_corners(mirrored);
  check(mirror[0][0]<-.999f && mirror[0][3]==-1,
        "Mirrored charts preserve tangent handedness rather than averaging it away");
  check(rejects([&]{tangent_corners(mesh,[]{return true;});}),
        "Cancelled tangent requests publish no partial frame array");
  PaintedMesh painted(mesh,64); Material material;
  painted.install_bake(textures());
  auto before=painted.encode();auto maps=painted.composite(material);
  check(pixel(maps->color)==std::array<uint8_t,4>{25,50,220,128} && maps->transparent &&
        pixel(maps->metal_rough)==std::array<uint8_t,4>{80,60,140,255} && pixel(maps->emission)[0]==90,
        "Baked intrinsic PBR alpha, AO and sRGB emission survive compositing without doubled factors");
  check(length(normal(maps->normal)-normalized(Vec3{0,.6f,.8f}))<.01f,
        "The composited surface carries linear tangent normal detail");
  auto held=painted.snapshot();
  PaintSettings green;green.color={0,1,0,1};green.front_only=false;
  Brush brush;brush.radius=3;brush.from=brush.to={};
  painted.begin_stroke();painted.stamp(brush,green);painted.end_stroke();
  auto over=painted.composite(material);
  check(pixel(over->color)[1]==255 && over->normal.rgba==maps->normal.rgba &&
        pixel(over->metal_rough)[0]==80 && held->encode()==before,
        "Editable paint overlays retain baked normals/AO and immutable held snapshots");
  check(painted.undo() && painted.encode()==before,
        "Paint-stroke undo restores the exact baked surface and paint layers");
  auto rotated=mesh;for(auto &uv:rotated.uv)uv={1-uv[1],uv[0]};
  auto changed=painted.reparameterize(rotated);
  auto rotated_maps=changed->composite(material);
  auto old_world=tangent_frame(mesh,frames,0,{.2f,.3f,.5f}).to_world(normal(maps->normal));
  auto new_world=tangent_frame(rotated,tangent_corners(rotated),0,{.2f,.3f,.5f}).to_world(normal(rotated_maps->normal));
  check(length(old_world-new_world)<.012f && pixel(rotated_maps->normal)[0]<70 &&
        pixel(rotated_maps->metal_rough)==pixel(maps->metal_rough),
        "Rotating UVs reorients the tangent normal while retaining object-space detail and AO");
  auto decoded=PaintedMesh::decode(changed->encode());
  check(decoded->encode()==changed->encode() && decoded->composite(material)->normal.rgba==rotated_maps->normal.rgba,
        "Baked color, normal, AO and coverage round-trip through native editable persistence");
  auto partial=textures();partial.maps.color=constant({255,0,0,255});partial.coverage.assign(64*64,0);
  partial.coverage[32*64+32]=128;
  painted.install_bake(partial);
  auto merged=painted.composite(material);
  float red=srgb_to_linear(25/255.f)*(127/255.f)+128/255.f;
  check(pixel(merged->color)[0]==uint8_t(std::lround(linear_to_srgb(red)*255)) &&
        pixel(merged->color,31,32)==pixel(maps->color,31,32),
        "Masked rebaking blends in linear light and leaves protected destination texels exact");
  auto intact=painted.encode();auto invalid=textures();invalid.coverage.pop_back();
  check(rejects([&]{painted.install_bake(invalid);}) && painted.encode()==intact,
        "Malformed bake coverage fails before replacing editable state");
  auto corrupt=intact;corrupt["baked"]["normal"]=nlohmann::json::binary(std::vector<uint8_t>{1,2,3});
  check(rejects([&]{PaintedMesh::decode(corrupt);}),"Malformed persisted normal buffers are rejected");
  Scene scene;scene.active().painted=painted.snapshot();scene.active().mesh_active=true;
  auto encoded=scene.encode();
  check(encoded["version"]==6 && Scene::decode(encoded).encode()==encoded,
        "Scene version 6 preserves baked surfaces without changing authoring vertex identities");
  auto legacy=encoded;legacy["version"]=5;
  for(auto key:{"groups","next_group_id","source_images","source_models"})legacy.erase(key);
  for(auto &object:legacy["objects"]) {
    object.erase("group");object.erase("appearance");
    for(auto key:{"alpha","alpha_cutoff","unlit"})object["material"].erase(key);
  }
  auto legacy_loaded=Scene::decode(legacy);
  check(legacy_loaded.active().painted->encode()==painted.encode() && legacy_loaded.encode()["version"]==6,
        "The pre-integration baked-v5 checkpoint upgrades without confusing grouped creator archives");
  auto originals=std::make_shared<PaintSourceMaps>();
  originals->color.image=std::make_shared<const Image>(constant({255,0,0,255}));
  auto combined=PaintedMesh::from_maps(mesh,originals,64);
  auto original_frame=combined->composite(material);
  auto overlay=textures();overlay.coverage.assign(64*64,0);
  for(int y=0;y<64;++y)for(int x=32;x<64;++x)overlay.coverage[y*64+x]=255;
  combined->install_bake(overlay);auto combined_frame=combined->composite(material);
  check(combined_frame!=original_frame && combined_frame->source_maps &&
        combined_frame->source_maps->color.image==originals->color.image &&
        pixel(combined_frame->color,24,32)==pixel(original_frame->color,24,32) &&
        pixel(combined_frame->color,40,32)==std::array<uint8_t,4>{25,50,220,128},
        "Baking invalidates cached composites and preserves original source images beneath protected texels");
  Scene combined_scene;combined_scene.active().painted=combined;combined_scene.active().mesh_active=true;
  auto combined_encoded=combined_scene.encode();auto combined_loaded=Scene::decode(combined_encoded);
  check(combined_loaded.encode()==combined_encoded && combined_loaded.active().painted->source_maps() &&
        combined_loaded.active().painted->baked(),
        "Scene v6 round-trips original shared image archives and baked maps together");
  encoded["version"]=4;
  check(rejects([&]{Scene::decode(encoded);}),"A baked scene cannot masquerade as a lossy older schema");
  Document doc;doc.replace(scene);auto prior=doc.copy_scene().encode();
  doc.edit_scene("Clear bake",[](Scene &s,const auto&){s.active().painted->clear_bake();});
  check(doc.wait_asset() && doc.frame().state->error.empty() && !doc.copy_scene().active().painted->baked() &&
        doc.undo() && doc.copy_scene().encode()==prior,
        "A guarded worker edit and global Undo retain the exact baked asset");
  check(doc.wait_mesh() && doc.frame().surface->tangents.size()==doc.frame().surface->mesh.positions.size() &&
        doc.frame().surface->mesh.positions.size()==mesh.indices.size(),
        "Meshing workers expand render tangent corners without changing editable topology");
  BakeSource source;source.mesh=curved;source.materials[0].maps=held->composite(material);
  BakeOptions options;options.resolution=64;options.padding=2;options.ao_samples=0;
  auto rebaked=bake_mesh(curved,source,options);
  check(length(normal(rebaked.normal)-normal(maps->normal))<.015f &&
        pixel(rebaked.maps.metal_rough)[0]==80,
        "Baking a previously baked source retains automatic normal-map detail and source AO");
  auto original_views=std::make_shared<PaintSourceMaps>();
  original_views->color.image=std::make_shared<const Image>(constant({128,64,255,128}));
  original_views->normal.image=std::make_shared<const Image>(constant({204,128,230,255}));
  original_views->normal.uv_transform={0,-1,1,0,1,0};original_views->normal_scale=.5f;
  original_views->occlusion.image=std::make_shared<const Image>(constant({128,0,0,255}));original_views->occlusion_strength=.6f;
  auto analytic=normalized(Vec3{.0039215686f*.5f,-.6f*.5f,230/127.5f-1});
  check(length(source_texture_normal(mesh,*original_views,0,{.2f,.3f,.5f})-analytic)<.001f,
        "Original normal sampling rotates the affine UV basis and applies strength exactly once");
  auto mirrored_views=*original_views;mirrored_views.normal.uv_transform={-1,0,0,1,1,0};
  check(source_texture_normal(mesh,mirrored_views,0,{.2f,.3f,.5f}).x<-.3f,
        "Mirrored original normal views retain tangent handedness");
  mirrored_views.normal.uv_transform={0,0,0,0,.5f,.5f};
  check(length(source_texture_normal(mesh,mirrored_views,0,{.2f,.3f,.5f})-Vec3{0,0,1})<1e-6f,
        "Singular original normal UV transforms retain geometric shading instead of inventing a tangent");
  auto original_paint=PaintedMesh::from_maps(mesh,original_views,64);Material original_material;
  original_material.color={.5f,.7f,.4f,.75f};
  BakeSource raw_source;raw_source.mesh=mesh;raw_source.materials[0].material=original_material;
  raw_source.materials[0].maps=original_paint->preview(original_material);
  auto raw_bake=bake_mesh(mesh,raw_source,options);
  check(length(normal(raw_bake.normal)-analytic)<.012f &&
        std::abs(int(pixel(raw_bake.maps.metal_rough)[0])-179)<=1 &&
        std::abs(int(pixel(raw_bake.maps.color)[0])-92)<=1 &&
        std::abs(int(pixel(raw_bake.maps.color)[3])-96)<=1,
        "Baking original GPU bindings retains transformed normals, independent AO, linear alpha and one application of material factors");
  raw_source.tangents.assign(mesh.positions.size(),{1,0,0,1});
  auto explicit_raw=bake_mesh(mesh,raw_source,options);
  check(normal(explicit_raw.normal).x>.3f && std::abs(normal(explicit_raw.normal).y)<.015f,
        "Explicit imported tangent frames remain authoritative for original normal views");
  auto protected_bake=textures();
  for(int y=0;y<64;++y)for(int x=0;x<32;++x)protected_bake.coverage[y*64+x]=0;
  original_paint->install_bake(std::move(protected_bake));
  auto protected_maps=original_paint->composite(original_material);
  auto decode_at=[&](int x){auto p=pixel(protected_maps->normal,x,32);return normalized(Vec3{p[0]/127.5f-1,p[1]/127.5f-1,p[2]/127.5f-1});};
  check(length(decode_at(24)-analytic)<.012f && decode_at(40).y>.59f,
        "Protected normal texels retain original affine-UV detail while covered texels use the canonical baked basis");
  Scene raw_authoring;raw_authoring.active().mesh_active=true;
  raw_authoring.active().painted=std::make_shared<PaintedMesh>(mesh,64);auto raw_target=raw_authoring.active().id;
  auto raw_id=raw_authoring.add("Original source detail",Volume{});
  raw_authoring.active().mesh_active=true;raw_authoring.active().visible=false;
  raw_authoring.active().material=original_material;
  raw_authoring.active().painted=PaintedMesh::from_maps(mesh,original_views,64);
  auto raw_before=raw_authoring.active().painted->encode();raw_authoring.select(raw_target);
  ObjectBakeSettings raw_settings;raw_settings.ao_samples=0;raw_settings.padding=2;
  auto raw_composites=paint_composite_cache_stats().builds;
  bake_object(raw_authoring,raw_id,raw_settings);
  check(paint_composite_cache_stats().builds==raw_composites,
        "Original source object baking avoids a preliminary CPU composite and a second image resampling pass");
  check(raw_authoring.object(raw_id).painted->encode()==raw_before &&
        length(normal(raw_authoring.active().painted->composite(raw_authoring.active().material)->normal)-analytic)<.012f,
        "Native object baking now transfers original source normal views without changing their retained archive");
  Scene authoring;
  auto &low=authoring.active();
  low.transform={{.2f,.3f,-.1f},{0,0,.70710678f,.70710678f},2};
  low.mesh_active=true;low.painted=std::make_shared<PaintedMesh>(mesh,64);
  low.painted->add_layer("Existing target paint");
  low.painted->layer_properties("Existing target paint",false,.35f);
  DeformationState target_rig(mesh);
  target_rig.skeleton=Skeleton{{{"root",-1,{},.2f}}};
  target_rig.skin=SkinWeights{};target_rig.skin->vertices.assign(4,{{0,1}});
  authoring.set_deformation(low.id,target_rig);
  auto target_id=low.id;auto transform=low.transform;transform.scale=1;
  auto source_id=authoring.add("Retained detail",Volume{},transform);
  auto &high=authoring.active();auto high_mesh=mesh;
  for(auto &p:high_mesh.positions)p=p*2;
  high.mesh_active=true;high.visible=false;high.locked=true;
  high.painted=std::make_shared<PaintedMesh>(high_mesh,64);
  high.material.color={.1f,.3f,.8f,.6f};
  DeformationState source_rig(high_mesh);
  source_rig.skeleton=Skeleton{{{"root",-1,{},.2f}}};
  source_rig.skin=SkinWeights{};source_rig.skin->vertices.assign(4,{{0,1}});
  source_rig.pose={source_rig.skeleton->bones[0].rest};source_rig.pose[0].translation.z=.02f;
  authoring.set_deformation(source_id,source_rig);
  authoring.select(target_id);
  auto source_before=authoring.encode();
  ObjectBakeSettings settings;settings.front_mm=25;settings.back_mm=5;settings.ao_samples=0;settings.padding=2;
  auto statistics=bake_object(authoring,source_id,settings);
  auto baked_scene=authoring.encode();
  check(statistics.projected>2000 && statistics.rays>=statistics.projected &&
        baked_scene["objects"][1]==source_before["objects"][1] &&
        baked_scene["objects"][0]["deformation"]==source_before["objects"][0]["deformation"] &&
        baked_scene["objects"][0]["painted"]["layers"]==source_before["objects"][0]["painted"]["layers"],
        "Object baking samples a hidden posed source with exact target rig/paint and source retention");
  auto original_source_maps=authoring.object(source_id).painted->composite(authoring.object(source_id).material);
  auto target_maps=authoring.active().painted->composite(authoring.active().material);
  check(pixel(target_maps->color)==pixel(original_source_maps->color),
        "Object/world transforms and world-millimetre projection bounds preserve material factors exactly once");
  auto failed=Scene::decode(source_before);auto short_cage=settings;short_cage.front_mm=10;
  check(rejects([&]{bake_object(failed,source_id,short_cage);}) && failed.encode()==source_before,
        "A short world-space cage fails atomically instead of falling back to an unrelated surface");
  check(rejects([&]{bake_object(failed,source_id,settings,[]{return true;});}) && failed.encode()==source_before,
        "Cancelled authoring bake never replaces target channels or source state");
  auto grouped=failed;auto group=grouped.add_group("Scaled bake group",{{.1f,.2f,0},{},1.5f});
  grouped.object(source_id).group=group;grouped.object(target_id).group=group;
  auto grouped_before=grouped.encode();
  check(rejects([&]{bake_object(grouped,source_id,settings);}) && grouped.encode()==grouped_before,
        "Group world scale keeps a 25 mm cage shorter than the 30 mm displayed source offset");
  auto longer=settings;longer.front_mm=45;
  check(bake_object(grouped,source_id,longer).projected>2000 && grouped.active().group==group,
        "A sufficient world-space cage bakes grouped sources without reparenting the target");
  auto posed=failed;auto active_rig=*posed.active().deformation;
  active_rig.pose={active_rig.skeleton->bones[0].rest};active_rig.pose[0].translation.z=.1f;
  posed.set_deformation(target_id,active_rig);auto posed_before=posed.encode();
  check(rejects([&]{bake_object(posed,source_id,settings);}) && posed.encode()==posed_before,
        "Unsupported posed-target baking fails without changing its editable rig or surfaces");
  doc.replace(Scene::decode(source_before));doc.bake_from_object(source_id,settings);
  check(doc.wait_asset() && doc.frame().state->error.empty() &&
        doc.copy_scene().active().painted->baked() && doc.undo() &&
        doc.copy_scene().encode()==source_before && doc.redo(),
        "The native bake API queues one guarded worker transaction with exact global Undo/Redo");
  auto path=std::filesystem::temp_directory_path()/("dome-baked-surface-"+std::to_string(getpid())+".dsculpt");
  doc.save(path);auto restored=Scene::load(path);std::filesystem::remove(path);
  check(restored.active().painted->encode()==doc.copy_scene().active().painted->encode() &&
        restored.active().deformation->encode(restored.active().painted->mesh())==
          doc.copy_scene().active().deformation->encode(restored.active().painted->mesh()),
        "Ordered native saving retains baked channels and editable rig/paint state");
  return 0;
} catch(const std::exception &e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
