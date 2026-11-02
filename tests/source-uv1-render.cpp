// SPDX-License-Identifier: GPL-3.0-or-later
#define main original_mtoon_renderer_suite
#include "mtoon-render.cpp"
#undef main
#include "dome/document.hpp"
#include <nlohmann/json.hpp>
namespace {
std::shared_ptr<const Image> pattern(bool transpose=false){
 Image out{64,64,{}};out.rgba.reserve(64*64*4);
 for(int y=0;y<64;++y)for(int x=0;x<64;++x){int u=transpose?y:x,v=transpose?x:y;out.rgba.insert(out.rgba.end(),{uint8_t(u*4),uint8_t(v*4),uint8_t(252-u*3),uint8_t(u<32?0:255)});}
 return std::make_shared<const Image>(std::move(out));
}
std::shared_ptr<PaintSourceMaps> sources(int role,bool secondary){
 auto maps=std::make_shared<PaintSourceMaps>();std::array<PaintSourceTexture*,5> slots{&maps->color,&maps->metal_rough,&maps->emission,&maps->normal,&maps->occlusion};auto &view=*slots.at(role);
 view.image=role==3?image(secondary?std::array<uint8_t,4>{255,128,128,255}:std::array<uint8_t,4>{128,255,128,255}):pattern(!secondary);
 view.min_filter=view.mag_filter=9728;view.wrap_s=view.wrap_t=33071;view.texcoord=secondary?1:0;
 if(secondary)maps->uv1=std::make_shared<const std::vector<std::array<float,2>>>(std::vector<std::array<float,2>>{{0,0},{0,1},{1,1},{1,0}});
 return maps;
}
std::shared_ptr<MeshSnapshot> model(const std::shared_ptr<const PaintSourceMaps> &maps,Material material){auto value=snapshot(material);value->ranges[0].textures=PaintedMesh::from_maps(value->mesh,maps,64)->preview(material);return value;}
}
int main(int argc,char **argv)try{
 NativeGraphics graphics;const std::string vendor=reinterpret_cast<const char*>(glGetString(GL_VENDOR)),gpu=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GL_VENDOR "<<vendor<<"\nGL_RENDERER "<<gpu<<'\n';
 check(argc<2||std::string(argv[1])!="--hardware"||(vendor.find("Intel")!=std::string::npos&&gpu.find("A770")!=std::string::npos),"Requested Arc renderer is the actual device");Fbo fbo;fbo.bind();SurfaceRenderer renderer;
 for(int role=0;role<5;++role){auto material=white();material.unlit=false;material.color={.65f,.75f,.85f,1};material.metallic=.7f;material.roughness=.6f;material.emissive=role==2?std::array<float,3>{.8f,.7f,.6f}:std::array<float,3>{0,0,0};
  auto actual=model(sources(role,true),material),reference=model(sources(role,false),material);auto rendered=draw(renderer,actual);int error=delta(rendered,draw(renderer,reference));std::cout<<"UV1 role="<<role<<" independent_transpose_error="<<error<<'\n';check(error<=2,"Independent transposed image or analytically rotated normal matches the selected UV1 PBR role");
  if(role==0){for(auto alpha:{Material::AlphaMode::Mask,Material::AlphaMode::Blend}){actual->ranges[0].material.alpha=reference->ranges[0].material.alpha=alpha;check(delta(draw(renderer,std::make_shared<MeshSnapshot>(*actual)),draw(renderer,std::make_shared<MeshSnapshot>(*reference)))<=2,"UV1 alpha masking and blending match independent transposed source alpha");}
   actual->ranges[0].material.alpha=Material::AlphaMode::Mask;MaterialSurfaceQuery query(actual->mesh,actual->ranges);check(!query.raycast({0,-.5f,1},{0,0,-1},2)&&query.raycast({0,.5f,1},{0,0,-1},2),"Actual material picker samples UV1 alpha and agrees with visible horizontal transparency");
  }
 }
 auto material=white();auto maps=sources(0,true);auto raw=model(maps,material);Scene scene;scene.active().mesh_active=true;scene.active().material=material;scene.active().painted=PaintedMesh::from_maps(quad(),maps,64);Document document;document.replace(scene);check(document.wait_mesh(),"Native worker publishes source UV1 with the real geometry snapshot");auto packed=document.frame().surface;
 check(*packed->ranges[0].textures->source_maps->uv1==*maps->uv1&&delta(draw(renderer,packed),draw(renderer,raw))==0,"Worker-owned packed UV1 produces exact original pixels without relabeling UV0");
 auto invalid=std::make_shared<MeshSnapshot>(*raw);auto payload=std::make_shared<TextureMaps>(*raw->ranges[0].textures);auto bad=std::make_shared<PaintSourceMaps>(*maps);bad->uv1=std::make_shared<const std::vector<std::array<float,2>>>(3,std::array<float,2>{0,0});payload->source_maps=bad;invalid->ranges[0].textures=payload;
 check(rejects([&]{draw(renderer,invalid);})&&rejects([&]{MaterialSurfaceQuery query(invalid->mesh,invalid->ranges);}),"GPU and picker reject incomplete source-coordinate authority before reading vertices");
 check(delta(draw(renderer,raw),draw(renderer,model(maps,material)))==0,"Renderer recovers exact UV1 pixels after rejecting malformed authority");
 std::cout<<"Source UV1 rendering checks "<<checks<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
