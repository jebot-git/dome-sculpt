// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_NORMAL_UV_RENDER_NO_MAIN
#include "normal-uv-render.cpp"
#undef DOME_NORMAL_UV_RENDER_NO_MAIN
#include "dome/normal-sampling.hpp"
#include "dome/source-uv-bake.hpp"
#include <numeric>
namespace {
Scene retained_transfer(const Scene &source,float factor,bool rotate,const std::filesystem::path &path){
  const auto &object=source.active();SourceUvBakeInput input;
  input.paint=object.painted;input.current_material=object.material_bindings;input.current_deformation=object.deformation;input.appearance=object.appearance;
  input.target=object.painted->mesh();input.source_vertices.resize(input.target.positions.size());std::iota(input.source_vertices.begin(),input.source_vertices.end(),0);
  for(auto &uv:input.target.uv){auto x=uv[0]-.5f,y=uv[1]-.5f;uv={.5f+(rotate?-y:x)*factor,.5f+(rotate?x:y)*factor};}
  SourceUvBakeOptions options;options.normal_sampling=SourceNormalUvMethod::RetainSource;
  auto adopted=adopt_source_uv_bake(bake_source_uv(input,options));auto result=source.snapshot();auto &target=result.active();
  target.painted=adopted.paint;target.material_bindings=adopted.material_bindings;target.deformation=adopted.deformation;target.appearance=adopted.appearance;
  result.validate();result.save(path);auto restarted=Scene::load(path);
  check(restarted.encode()==result.encode(),"Exact retained normal Scale/rotation restarts with its complete owning graph");
  return restarted;
}
void check_images(const Scene &original,const Scene &changed,bool typed,const std::filesystem::path &prefix,SurfaceRenderer &renderer,Fbo &fbo,prepared_normal_consumer::Renderer &independent){
  for(int pose=0;pose<3;++pose)for(float weight:{0.f,.35f,1.f}){
    auto before=original,after=changed;posed(before,pose,weight);posed(after,pose,weight);
    Document source;source.replace(before);settled(source);
    auto path=prefix.string()+"-p"+std::to_string(pose)+"-w"+std::to_string(int(weight*100))+(typed?".vrm":".glb");
    source.export_asset(path,typed?AssetExportFormat::Avatar:AssetExportFormat::Glb).get();
    prepared_normal_consumer::Model file(path);Document current;current.replace(after);settled(current);
    const auto surface=current.frame().surface;
    check(!surface->normal_coordinates.empty(),"Actual Document worker publishes immutable retained normal coordinates");
    const auto native=draw(renderer,surface);const auto buffers=renderer.buffer_stats();const auto textures=renderer.texture_stats();
    check(native==draw(renderer,surface)&&renderer.buffer_stats().geometry_uploads==buffers.geometry_uploads&&renderer.texture_stats().uploads==textures.uploads,
          "Repeated/stereo retained-domain frames reuse geometry and immutable normal image uploads");
    fbo.bind();independent.render(file.data,typed?&file.json.at("extensions").at("VRM"):nullptr);const auto reference=read();
    const auto error=delta(native,reference);size_t covered=0;for(size_t at=3;at<reference.size();at+=4)covered+=reference[at]>0;
    std::cout<<"NORMAL_DOMAIN typed="<<typed<<" pose="<<pose<<" morph="<<weight<<" error="<<error<<" covered="<<covered<<'\n';
    check(covered>1000&&error<=4,"Retained normal sampling matches independent original-domain file pixels through mixed skin and morphs");
    auto invalid=std::make_shared<MeshSnapshot>(*surface);invalid->geometry_identity.reset();invalid->normal_coordinates[invalid->mesh.indices.front()][0]+=.01f;
    check(rejects([&]{draw(renderer,invalid);}),"GPU publication rejects an unauthenticated changed normal correspondence");
    current.shading(false);settled(current);const auto compiled_path=prefix.string()+"-compiled-p"+std::to_string(pose)+"-w"+std::to_string(int(weight*100))+".glb";
    current.export_asset(compiled_path,AssetExportFormat::Glb).get();prepared_normal_consumer::Model compiled(compiled_path);
    const auto core=draw(renderer,current.frame().surface);fbo.bind();independent.render(compiled.data);
    check(delta(core,read())<=4,"Genuine compiled core-PBR normal coordinates match the real user-visible PBR mode");
  }
}
}
#ifndef DOME_NORMAL_DOMAIN_RENDER_NO_MAIN
int main(int argc,char **argv)try{
  if(argc<2)return 2;const std::filesystem::path prefix=argv[1];std::filesystem::create_directories(prefix.parent_path());
  NativeGraphics graphics;const std::string device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GPU "<<device<<'\n';
  check(argc<3||std::string_view(argv[2])!="--hardware"||device.find("A770")!=std::string::npos,"Retained normal fixture uses the requested actual graphics device");
  Fbo fbo;fbo.bind();SurfaceRenderer renderer;prepared_normal_consumer::Renderer independent;
  for(bool typed:{false,true}){auto original=prepared(typed);const auto exact=original.encode();auto stem=prefix.string()+(typed?"-typed":"-pbr");
    auto current=retained_transfer(original,.95f,false,stem+"-scale.dsculpt");
    current=retained_transfer(current,.7f,true,stem+"-repeat.dsculpt");
    check_images(original,current,typed,stem,renderer,fbo,independent);
    check(original.encode()==exact,"Native domain sampling preserves exact original artist/source and normal-detail authority");
  }
  std::cout<<"PASS "<<checks<<" retained normal graphical checks\n";return 0;
}catch(const std::exception &error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
#endif
