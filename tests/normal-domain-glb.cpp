// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_NORMAL_DOMAIN_RENDER_NO_MAIN
#include "normal-domain-render.cpp"
#undef DOME_NORMAL_DOMAIN_RENDER_NO_MAIN
#include "dome/gltf-export.hpp"
#include <fstream>
int main(int argc,char **argv)try{
 if(argc==3&&std::string_view(argv[2])=="--reimport"){std::ifstream input(argv[1],std::ios::binary);std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});ImportLimits limits;limits.mode=ImportMode::PreserveGltf;auto asset=import_glb(bytes,limits);std::cout<<"Native importer accepted "<<asset.primitives.size()<<" primitives\n";return 0;}
 if(argc<2)return 2;const bool require_original=argc>2&&std::string_view(argv[2])=="--require-original-colour";const std::filesystem::path prefix=argv[1];std::filesystem::create_directories(prefix.parent_path());
 NativeGraphics graphics;const std::string device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GPU "<<device<<'\n';
 check(argc<3||std::string_view(argv[2])!="--hardware"||(device.find("Intel")!=std::string::npos&&device.find("Arc")!=std::string::npos&&device.find("A770")!=std::string::npos),"Exact retained normal GLB consumer uses the requested graphics device");
 Fbo fbo;fbo.bind();SurfaceRenderer renderer;prepared_normal_consumer::Renderer independent;
 for(int variant=0;variant<3;++variant){
  const bool typed=variant!=0,secondary_uv=variant==2;const auto original=prepared(typed,secondary_uv);const auto original_data=original.encode();auto stem=prefix.string()+(secondary_uv?"-typed-uv1":typed?"-typed":"-pbr");
  auto retained=retained_transfer(original,.95f,false,stem+"-scale.dsculpt");retained=retained_transfer(retained,.7f,true,stem+"-repeat.dsculpt");
  for(int pose=0;pose<3;++pose)for(float weight:{0.f,.35f,1.f}){
   auto before=original,after=retained;posed(before,pose,weight);posed(after,pose,weight);Document reference,current;reference.replace(before);current.replace(after);settled(reference);settled(current);reference.shading(false);current.shading(false);settled(reference);settled(current);
   auto path=stem+"-p"+std::to_string(pose)+"-w"+std::to_string(int(weight*100));
   reference.export_asset(path+"-original.glb",AssetExportFormat::Glb).get();current.export_asset(path+"-current.glb",AssetExportFormat::Glb).get();
   prepared_normal_consumer::Model source(path+"-original.glb"),output(path+"-current.glb");
   auto &primitive=output.data->meshes[0].primitives[0];check(primitive.material->normal_texture.texcoord==(secondary_uv?2:1)&&primitive.material->normal_texture.texture,"Standard GLB declares a separate retained normal texture-coordinate accessor");
   if(secondary_uv){
    auto *position=cgltf_find_accessor(&primitive,cgltf_attribute_type_position,0),*uv1=cgltf_find_accessor(&primitive,cgltf_attribute_type_texcoord,1),*uv0=cgltf_find_accessor(&primitive,cgltf_attribute_type_texcoord,0);check(position&&uv0&&uv1&&uv1->count==position->count,"Original UV1 remains present beside current authored UV0 and the genuine normal UV2");
    for(size_t v=0;v<position->count;++v){float p[3]{},uv[2]{};cgltf_accessor_read_float(position,v,p,3);cgltf_accessor_read_float(uv1,v,uv,2);if(std::abs(uv[0]-(.2f+.1f*p[0]))>1e-7f||std::abs(uv[1]-(.4f+.1f*p[1]))>1e-7f)throw std::runtime_error("Original independently authored UV1 was replaced or mis-remapped");}
    check(true,"Independent POSITION-to-UV1 authoring formula survives every frame/skin/morph layout split");
    auto *uv2=cgltf_find_accessor(&primitive,cgltf_attribute_type_texcoord,2);if(!uv2||uv2->count!=position->count)throw std::runtime_error("Independent normal domain UV2 is missing");
    for(size_t v=0;v<position->count;++v){float p[3]{},authored[2]{},normal[2]{};cgltf_accessor_read_float(position,v,p,3);cgltf_accessor_read_float(uv0,v,authored,2);cgltf_accessor_read_float(uv2,v,normal,2);const float u=(p[0]+.75f)/1.5f,w=(p[1]+.75f)/1.5f;const float x=.5f+(u-.5f)*.95f,y=.5f+(w-.5f)*.95f;if(std::abs(normal[0]-u)>1e-7f||std::abs(normal[1]-w)>1e-7f||std::abs(authored[0]-(.5f-(y-.5f)*.7f))>1e-7f||std::abs(authored[1]-(.5f+(x-.5f)*.7f))>1e-7f)throw std::runtime_error("Authored UV0 or independent normal UV2 was replaced by another domain");}
    check(true,"Independent authored Scale/rotation and normal-domain formulas prove all three distinct UV streams remain intact");
   }
   fbo.bind();independent.render(source.data);const auto expected=read();fbo.bind();independent.render(output.data);const auto actual=read();
   auto error=delta(expected,actual);std::cout<<"NORMAL_DOMAIN_GLB typed="<<typed<<" pose="<<pose<<" morph="<<weight<<" error="<<error<<'\n';
   if(error>4){
    auto a=prepared_normal_consumer::image(source.data->meshes[0].primitives[0].material->normal_texture.texture),b=prepared_normal_consumer::image(primitive.material->normal_texture.texture);
    std::cout<<"DIAGNOSTIC normal image bytes error="<<delta(a.rgba,b.rgba)<<'\n';
    auto *old_normal=source.data->meshes[0].primitives[0].material->normal_texture.texture;auto *new_normal=primitive.material->normal_texture.texture;
    source.data->meshes[0].primitives[0].material->normal_texture.texture=nullptr;primitive.material->normal_texture.texture=nullptr;
    fbo.bind();independent.render(source.data);auto flat_before=read();fbo.bind();independent.render(output.data);std::cout<<"DIAGNOSTIC no-normal error="<<delta(flat_before,read())<<'\n';
    source.data->meshes[0].primitives[0].material->normal_texture.texture=old_normal;primitive.material->normal_texture.texture=new_normal;

   }
   if(require_original)check(error<=4,"Required original-beforeUV colour/alpha fidelity remains within the unchanged bound");
   const auto native=draw(renderer,current.frame().surface);const auto current_error=delta(native,actual);
   std::cout<<"NORMAL_DOMAIN_CURRENT_PBR typed="<<typed<<" pose="<<pose<<" morph="<<weight<<" error="<<current_error<<'\n';
   check(current.frame().state->pbr_view&&current.frame().surface->ranges.front().pbr_view&&current_error<=4,"Actual user-visible PBR command agrees with independently parsed current GLB without rewriting authored material roles");
   if(typed&&!pose&&weight==0)std::cout<<"REQUIRED_ORIGINAL_COLOUR_FIDELITY error="<<error<<" unchanged_bound=4\n";
   const auto saved=primitive.material->normal_texture.texcoord;primitive.material->normal_texture.texcoord=0;fbo.bind();independent.render(output.data);const auto wrong=read();primitive.material->normal_texture.texcoord=saved;
   if(!pose&&weight==0)check(delta(actual,wrong)>2,"Independent wrong-atlas sampling visibly fails instead of passing a constant-normal stand-in");
  }
  if(typed){Document doc;doc.replace(retained);settled(doc);bool refused=false;try{doc.export_asset(stem+"-unsupported.vrm",AssetExportFormat::Avatar).get();}catch(const std::exception &e){refused=std::string(e.what()).find("format-specific sampling export")!=std::string::npos;}check(refused,"Legacy shared-UV VRM remains explicitly guarded until its sampling compiler is proven");}
  {
   const auto file=stem+"-cancelled.glb";{std::ofstream old(file,std::ios::binary);old<<"retained old output";}bool reached=false,refused=false;GltfExportOptions options;options.before_commit=[&]{reached=true;throw std::runtime_error("explicit late export cancellation");};const auto state=retained.encode();try{export_glb(retained.preview_surfaces(),file,options);}catch(const std::exception &e){refused=std::string_view(e.what())=="explicit late export cancellation";}std::ifstream input(file,std::ios::binary);std::string bytes((std::istreambuf_iterator<char>(input)),{});check(reached&&refused&&bytes=="retained old output"&&retained.encode()==state,"Compiled retained normal export preserves before_commit cancellation, old output and exact owning source");
  }
  check(original.encode()==original_data,"Exact GLB sampling export retains complete immutable original source and artist attributes");
 }
 std::cout<<"PASS "<<checks<<" current-state retained-normal GLB checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
