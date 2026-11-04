// SPDX-License-Identifier: GPL-3.0-or-later
#define main source_marker_original_renderer_suite
#include "mtoon-render.cpp"
#undef main
#include "dome/material-bindings.hpp"
#include <nlohmann/json.hpp>
int main()try{
 auto maps=std::make_shared<PaintSourceMaps>();Image pattern{64,64,{}};
 for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x)pattern.rgba.insert(pattern.rgba.end(),{uint8_t(x*4),uint8_t(y*4),uint8_t((x^y)*4),uint8_t(x<24?0:255)});
 maps->color.image=std::make_shared<const Image>(std::move(pattern));maps->color.min_filter=maps->color.mag_filter=9728;
 auto paint=PaintedMesh::from_maps(quad(),maps,64);auto mat=white();mat.alpha=Material::AlphaMode::Mask;
 auto origin=std::make_shared<SourceUvOrigin>();origin->paint=paint->snapshot();
 auto binding=std::make_shared<SourceMaterialBindings>();binding->has_mtoon=false;binding->uv_origin=origin;
 Scene scene;scene.active().mesh_active=true;scene.active().painted=paint;scene.active().material=mat;scene.active().material_bindings=binding;
 const auto encoded=scene.encode();
 std::weak_ptr<const TextureMaps> marker;
 {
  auto first=paint->preview(mat);marker=first;
  bool repeated=true;for(unsigned n=0;n<256;++n){auto second=paint->preview(mat);repeated&=second==first;}check(repeated,"Live caller retains exact source marker on256 repeated previews");
  check(first->source_only&&first->source_maps==paint->source_maps(),"Source marker owns genuine immutable source maps");
 }
 check(marker.expired(),"Historical original paint/cache must not strongly retain dead render marker");
 // A real native worker MeshSnapshot, not a fixture-owned side cache, keeps
 // the source marker alive across both eyes and while the next frame is built.
 std::shared_ptr<const MeshSnapshot> live;
 {
  Document document;document.replace(scene);
  check(document.wait_mesh(std::chrono::seconds(120)),"Actual native source snapshot ready");
  live=document.frame().surface;
  check(live&&live->ranges.size()==1&&live->ranges[0].textures,"Actual native range retains source marker");
  marker=live->ranges[0].textures;
  auto same=paint->preview(mat);
  check(same==live->ranges[0].textures,"Native live frame and repeated source view share exact marker");
  check(origin->paint->preview(mat)==same,"Original immutable UV origin reuses live marker without becoming its strong cache");
 }
 check(!marker.expired(),"Actual native snapshot alone keeps source texture lifetime after Document destruction");
 NativeGraphics graphics;std::cout<<"GL_RENDERER "<<reinterpret_cast<const char*>(glGetString(GL_RENDERER))<<'\n';Fbo fbo;fbo.bind();SurfaceRenderer renderer;
 auto direct=snapshot(mat);direct->ranges[0].textures=paint->preview(mat);
 check(direct->ranges[0].textures==live->ranges[0].textures,"Renderer source view still shares actual frame-owned marker");
 const auto baseline=draw(renderer,direct);const auto first_eye=draw(renderer,live);const auto eye_textures=renderer.texture_stats();const auto second_eye=draw(renderer,live);
 check(renderer.texture_stats().uploads==eye_textures.uploads&&renderer.texture_stats().pack_builds==eye_textures.pack_builds,"Second eye reuses actual source image uploads/coordinate packs");
 check(delta(baseline,first_eye)==0&&delta(first_eye,second_eye)==0,"Actual native frame and both eyes preserve exact original source pixels");
 const auto builds=renderer.buffer_stats();const auto texture_builds=renderer.texture_stats();
 bool repeated_pixels=true;for(unsigned n=0;n<32;++n)repeated_pixels&=delta(first_eye,draw(renderer,live))==0;check(repeated_pixels,"Same live frame32 repeated source images exact");
 check(renderer.buffer_stats().geometry_uploads==builds.geometry_uploads,"Same live source frame reuses geometry between eyes/repeats");
 check(renderer.texture_stats().uploads==texture_builds.uploads&&renderer.texture_stats().pack_builds==texture_builds.pack_builds,"Repeated live frame keeps real image/coordinate upload counters unchanged");
 std::cout<<"LIVE_FRAME geometry_uploads="<<builds.geometry_uploads<<" texture_uploads="<<texture_builds.uploads<<" pack_builds="<<texture_builds.pack_builds<<'\n';
 renderer.draw(std::make_shared<MeshSnapshot>(),{},identity,{0,0,2},1,1,false);
 direct.reset();live.reset();
 check(marker.expired(),"Frame/renderer release drops marker while original source paint and material remain alive");
 check(scene.encode()==encoded&&scene.active().painted==paint&&scene.active().material_bindings==binding&&binding->uv_origin==origin,"Marker lifetime changes no source/paint/material/UV-origin bytes or aliases");
 auto again=paint->preview(mat);check(again->source_maps==paint->source_maps(),"Expired weak marker is rebuilt from exact immutable source owner");
 std::cout<<"Source marker lifetime checks "<<checks<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
