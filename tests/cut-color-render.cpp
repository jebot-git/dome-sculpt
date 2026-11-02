// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_material_renderer_suite
#include "mtoon-render.cpp"
#undef main
#include "cut-color-fixture.hpp"
#include "cut-color.hpp"
#include "source-color-consumer.hpp"
#include "dome/document.hpp"
#include <filesystem>
namespace {
std::shared_ptr<MeshSnapshot> clipped(Document &document,const CutPreview &preview) {
  auto out=std::make_shared<MeshSnapshot>(*document.frame().surface);
  out->geometry_identity=std::make_shared<const int>(1);
  out->clipped_object=document.frame().state->active_object;
  out->clip_plane=preview.plane;
  auto offset=uint32_t(out->mesh.positions.size());
  auto first=out->mesh.indices.size();
  for(auto p:preview.cap_mesh.positions)out->mesh.positions.push_back(p);
  for(auto n:preview.cap_mesh.normals)out->mesh.normals.push_back(n);
  out->mesh.uv.insert(out->mesh.uv.end(),preview.cap_mesh.uv.begin(),preview.cap_mesh.uv.end());
  for(auto i:preview.cap_mesh.indices)out->mesh.indices.push_back(offset+i);
  out->source_colors.resize(out->mesh.positions.size(),{1,1,1,1});
  if(!out->tangents.empty())out->tangents.resize(out->mesh.positions.size(),{1,0,0,1});
  if(!out->source_normals.empty())out->source_normals.insert(out->source_normals.end(),preview.cap_mesh.normals.begin(),preview.cap_mesh.normals.end());
  if(!out->canonical_normals.empty())out->canonical_normals.insert(out->canonical_normals.end(),preview.cap_mesh.normals.begin(),preview.cap_mesh.normals.end());
  if(!out->source_tangents.empty())out->source_tangents.resize(out->mesh.positions.size(),{1,0,0,1});
  out->ranges.push_back({UINT64_MAX,first,preview.cap_mesh.indices.size(),preview.settings.cap_material});
  return out;
}
Image solid(std::array<uint8_t,4> pixel) {
  Image out{64,64,std::vector<uint8_t>(64*64*4)};
  for(size_t i=0;i<out.rgba.size();++i)out.rgba[i]=pixel[i%4];
  return out;
}
void paint(Scene &scene) {
  auto &o=scene.active();o.painted=o.painted->snapshot();
  BakedTextures baked;baked.maps.color=solid({174,210,239,255});
  baked.maps.metal_rough=solid({255,186,16,255});
  baked.maps.emission=solid({0,0,0,255});
  baked.maps.normal=solid({128,128,255,255});baked.coverage.assign(64*64,255);
  o.painted->install_bake(std::move(baked));
  o.painted->add_layer("Actual retained artist color");
  PaintSettings settings;settings.color={.7f,.8f,.65f,1};settings.front_only=false;settings.flow=.4f;
  Brush brush;brush.from=brush.to={0,0,.15f};brush.radius=.7f;
  o.painted->begin_stroke();o.painted->stamp(brush,settings);
  check(o.painted->end_stroke(),"Fixture has actual editable artist paint above baked common color");
}
size_t visible(const Pixels &pixels){size_t count=0;for(size_t i=3;i<pixels.size();i+=4)count+=pixels[i]>0;return count;}
}
int main(int argc,char **argv)try {
  if(argc<2)return 2;
  NativeGraphics graphics;std::string gpu=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GPU "<<gpu<<'\n';
  check(argc<3||std::string_view(argv[2])!="--hardware"||(gpu.find("llvmpipe")==std::string::npos&&gpu.find("softpipe")==std::string::npos),"Cut consumer uses the requested actual graphics device");
  std::filesystem::path prefix=argv[1];std::filesystem::create_directories(prefix.parent_path());
  Fbo framebuffer;framebuffer.bind();SurfaceRenderer native;color_consumer::Renderer independent;
  size_t case_id=0;
  struct Case {bool affine,seam,lit,pose;Material::AlphaMode alpha;float yaw;};std::vector<Case> cases;
  for(bool affine:{false,true})for(bool seam:{false,true})for(bool lit:{false,true})for(auto alpha:{Material::AlphaMode::Opaque,Material::AlphaMode::Mask,Material::AlphaMode::Blend})cases.push_back({affine,seam,lit,false,alpha,0});
  for(bool affine:{false,true})for(bool lit:{false,true}){cases.push_back({affine,false,lit,true,Material::AlphaMode::Opaque,0});cases.push_back({affine,false,lit,false,Material::AlphaMode::Opaque,35});}
  for(const auto &test:cases) {
    auto [affine,seam,lit,posed,alpha,yaw]=test;
    auto original=cut_color_fixture::scene(seam,affine);paint(original);
    original.active().material.unlit=!lit;original.active().material.alpha=alpha;original.active().material.alpha_cutoff=.35f;
    if(posed)cut_color_fixture::pose(original);
    auto original_paint=original.active().painted->encode();auto original_field=original.active().polygon->colors;
    auto source=cut_color_fixture::source(original);CutSettings settings;settings.offset_mm=31;settings.yaw=yaw;
    auto preview=preview_cut(source.mesh,settings);
    Document before;before.replace(original);check(before.wait_mesh(),"Actual current-field source packs through the production worker");
    auto presentation=clipped(before,preview);auto expected=draw(native,presentation);
    auto result=original.snapshot();auto report=detail::publish_colored_cut(result,preview,source,256,{},{});
    Document after;after.replace(result);check(after.wait_mesh(),"Actual cut output packs common paint and current RGBA independently");
    auto output=draw(native,after.frame().surface);int appearance_error=delta(expected,output);
    std::cout<<"CUT case="<<case_id<<" affine="<<affine<<" seam="<<seam<<" lit="<<lit<<" alpha="<<int(alpha)<<" posed="<<posed<<" yaw="<<yaw<<" appearance_error="<<appearance_error<<'\n';
    check(visible(output)>15,"Clipped presentation and cut output draw nonempty real geometry");
    check(appearance_error<=4,"Current corner RGBA, paint and alpha preserve the clipped original's rendered appearance");
    auto project=prefix;project+="-"+std::to_string(case_id)+".dsculpt";result.save(project);auto reopened=Scene::load(project);
    check(reopened.encode()==result.encode(),"Actual colored-cut project restarts with exact complete source authority");
    auto file=prefix;file+="-"+std::to_string(case_id)+".glb";export_glb(reopened.preview_surfaces(),file);
    color_consumer::Model parsed(file);framebuffer.bind();independent.render(parsed.data,nullptr,{},true);auto consumed=read();int consumer_error=delta(output,consumed);
    std::cout<<"GLB case="<<case_id<<" maxerror="<<consumer_error<<'\n';
    check(consumer_error<=4,"Independent cgltf/PNG/GLSL rendering preserves exported cut RGBA, paint, alpha and normal basis");
    auto restored=reopened.snapshot();restore_polygon_origin(restored,report.shell);
    check(restored.active().painted->encode()==original_paint&&std::equal(restored.active().polygon->colors->sources().begin(),restored.active().polygon->colors->sources().end(),original_field->sources().begin(),original_field->sources().end(),[](const auto &a,const auto &b){return a.corner==b.corner&&a.value==b.value;}),"Source restoration recovers original layers, UVs and exact original corner RGBA");
    ++case_id;
  }
  std::cout<<checks<<" cut render/export checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
