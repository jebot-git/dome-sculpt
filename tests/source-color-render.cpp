// SPDX-License-Identifier: GPL-3.0-or-later
#define main original_material_renderer_suite
#include "mtoon-render.cpp"
#undef main
#include "dome/material-visibility.hpp"
#include "dome/source-uv-bake.hpp"
#include "source-color-consumer.hpp"
#include "source-color-fixtures.hpp"
namespace {
bool capture_buffers=false;size_t array_bytes=0,index_bytes=0;
}
extern "C" void __real_glBufferData(GLenum,GLsizeiptr,const void*,GLenum);
extern "C" void __wrap_glBufferData(GLenum target,GLsizeiptr bytes,const void *data,GLenum usage){if(capture_buffers){if(target==GL_ARRAY_BUFFER)array_bytes+=bytes;if(target==GL_ELEMENT_ARRAY_BUFFER)index_bytes+=bytes;}__real_glBufferData(target,bytes,data,usage);}
extern "C" void __real_glBufferSubData(GLenum,GLintptr,GLsizeiptr,const void*);
extern "C" void __wrap_glBufferSubData(GLenum target,GLintptr offset,GLsizeiptr bytes,const void *data){if(capture_buffers){if(target==GL_ARRAY_BUFFER)array_bytes+=bytes;if(target==GL_ELEMENT_ARRAY_BUFFER)index_bytes+=bytes;}__real_glBufferSubData(target,offset,bytes,data);}
namespace {
void uv_adopt(Scene &scene){auto &o=scene.active();SourceUvBakeInput in;in.paint=o.painted;in.appearance=o.appearance;in.current_material=o.material_bindings;in.current_deformation=o.deformation;const auto &mesh=o.painted->mesh();for(auto v:mesh.indices){in.source_vertices.push_back(v);in.target.positions.push_back(mesh.positions[v]);in.target.normals.push_back(mesh.normals[v]);auto uv=mesh.uv[v];in.target.uv.push_back({1-uv[1],uv[0]});in.target.indices.push_back(uint32_t(in.target.positions.size()-1));}SourceUvBakeOptions options;options.resolution=64;auto result=adopt_source_uv_bake(bake_source_uv(in,options));o.painted=result.paint;o.appearance=result.appearance;o.deformation=result.deformation;o.material_bindings=result.material_bindings;}
void paint(Scene &scene){auto &o=scene.active();o.painted=o.painted->snapshot();Brush brush;brush.from=brush.to={0,0,0};brush.normal={0,0,1};brush.radius=.65f;PaintSettings settings;settings.color={.9f,.2f,.4f,.6f};settings.front_only=false;o.painted->begin_stroke();o.painted->stamp(brush,settings);o.painted->end_stroke();}
}
int main(int argc,char **argv)try {
 if(argc<2)return 2;std::filesystem::path prefix=argv[1];NativeGraphics graphics;const std::string gpu=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GPU "<<gpu<<'\n';check(argc<3||std::string(argv[2])!="--hardware"||gpu.find("llvmpipe")==std::string::npos,"Colored native and independent consumers execute on requested hardware");Fbo fbo;fbo.bind();SurfaceRenderer native;color_consumer::Renderer independent;auto original=source_color_fixture::model();const auto original_colors=original.active().appearance->asset->primitives[0].colors;
 for(int stage=0;stage<3;++stage){auto scene=original;if(stage)uv_adopt(scene);if(stage==2)paint(scene);
  for(auto alpha:{Material::AlphaMode::Opaque,Material::AlphaMode::Mask,Material::AlphaMode::Blend})for(float weight:{0.f,.35f,1.f}){
   auto &o=scene.active();o.material.alpha=alpha;o.material.alpha_cutoff=.35f;auto state=*o.deformation;state.morph_weights={weight};state.pose={{{},{},{1.5f,1,.7f}}};scene.set_deformation(o.id,state);Document document;document.replace(scene);check(document.wait_mesh(),"Actual worker packs linear original COLOR_0 through atlas seams, normal-target layout, morph and skin");auto snapshot=document.frame().surface;check(snapshot->source_colors.size()==snapshot->mesh.positions.size(),"Packed color attributes cover the complete current renderer vertex domain");auto path=prefix;path+="-"+std::to_string(stage)+"-"+std::to_string(int(alpha))+"-"+std::to_string(int(weight*100))+".glb";export_glb(scene.preview_surfaces(),path);color_consumer::Model parsed(path);auto wanted=draw(native,snapshot);fbo.bind();independent.render(parsed.data);int error=delta(wanted,read());std::cout<<"COLOR_CONSUMER stage="<<stage<<" alpha="<<int(alpha)<<" morph="<<weight<<" maxerror="<<error<<'\n';check(error<=4,"Independent cgltf/PNG/GLSL consumer matches linear colored PBR, alpha masking or linear-light blending without double modulation");
  }
  auto saved=scene.encode();auto reopened=Scene::decode(nlohmann::json::from_cbor(nlohmann::json::to_cbor(saved)));check(reopened.encode()==saved && reopened.active().appearance->asset->primitives[0].colors==original_colors,"Raw source colors and current atlas/paint/deformation survive native restart unchanged");
 }
 // Versioned MToon ignores COLOR_0 even though its core PBR fallback retains it.
 for(bool legacy:{false,true}){auto m=typed(legacy);m.parameters.base_color={.6f,.4f,.2f,1};m.parameters.shade_color={.1f,.05f,.02f};m.parameters.toony=.5f;m.parameters.shift=-.2f;m.parameters.indirect_intensity=0;auto a=appearance(m);auto colored=snapshot();colored->ranges[0].appearance=a;colored->source_colors.assign(4,{0,0,0,0});auto white=std::make_shared<MeshSnapshot>(*colored);white->source_colors.assign(4,{1,1,1,1});check(draw(native,colored)==draw(native,white),"Pinned legacy MToon0 and normative MToon1 ignore vertex RGB and alpha rather than inheriting PBR modulation");}
 // Picker alpha uses the same interpolated linear source alpha as PBR drawing.
 {auto scene=original;scene.active().material.alpha=Material::AlphaMode::Mask;scene.active().material.alpha_cutoff=.35f;Document document;document.replace(scene);check(document.wait_mesh(),"Colored alpha source publishes before independent visibility rays");auto snapshot=document.frame().surface;MaterialSurfaceQuery query(snapshot->mesh,snapshot->ranges);check(!query.raycast({-.65f,-.65f,1},{0,0,-1},2) && query.raycast({.6f,.6f,1},{0,0,-1},2),"Visibility rays respect zero and interpolated vertex alpha using the same mask threshold as the GPU");auto zero_cutoff=std::make_shared<MeshSnapshot>(*snapshot);zero_cutoff->ranges[0].material.alpha_cutoff=0;MaterialSurfaceQuery zero_query(zero_cutoff->mesh,zero_cutoff->ranges);check(zero_query.raycast({-.75f,-.75f,1},{0,0,-1},2).has_value(),"Mask cutoff zero accepts exactly zero source alpha as the shader does, unlike transparent Blend");}
 {auto ranged=snapshot(Material{});ranged->source_colors.assign(4,{.25f,.5f,.1f,.5f});auto unscoped=std::make_shared<MeshSnapshot>(*ranged);unscoped->ranges.clear();check(draw(native,ranged)==draw(native,unscoped),"Range-free automatic clay rendering respects vertex alpha just like an explicit equivalent material range");}
 // Count actual GL writes, including the optional RGBA block, rather than
 // assuming a diagnostic counter proves immutable geometry reuse.
 {Document document;document.replace(original);check(document.wait_mesh(),"Colored source reaches the worker before GPU traffic measurements");auto base=document.frame().surface;
  auto measured=[&](std::shared_ptr<const MeshSnapshot> value){array_bytes=index_bytes=0;capture_buffers=true;auto pixels=draw(native,std::move(value));capture_buffers=false;return pixels;};
  auto baseline=measured(base);struct BaseVertex{Vec3 p,n;std::array<float,2> uv0,uv1;std::array<float,4> tangent;};struct FrameVertex{Vec3 source,canonical;std::array<float,4> tangent;};
  const auto bytes=base->mesh.positions.size()*(sizeof(BaseVertex)+sizeof(FrameVertex)+sizeof(std::array<float,4>));
  check(native.buffer_stats().vertex_bytes==bytes && array_bytes==bytes*2 && index_bytes==base->mesh.indices.size()*sizeof(uint32_t),"Compiler-sized base, frame and RGBA blocks allocate and upload exactly once through real GL Data/SubData calls");
  check(measured(base)==baseline && array_bytes==0 && index_bytes==0,"Second-eye colored draw uploads no vertex, frame, RGBA or index bytes");
  auto material=std::make_shared<MeshSnapshot>(*base);material->ranges[0].material.emissive={.3f,.1f,0};auto lit=measured(material);check(lit!=baseline && array_bytes==0 && index_bytes==0,"Material-only emission edit changes pixels while retaining the source color stream unchanged");
  auto protected_mesh=std::make_shared<MeshSnapshot>(*base);protected_mesh->sculpt_masks.assign(base->mesh.positions.size(),.6f);check(measured(protected_mesh)!=baseline && array_bytes==base->mesh.positions.size()*sizeof(float) && index_bytes==0,"Mask-only feedback uploads only its scalar stream and never multiplies or uploads source colors twice");
  check(measured(base)==baseline && array_bytes==0 && index_bytes==0,"Removing mask feedback restores exact colored pixels without geometry uploads");
  auto clipped=std::make_shared<MeshSnapshot>(*base);clipped->clipped_object=base->ranges[0].object;clipped->clip_plane={{1,0,0},.1f};check(measured(clipped)!=baseline && array_bytes==0 && index_bytes==0,"Clip-plane uniforms preserve immutable RGBA and direction blocks");
  auto broken=std::make_shared<MeshSnapshot>(*base);broken->geometry_identity.reset();broken->source_colors[0][0]=std::numeric_limits<float>::quiet_NaN();check(rejects([&]{draw(native,broken);}),"Malformed nonfinite linear colors fail before GPU allocation");
  check(measured(base)==baseline,"Renderer recovers exact colored output after rejecting invalid color authority");
 }
 std::cout<<"Source color renderer checks "<<checks<<'\n';return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
