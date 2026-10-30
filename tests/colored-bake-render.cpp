// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_material_renderer_suite
#include "mtoon-render.cpp"
#undef main
#include "source-color-consumer.hpp"
int main(int argc,char **argv)try{
  if(argc<2)return 2;
  NativeGraphics graphics;std::string gpu=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GPU "<<gpu<<'\n';
  check(argc<3||std::string(argv[2])!="--hardware"||(gpu.find("llvmpipe")==std::string::npos&&gpu.find("softpipe")==std::string::npos),"Actual baked-normal consumer uses the requested graphics device");
  Fbo framebuffer;framebuffer.bind();SurfaceRenderer native;color_consumer::Renderer consumer;std::filesystem::path prefix=argv[1];
  for(bool repeated:{false,true}){
    auto project=prefix;project+=repeated?"-repeat.dsculpt":".dsculpt";auto original=Scene::load(project);
    auto original_colors=original.active().appearance->asset->primitives[0].colors;
    for(auto alpha:{Material::AlphaMode::Opaque,Material::AlphaMode::Mask,Material::AlphaMode::Blend})for(float weight:{0.f,.35f,1.f}){
      auto scene=original;auto &o=scene.active();o.material.alpha=alpha;o.material.alpha_cutoff=.35f;auto state=*o.deformation;state.morph_weights={weight};state.pose={{{},{},{1.2f,1,.7f}}};scene.set_deformation(o.id,state);
      Document document;document.replace(scene);check(document.wait_mesh()&&document.frame().state->error.empty(),"The actual reopened normal/color bake publishes POSITION morph and anisotropic skin");
      auto snapshot=document.frame().surface;check(snapshot->source_colors.empty()&&!scene.active().painted->baked()->maps.normal.rgba.empty(),"Actual normal payload remains active while resolved PBR avoids double vertex tint");
      auto output=prefix;output+="-consumer-"+std::to_string(repeated)+"-"+std::to_string(int(alpha))+"-"+std::to_string(int(weight*100))+".glb";document.export_asset(output).get();color_consumer::Model parsed(output);
      auto &p=parsed.data->meshes[0].primitives[0];check(p.material->normal_texture.texture&&!color_consumer::attr(p.attributes,p.attributes_count,cgltf_attribute_type_color),"Actual exported model carries the captured normal texture and explicit resolved color interpretation");
      auto expected=draw(native,snapshot);framebuffer.bind();consumer.render(parsed.data);auto actual=read();int error=delta(expected,actual);bool visible=false;for(size_t i=3;i<actual.size();i+=4)visible|=actual[i]>0&&expected[i]>0;
      if(alpha==Material::AlphaMode::Opaque)check(visible,"Both independent and native actual baked-normal consumers draw nonempty geometry");
      if(alpha==Material::AlphaMode::Opaque&&weight==0){auto *normal=p.material->normal_texture.texture;p.material->normal_texture.texture=nullptr;framebuffer.bind();consumer.render(parsed.data);auto without_normal=read();p.material->normal_texture.texture=normal;check(actual!=without_normal,"Independent exported normal-map removal changes actual pixels, proving the real normal payload affects shading");}
      std::cout<<"ACTUAL_BAKE repeated="<<repeated<<" alpha="<<int(alpha)<<" morph="<<weight<<" maxerror="<<error<<'\n';check(error<=4,"Independent cgltf/PNG/GLSL consumer matches actual prepared/baked/UV/restarted normals, color and alpha");
      check(scene.active().appearance->asset->primitives[0].colors==original_colors,"Original authored RGBA values remain immutable through posed export");
    }
  }
  std::cout<<"Actual baked-normal consumer checks "<<checks<<'\n';return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
