// SPDX-License-Identifier: GPL-3.0-or-later
#define main original_extra_uv_renderer_suite
#include "mtoon-render.cpp"
#undef main
#include "dome/document.hpp"
#include <nlohmann/json.hpp>
#include "render-upload-target.hpp"
namespace {
std::shared_ptr<const Image> pattern(bool transpose=false){
 Image out{64,64,{}};out.rgba.reserve(64*64*4);
 for(int y=0;y<64;++y)for(int x=0;x<64;++x){int u=transpose?y:x,v=transpose?x:y;out.rgba.insert(out.rgba.end(),{uint8_t(u*4),uint8_t(v*4),uint8_t(252-u*3),uint8_t(u<32?0:255)});}
 return std::make_shared<const Image>(std::move(out));
}
std::shared_ptr<const Image> normal_pattern(bool reference){
 Image result{64,64,{}};result.rgba.reserve(64*64*4);
 for(int y=0;y<64;++y)for(int x=0;x<64;++x){int u=reference?y:x,v=reference?x:y;uint8_t r=uint8_t(128+70*std::sin(u*.14f)),g=uint8_t(128+55*std::cos(v*.11f));if(reference)std::swap(r,g);result.rgba.insert(result.rgba.end(),{r,g,230,255});}return std::make_shared<const Image>(std::move(result));
}
std::shared_ptr<PaintSourceMaps> sources(int role,bool secondary,int coordinate=2){
 auto maps=std::make_shared<PaintSourceMaps>();std::array<PaintSourceTexture*,5> slots{&maps->color,&maps->metal_rough,&maps->emission,&maps->normal,&maps->occlusion};auto &view=*slots.at(role);
 view.image=role==3?normal_pattern(!secondary):pattern(!secondary);
 view.min_filter=view.mag_filter=9728;view.wrap_s=view.wrap_t=33071;view.texcoord=secondary?coordinate:0;
 if(secondary)maps->extra_uv_sets[uint32_t(coordinate)]=std::make_shared<const std::vector<std::array<float,2>>>(std::vector<std::array<float,2>>{{0,0},{0,1},{1,1},{1,0}});
 return maps;
}
std::shared_ptr<MeshSnapshot> model(const std::shared_ptr<const PaintSourceMaps> &maps,Material material){auto value=snapshot(material);value->ranges[0].textures=PaintedMesh::from_maps(value->mesh,maps,64)->preview(material);return value;}
}
namespace {size_t allocated=0,uploaded=0,index_bytes=0;bool capture=false;}
extern "C" void __real_glBufferData(GLenum,GLsizeiptr,const void*,GLenum);
extern "C" void __real_glBufferSubData(GLenum,GLintptr,GLsizeiptr,const void*);
extern "C" void __wrap_glBufferData(GLenum target,GLsizeiptr size,const void *data,GLenum usage){if(capture&&render_upload_test::vertex(target,data,usage))allocated+=size;if(capture&&render_upload_test::index(target,data,usage))index_bytes+=size;__real_glBufferData(target,size,data,usage);}
extern "C" void __wrap_glBufferSubData(GLenum target,GLintptr offset,GLsizeiptr size,const void *data){if(capture&&render_upload_test::subvertex(target))uploaded+=size;__real_glBufferSubData(target,offset,size,data);}
int main(int argc,char **argv)try{
 NativeGraphics graphics;const std::string vendor=reinterpret_cast<const char*>(glGetString(GL_VENDOR)),gpu=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GL_VENDOR "<<vendor<<"\nGL_RENDERER "<<gpu<<'\n';
 check(argc<2||std::string(argv[1])!="--hardware"||(vendor.find("Intel")!=std::string::npos&&gpu.find("Arc")!=std::string::npos&&gpu.find("A770")!=std::string::npos),"Requested Arc renderer is the actual device");Fbo fbo;fbo.bind();SurfaceRenderer renderer;
 for(int coordinate=2;coordinate<=7;++coordinate)for(int role=0;role<5;++role){auto material=white();material.unlit=false;material.color={.65f,.75f,.85f,1};material.metallic=.7f;material.roughness=.6f;material.emissive=role==2?std::array<float,3>{.8f,.7f,.6f}:std::array<float,3>{0,0,0};
  auto actual=model(sources(role,true,coordinate),material),reference=model(sources(role,false),material);auto rendered=draw(renderer,actual);int error=delta(rendered,draw(renderer,reference));std::cout<<"EXTRA_UV set="<<coordinate<<" role="<<role<<" independent_transpose_error="<<error<<'\n';check(error<=2,"Independent transposed image or analytically rotated normal matches the selected additional UV PBR role");
  if(role==0){for(auto alpha:{Material::AlphaMode::Mask,Material::AlphaMode::Blend}){actual->ranges[0].material.alpha=reference->ranges[0].material.alpha=alpha;check(delta(draw(renderer,std::make_shared<MeshSnapshot>(*actual)),draw(renderer,std::make_shared<MeshSnapshot>(*reference)))<=2,"additional UV alpha masking and blending match independent transposed source alpha");}
   actual->ranges[0].material.alpha=Material::AlphaMode::Mask;MaterialSurfaceQuery query(actual->mesh,actual->ranges);check(!query.raycast({0,-.5f,1},{0,0,-1},2)&&query.raycast({0,.5f,1},{0,0,-1},2),"Actual material picker samples additional UV alpha and agrees with visible horizontal transparency");
  }
 }
 auto material=white();auto maps=sources(0,true);auto raw=model(maps,material);Scene scene;scene.active().mesh_active=true;scene.active().material=material;scene.active().painted=PaintedMesh::from_maps(quad(),maps,64);Document document;document.replace(scene);check(document.wait_mesh(),"Native worker publishes source additional UV with the real geometry snapshot");auto packed=document.frame().surface;
 check(*packed->ranges[0].textures->source_maps->extra_uv_sets.at(2)==*maps->extra_uv_sets.at(2)&&delta(draw(renderer,packed),draw(renderer,raw))==0,"Worker-owned packed additional UV produces exact original pixels without relabeling UV0");
 auto invalid=std::make_shared<MeshSnapshot>(*raw);auto payload=std::make_shared<TextureMaps>(*raw->ranges[0].textures);auto bad=std::make_shared<PaintSourceMaps>(*maps);bad->extra_uv_sets[2]=std::make_shared<const std::vector<std::array<float,2>>>(3,std::array<float,2>{0,0});payload->source_maps=bad;invalid->ranges[0].textures=payload;
 check(rejects([&]{draw(renderer,invalid);})&&rejects([&]{MaterialSurfaceQuery query(invalid->mesh,invalid->ranges);}),"GPU and picker reject incomplete source-coordinate authority before reading vertices");
 auto reused=model(maps,material);reused->geometry_identity=std::make_shared<const int>(26);allocated=uploaded=index_bytes=0;capture=true;auto first=draw(renderer,reused);capture=false;
 const auto bytes=(reused->mesh.positions.size()+reused->mesh.normals.size())*sizeof(Vec3)+reused->mesh.uv.size()*sizeof(std::array<float,2>)+maps->extra_uv_sets.at(2)->size()*sizeof(std::array<float,4>);std::cout<<"EXTRA_UV_PAYLOAD_BYTES native="<<bytes<<" allocated="<<allocated<<" uploaded="<<uploaded<<" indices="<<index_bytes<<'\n';
 check(allocated==bytes&&uploaded==allocated&&index_bytes==6*sizeof(uint32_t)&&renderer.buffer_stats().vertex_bytes==allocated,"Actual GL allocation/subdata traffic contains exact native positions/normals/UV0 plus one optional vec4 UV block without duplicated default streams");
 allocated=uploaded=index_bytes=0;capture=true;auto second=draw(renderer,reused);capture=false;check(first==second&&!allocated&&!uploaded&&!index_bytes,"Second-eye/static extra UV rendering uploads no geometry or coordinate buffers");
 check(delta(draw(renderer,raw),draw(renderer,model(maps,material)))==0,"Renderer recovers exact additional UV pixels after rejecting malformed authority");
 std::cout<<"Genuine extra source UV rendering checks "<<checks<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
