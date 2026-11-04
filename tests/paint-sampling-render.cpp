// SPDX-License-Identifier: GPL-3.0-or-later
#include "bake-recipe-capabilities.hpp"
#define DOME_NORMAL_COLOR_RENDER_NO_MAIN
#include "normal-color-uv-render.cpp"
#undef DOME_NORMAL_COLOR_RENDER_NO_MAIN
#include "dome/color-paint-sampling.hpp"
#include "paint-sampling-access.hpp"
#include "normal-sampling-access.hpp"
#include "normal-frame-runtime.hpp"
#include "dome/paint-sampling.hpp"
#include "dome/material-visibility.hpp"
#include "affine-surface.hpp"
namespace {
Scene retain_writable(const Scene &scene) {
 const auto &object=scene.active();UvWorkspace uv(object.painted->mesh(),256);const auto &m=object.painted->mesh();size_t face=0;while(face<m.indices.size()/3&&m.normals[m.indices[face*3]].z<.9f)++face;
 std::array<float,2> p{};for(size_t k=0;k<3;++k){auto q=m.uv[m.indices[face*3+k]];for(size_t d=0;d<2;++d)p[d]+=q[d]/3;}
 uv.edit(UvAction::Pick,p[0],p[1]);uv.edit(UvAction::Scale,.7f);uv.edit(UvAction::Rotate,1.57079632679f);auto view=uv.view();if(!view.valid)throw std::runtime_error("Retained writable UV draft invalid");
 auto derivative=prepare_polygon_uv_derivative(object,*view.mesh,*view.source_vertices,{});
 auto descriptor=std::make_shared<const PolygonColorSurface>(PolygonColorSurface{std::make_shared<const PolygonMesh>(derivative.authority->topology),derivative.authority->colors});
 auto result=retain_source_corner_paint(object.painted,object.material_bindings,object.deformation,descriptor,derivative.mesh,derivative.source_vertices);
 SourceUvBakeInput input;input.paint=object.painted;input.current_material=object.material_bindings;input.current_deformation=object.deformation;input.target=derivative.mesh;input.source_vertices=derivative.source_vertices;
 SourceUvBakeResult normal_result;SourceUvBakeOptions options;options.normal_sampling=SourceNormalUvMethod::RetainSource;
 auto normal=detail::retain_normal_sampling(input,normal_result,options,result.paint->paint_sampling_domain());
 auto joint=adopt_retained_paint_normal(*result.paint,*normal);auto binding=std::make_shared<SourceMaterialBindings>(*object.material_bindings);binding->color_preparation=result.colors;
 auto preparation=std::make_shared<SourceNormalPreparation>(*binding->normal_preparation);preparation->projection=normal->projection;preparation->sampling_domain=normal;preparation->uv_reprojection.reset();binding->normal_preparation=std::move(preparation);
 auto output=scene.snapshot();auto &out=output.active();out.painted=joint;out.material_bindings=binding;out.polygon=derivative.authority;out.deformation=detail::normal_frame_remap_deformation(object.deformation,object.painted->mesh(),derivative.mesh,derivative.source_vertices,{});output.validate();return output;
}
}
#ifndef DOME_PAINT_SAMPLING_RENDER_NO_MAIN
int main(int argc,char **argv)try {
 if(argc<2)return 2;const std::filesystem::path prefix=argv[1];std::filesystem::create_directories(prefix);
 NativeGraphics graphics;const std::string device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GPU "<<device<<std::endl;
 check(argc<3||std::string_view(argv[2])!="--hardware"||(device.find("Intel")!=std::string::npos&&device.find("Arc")!=std::string::npos&&device.find("A770")!=std::string::npos),"Writable domain fixture uses actual requested graphics device");
 Fbo fbo;fbo.bind();SurfaceRenderer renderer;prepared_normal_consumer::Renderer independent;size_t number=0;
 for(bool seam:{false,true})for(auto alpha:{Material::AlphaMode::Opaque,Material::AlphaMode::Mask,Material::AlphaMode::Blend}) {
  auto original=joint::prepare(seam,alpha);auto current=retain_writable(original);auto file=prefix/(std::to_string(number)+".dsculpt");current.save(file);current=Scene::load(file);
  check(bake_recipe_fixture::scene30(current,24),"Actual fractional scale/turn restarts its owning Scene24 normal/colour/paint closure");
  for(int pose=0;pose<3;++pose)for(float weight:{0.f,.35f,1.f}) {
   auto before=original,after=current;posed(before,pose,weight);posed(after,pose,weight);Document b,a;b.replace(before);a.replace(after);settled(b);settled(a);
   auto surface=a.frame().surface;check(!surface->paint_coordinates.empty()&&surface->paint_coordinates.size()==surface->mesh.positions.size(),"Worker publishes explicit retained paint coordinates beside independent normal coordinates");
   auto pixels=draw(renderer,b.frame().surface);auto result=draw(renderer,surface);size_t covered=0;std::set<std::array<uint8_t,4>> varied;for(size_t p=0;p<pixels.size();p+=4)if(pixels[p+3]){++covered;varied.insert({pixels[p],pixels[p+1],pixels[p+2],pixels[p+3]});}
   check(covered>100&&varied.size()>16,"Quality oracle covers varying partial-bake colour/alpha and independent normal detail");auto error=delta(pixels,result);std::cout<<"PAINT_DOMAIN seam="<<seam<<" alpha="<<int(alpha)<<" pose="<<pose<<" morph="<<weight<<" max="<<error<<std::endl;
   if(error>2){joint::capture(prefix/"failure-before.png",pixels);joint::capture(prefix/"failure-current.png",result);before.save(prefix/"failure-before.dsculpt");after.save(prefix/"failure-current.dsculpt");}
   check(error<=2,"Same writable grid preserves actual before/after partial Bake through fractional UV, pose, morph and alpha");
   const auto stats=renderer.buffer_stats();check(result==draw(renderer,surface)&&renderer.buffer_stats().geometry_uploads==stats.geometry_uploads,"Second eye reuses the authenticated immutable paint stream without geometry upload");
   if(pose==0&&weight==0) {
    joint::capture(prefix/(std::to_string(number)+"-before.png"),pixels);joint::capture(prefix/(std::to_string(number)+"-current.png"),result);
    auto bad=std::make_shared<MeshSnapshot>(*surface);bad->geometry_identity.reset();bad->paint_coordinates[bad->mesh.indices.front()][0]+=.01f;check(rejects([&]{draw(renderer,bad);}),"Renderer refuses changed paint corner sampling rather than borrowing normal authority");
    bad=std::make_shared<MeshSnapshot>(*surface);bad->geometry_identity.reset();bad->paint_coordinates.clear();check(rejects([&]{draw(renderer,bad);}),"Renderer refuses omitted paint stream");
    bad=std::make_shared<MeshSnapshot>(*surface);bad->geometry_identity.reset();for(auto &r:bad->ranges)r.paint_sampling_attachment.reset();check(rejects([&]{draw(renderer,bad);}),"Renderer refuses an omitted owning attachment even when coordinate bytes remain");
    auto detached=Scene::decode(after.encode());auto borrowed=detached.active().painted->paint_sampling_attachment();bad=std::make_shared<MeshSnapshot>(*surface);bad->geometry_identity.reset();for(auto &r:bad->ranges)if(r.paint_sampling_attachment)r.paint_sampling_attachment=borrowed;check(rejects([&]{draw(renderer,bad);}),"An equal-valued separately owned domain cannot borrow this range's actual material proof");
   }
   auto path=prefix/(std::to_string(number)+"-"+std::to_string(pose)+"-"+std::to_string(int(weight*100))+".glb");b.export_asset(path).get();prepared_normal_consumer::Model model(path);fbo.bind();independent.render(model.data,nullptr);auto reference=read();auto external=delta(result,reference);std::cout<<"PAINT_ORIGINAL_GLB max="<<external<<std::endl;check(external<=3,"Independent original-basis GLB consumer agrees with current writable-domain pixels");
   auto output=prefix/(std::to_string(number)+"-"+std::to_string(pose)+"-"+std::to_string(int(weight*100))+"-current.glb");a.export_asset(output).get();prepared_normal_consumer::Model emitted(output);fbo.bind();independent.render(emitted.data,nullptr);auto compiled=read();auto compiled_error=delta(result,compiled);std::cout<<"PAINT_CURRENT_GLB max="<<compiled_error<<std::endl;check(compiled_error<=3,"Independent newly emitted current-project GLB agrees with native colour/normal/paint-domain pixels");
   auto primitives=emitted.data->meshes[0].primitives;const auto &primitive=primitives[0];const auto uv0=prepared_normal_consumer::attr(primitive.attributes,primitive.attributes_count,cgltf_attribute_type_texcoord,0);
   const auto &rest=after.active().painted->mesh();bool uv_exact=primitive.indices->count==rest.indices.size();
   for(size_t c=0;uv_exact&&c<rest.indices.size();++c){float coordinate[2]{};uv_exact=cgltf_accessor_read_float(uv0,cgltf_accessor_read_index(primitive.indices,c),coordinate,2)&&std::array<float,2>{coordinate[0],coordinate[1]}==rest.uv[rest.indices[c]];}
   check(uv_exact&&primitive.material->pbr_metallic_roughness.base_color_texture.texcoord>0,"New GLB preserves actual authored UV0 corner order and genuinely binds the separate writable colour coordinate accessor");

  }
  ++number;
 }
 auto original=joint::prepare(true,Material::AlphaMode::Mask);auto changed=retain_writable(original);
 auto matrix=AffineTransform{}.matrix();matrix[0]=-1;matrix[4]=.15;matrix[8]=.08;auto edit=std::make_shared<const NativeAffineEdit>(AffineTransform(matrix));original.active().affine_edit=edit;changed.active().affine_edit=edit;posed(original,1,.35f);posed(changed,1,.35f);
 Document before_reflected,after_reflected;before_reflected.replace(original);after_reflected.replace(changed);settled(before_reflected);settled(after_reflected);auto snapshot=after_reflected.frame().surface;
 auto range=std::find_if(snapshot->ranges.begin(),snapshot->ranges.end(),[&](const auto &r){return r.object==changed.active().id;});check(range!=snapshot->ranges.end()&&range->paint_winding_reversed,"Packed reflected range records its actual EBO flip independently of source/cull orientation");
 auto initial=draw(renderer,before_reflected.frame().surface),reflected=draw(renderer,snapshot);check(delta(initial,reflected)<=2,"Exact corner paint/normal correspondence survives actual reflected sheared world placement");
 auto bad=std::make_shared<MeshSnapshot>(*snapshot);bad->geometry_identity.reset();for(auto &r:bad->ranges)if(r.paint_sampling_attachment)r.paint_winding_reversed=false;check(rejects([&]{draw(renderer,bad);}),"Missing explicit EBO winding authority cannot silently reinterpret logical paint corners");
 auto surfaces=changed.preview_surfaces();auto object=std::find_if(surfaces.begin(),surfaces.end(),[&](const auto &o){return o.id==changed.active().id;});auto raw=object->preview_mesh();auto placement=detail::placement_frame(*object);detail::place_mesh(raw,placement,{});
 SurfaceRange raw_range{object->id,0,raw.indices.size(),object->material,object->textures,object->appearance,0,object->material_bindings};raw_range.reflected=true;raw_range.paint_sampling_attachment=object->paint_sampling_attachment;
 auto colors=detail::current_vertex_colors(*object,{});MaterialSurfaceQuery raw_query(raw,std::span<const SurfaceRange>(&raw_range,1),{},colors,PaintCornerCoordinates{object->paint_corner_coordinates});
 MaterialSurfaceQuery packed_query(snapshot->mesh,snapshot->ranges,{},snapshot->source_colors,PaintVertexCoordinates{snapshot->paint_coordinates});
 size_t visible=0,misses=0;bool same=true;for(int y=0;y<24;++y)for(int x=0;x<24;++x){Vec3 origin{(x+.5f)/12-1,(y+.5f)/12-1,2};auto a=raw_query.raycast(origin,{0,0,-1},100,0,object->id),b=packed_query.raycast(origin,{0,0,-1},100,0,object->id);same&=bool(a)==bool(b);if(a&&b){++visible;same&=length(a->position-b->position)<1e-5f;}else ++misses;}
 check(same&&visible>0&&misses>0,"Raw logical-corner and packed-vertex material alpha picking agree under posed reflected/sheared geometry");
 check(rejects([&]{MaterialSurfaceQuery omitted(raw,std::span<const SurfaceRange>(&raw_range,1),{},colors);}),"Legacy picker overload refuses an omitted paint domain rather than assuming authored UV0");
 std::cout<<"PASS "<<checks<<" retained writable-domain graphical checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}

#endif
