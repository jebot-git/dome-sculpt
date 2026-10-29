// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/render.hpp"
#include "dome/source-maps.hpp"
#include "dome/bake.hpp"
#include "dome/humanoid-creator.hpp"
#include "graphics.h"
#include <chrono>
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
namespace {
void check(bool value,const char*text){if(!value)throw std::runtime_error(text);std::cout<<"PASS "<<text<<'\n';}
template<class F>bool rejects(F f){try{f();return false;}catch(const std::exception&){return true;}}
Mesh quad(){Mesh m;m.positions={{-1,-1,0},{1,-1,0},{1,1,0},{-1,1,0}};m.normals.assign(4,{0,0,1});m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.indices={0,1,2,0,2,3};return m;}
std::shared_ptr<const Image> flat(std::array<uint8_t,4> c){return std::make_shared<const Image>(Image{1,1,{c[0],c[1],c[2],c[3]}});}
std::shared_ptr<MeshSnapshot> snapshot(Mesh m,Material material,std::shared_ptr<const TextureMaps>maps){auto result=std::make_shared<MeshSnapshot>();result->mesh=std::move(m);result->ranges.push_back({1,0,result->mesh.indices.size(),material,std::move(maps),{}});return result;}
std::shared_ptr<PaintedMesh> painted(std::shared_ptr<const PaintSourceMaps> maps){return PaintedMesh::from_maps(quad(),std::move(maps),64);}
Material white(){Material m;m.color={1,1,1,1};m.unlit=true;m.alpha=Material::AlphaMode::Opaque;return m;}
struct Framebuffer {
  static constexpr int size=128;GLuint fbo=0,color=0,depth=0;
  Framebuffer(){glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glGenTextures(1,&color);glBindTexture(GL_TEXTURE_2D,color);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,size,size,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,size,size);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("GPU fixture framebuffer incomplete");}
  ~Framebuffer(){glDeleteRenderbuffers(1,&depth);glDeleteTextures(1,&color);glDeleteFramebuffers(1,&fbo);}
};
const Mat4 identity{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
std::vector<uint8_t> capture(SurfaceRenderer&r,std::shared_ptr<const MeshSnapshot>s){r.draw(std::move(s),{},identity,{0,0,2},Framebuffer::size,Framebuffer::size,true);std::vector<uint8_t>pixels(Framebuffer::size*Framebuffer::size*4);glReadPixels(0,0,Framebuffer::size,Framebuffer::size,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("GPU fixture readback failed");return pixels;}
std::array<uint8_t,4> pixel(const std::vector<uint8_t>&p,int x=64,int y=64){size_t at=(y*Framebuffer::size+x)*4;return {p[at],p[at+1],p[at+2],p[at+3]};}
int difference(const std::vector<uint8_t>&a,const std::vector<uint8_t>&b){int d=0;for(size_t n=0;n<a.size();++n)d=std::max(d,std::abs(int(a[n])-int(b[n])));return d;}
}
int main(int argc,char**argv)try{
  bool hardware=argc>1&&std::string(argv[1])=="--hardware";int first=hardware?2:1;
  NativeGraphics graphics;std::string device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GPU "<<device<<'\n';
  check(!hardware||(device.find("llvmpipe")==std::string::npos&&device.find("softpipe")==std::string::npos),hardware?"Hardware verification uses an actual graphics device rather than software rendering":"Software-permitted verification uses the reported graphics renderer");
  Framebuffer fbo;SurfaceRenderer renderer;auto maps=std::make_shared<PaintSourceMaps>();maps->color.image=std::make_shared<const Image>(Image{2,1,{255,0,0,255,0,0,255,128}});maps->color.mag_filter=maps->color.min_filter=GL_NEAREST;
  auto p=painted(maps);auto material=white();auto before=paint_composite_cache_stats();auto marker=p->preview(material);
  check(marker->source_only&&marker->color.rgba.empty()&&marker->source_maps->color.image==maps->color.image&&marker->transparent,"Unpainted preview carries exact original image ownership and alpha without allocating derived pixels");
  check(paint_composite_cache_stats().builds==before.builds&&p->snapshot()->preview(material)==marker,"Unchanged preview and detached geometry snapshots perform zero CPU composite builds");
  PaintSettings settings;settings.channel=PaintChannel::Mask;settings.mask=.5f;Brush b;b.from=b.to={0,0,0};b.normal={0,0,1};b.radius=.4f;p->begin_stroke();p->stamp(b,settings);p->end_stroke();
  check(p->has_paint_data()&&p->preview(material)->source_only,"Mask-only retained data with no channel coverage remains eligible for exact raw-source preview");
  settings.channel=PaintChannel::Color;settings.color={0,1,0,1};p->begin_stroke();p->stamp(b,settings);p->end_stroke();check(!p->preview(material)->source_only,"Visible masked paint coverage keeps the fully composited overlay path");
  p->layer_properties("Hidden",false,1);check(p->preview(material)->source_only,"Hidden paint contributes no pixels and safely uses original source bindings");p->layer_properties("Zero",true,0);check(p->preview(material)->source_only,"Zero-opacity paint keeps its data while using original source bindings");p->layer_properties("Paint",true,1);
  auto encoded=p->encode();for(auto &layer:encoded["layers"])for(auto &tile:layer["tiles"]){auto &bytes=tile["data"].get_binary();for(size_t i=13;i<bytes.size();i+=14)bytes[i]=0;}
  check(PaintedMesh::decode(encoded)->preview(material)->source_only,"Fully masked retained color data is not accidentally displayed as an overlay");
  auto opaque=white();auto displayed=capture(renderer,snapshot(quad(),opaque,marker));check(pixel(displayed,24)[0]==255&&pixel(displayed,104)[2]==255,"Raw GPU bindings sample original nearest texture colors with no CPU proxy image");
  auto blend=opaque;blend.alpha=Material::AlphaMode::Blend;auto alpha=capture(renderer,snapshot(quad(),blend,marker));check(pixel(alpha,104)[3]==128,"Original texture alpha stays linear and is applied once in the GPU blend pass");
  auto cut=opaque;cut.alpha=Material::AlphaMode::Mask;cut.alpha_cutoff=.7f;auto clipped=capture(renderer,snapshot(quad(),cut,marker));check(pixel(clipped,104)[3]==0&&pixel(clipped,24)[3]==255,"Raw source alpha cutoff preserves opaque coverage and discards authored holes");
  auto factors=std::make_shared<PaintSourceMaps>();auto image=std::make_shared<Image>();image->width=image->height=64;
  for(int y=0;y<64;++y)for(int x=0;x<64;++x){image->rgba.push_back(uint8_t(x*4));image->rgba.push_back(uint8_t(y*4));image->rgba.push_back(128);image->rgba.push_back(192);}
  factors->color.image=image;factors->color.wrap_s=factors->color.wrap_t=GL_CLAMP_TO_EDGE;factors->emission.image=flat({32,64,128,255});factors->metal_rough.image=flat({99,128,64,255});factors->occlusion.image=flat({128,0,0,255});factors->occlusion_strength=.5f;factors->normal.image=flat({128,128,255,255});
  auto test=painted(factors);auto factored=white();factored.color={.5f,.7f,.4f,.75f};factored.emissive={.2f,.1f,.05f};factored.alpha=Material::AlphaMode::Blend;
  auto raw=capture(renderer,snapshot(quad(),factored,test->preview(factored))),composite=capture(renderer,snapshot(quad(),factored,test->composite(factored)));
  check(difference(raw,composite)<=3,"Independent GPU raw-source and CPU-composite paths agree on sRGB filtering, alpha and material factors within8bit quantization");
  factored.unlit=false;factored.alpha=Material::AlphaMode::Opaque;raw=capture(renderer,snapshot(quad(),factored,test->preview(factored)));composite=capture(renderer,snapshot(quad(),factored,test->composite(factored)));
  check(difference(raw,composite)<=4,"Raw PBR normal/AO/scalar bindings agree with composited ORM and original normal views");
  auto ao=std::make_shared<PaintSourceMaps>();ao->occlusion.image=flat({0,0,0,255});auto dark=painted(ao);auto ambient_mesh=quad();ambient_mesh.normals.assign(4,{0,-1,0});auto lit=white();lit.unlit=false;
  auto occluded=pixel(capture(renderer,snapshot(ambient_mesh,lit,dark->preview(lit))));ao->occlusion.image=flat({255,255,255,255});auto open=painted(ao);auto ambient=pixel(capture(renderer,snapshot(ambient_mesh,lit,open->preview(lit))));
  check(ambient[0]>occluded[0]+40,"Independent zero-direct-light fixture proves original AO controls PBR ambient illumination");
  auto normals=std::make_shared<PaintSourceMaps>();normals->normal.image=flat({128,128,255,255});auto flat_normal=painted(normals);auto flat_pixels=capture(renderer,snapshot(quad(),lit,flat_normal->preview(lit)));normals->normal.image=flat({255,128,128,255});auto tilted=painted(normals);auto tilt_pixels=capture(renderer,snapshot(quad(),lit,tilted->preview(lit)));
  check(difference(flat_pixels,tilt_pixels)>10,"Independent original normal-map fixture changes PBR lighting through the UV tangent basis");normals->normal_scale=0;auto disabled=painted(normals);
  check(difference(flat_pixels,capture(renderer,snapshot(quad(),lit,disabled->preview(lit))))<=2,"Original normal strength zero restores geometric shading without changing source pixels");
  auto pair=std::make_shared<const Image>(Image{2,1,{255,0,0,255,0,0,255,255}});auto wrapped=std::make_shared<PaintSourceMaps>();wrapped->color.image=pair;wrapped->color.min_filter=wrapped->color.mag_filter=GL_NEAREST;
  uint64_t shared_uploads=0;for(auto mode:{GL_REPEAT,GL_MIRRORED_REPEAT,GL_CLAMP_TO_EDGE})for(float offset:{1.25f,-.25f}){
    wrapped->color.wrap_s=mode;wrapped->color.uv_transform={0,0,0,0,offset,.5f};auto view=painted(wrapped);auto output=capture(renderer,snapshot(quad(),opaque,view->preview(opaque)));bool red=mode==GL_REPEAT?offset>0:offset<0;auto at=pixel(output);
    check(red?at[0]==255&&at[2]==0:at[2]==255&&at[0]==0,"GPU source affine lookup and repeat/mirror/clamp addressing preserve original sampler semantics");if(!shared_uploads)shared_uploads=renderer.texture_stats().uploads;else check(renderer.texture_stats().uploads==shared_uploads,"Sampler and transform edits reuse original GPU image storage");
  }
  auto separate=std::make_shared<PaintSourceMaps>();separate->color.image=pair;separate->emission.image=pair;separate->normal.image=pair;separate->metal_rough.image=pair;separate->occlusion.image=pair;
  auto shared=painted(separate);auto scene=snapshot(quad(),opaque,shared->preview(opaque));capture(renderer,scene);auto uploaded=renderer.texture_stats();check(uploaded.images==2,"One original image shares GPU storage by identity with distinct sRGB and linear encodings");
  auto changed=std::make_shared<MeshSnapshot>(*scene);changed->mesh.positions[0].x+=.1f;capture(renderer,changed);check(renderer.texture_stats().uploads==uploaded.uploads,"Geometry-only preview jobs preserve existing GPU images with zero pixel reupload");
  capture(renderer,snapshot(quad(),opaque,{}));check(renderer.texture_stats().images==0&&renderer.texture_stats().samplers==0&&renderer.texture_stats().bytes==0,"Stale GL image and sampler resources are released when the current snapshot no longer uses them");
  auto missing=std::make_shared<PaintSourceMaps>();missing->normal.image=flat({128,128,255,255});auto defaults=painted(missing);auto tint=white();tint.color={1,0,0,1};check(pixel(capture(renderer,snapshot(quad(),tint,defaults->preview(tint))))==std::array<uint8_t,4>{255,0,0,255},"Missing source slots use material defaults rather than black incomplete textures");
  auto mip=std::make_shared<PaintSourceMaps>();mip->color.image=pair;mip->color.min_filter=GL_LINEAR_MIPMAP_LINEAR;auto mip_paint=painted(mip);capture(renderer,snapshot(quad(),opaque,mip_paint->preview(opaque)));
  check(renderer.texture_stats().images==1&&renderer.texture_stats().bytes==12,"Original mipmapped sampler generates a GPU mip chain with explicitly accounted storage");
  SurfaceTextureLimits tiny;tiny.max_bytes=4;SurfaceRenderer limited(tiny);check(rejects([&]{capture(limited,scene);})&&limited.texture_stats().images==0,"GPU byte budgets fail explicitly before retaining partial oversized resources");
  auto original=std::make_shared<PaintSourceMaps>();original->color.image=flat({255,0,0,255});
  original->normal.image=flat({255,128,128,255});original->normal_scale=.15f;
  original->normal.uv_transform={0,-1,1,0,1,0};original->occlusion.image=flat({255,255,255,255});
  BakeSource source_view;source_view.mesh=quad();source_view.materials[0].material=lit;
  source_view.materials[0].maps=PaintedMesh::from_maps(quad(),original,64)->preview(lit);
  auto original_pixels=capture(renderer,snapshot(quad(),lit,source_view.materials[0].maps));
  BakeOptions source_options;source_options.resolution=64;source_options.ao_samples=0;source_options.padding=0;
  auto transferred=bake_mesh(quad(),source_view,source_options);
  BakedTextures source_bake;source_bake.maps=std::move(transferred.maps);source_bake.maps.normal=std::move(transferred.normal);
  for(auto coverage:transferred.coverage)source_bake.coverage.push_back(coverage==1?255:0);
  Scene transfer_scene;transfer_scene.active().mesh_active=true;transfer_scene.active().material=lit;
  transfer_scene.active().painted=std::make_shared<PaintedMesh>(quad(),64);transfer_scene.active().painted->install_bake(std::move(source_bake));
  Document transfer_document;transfer_document.replace(transfer_scene);check(transfer_document.wait_mesh(),"Original-source bake prepares its immutable GPU snapshot");
  check(difference(original_pixels,capture(renderer,transfer_document.frame().surface))<=6,
        "Native affine-normal/AO/PBR baking reconstructs independent original-source GPU pixels within8bit quantization");
  BakedTextures baked;baked.maps.color={64,64,std::vector<uint8_t>(64*64*4)};
  baked.maps.metal_rough=baked.maps.emission=baked.maps.normal=baked.maps.color;
  for(size_t i=0;i<baked.maps.color.rgba.size();++i){
    baked.maps.color.rgba[i]=std::array<uint8_t,4>{30,60,220,255}[i%4];
    baked.maps.metal_rough.rgba[i]=std::array<uint8_t,4>{0,180,0,255}[i%4];
    baked.maps.normal.rgba[i]=std::array<uint8_t,4>{128,204,230,255}[i%4];
  }
  baked.coverage.assign(64*64,255);
  Scene baked_scene;baked_scene.active().mesh_active=true;baked_scene.active().material=lit;
  baked_scene.active().painted=PaintedMesh::from_maps(quad(),original,64);
  baked_scene.active().painted->install_bake(baked);
  Document baked_document;baked_document.replace(baked_scene.snapshot());check(baked_document.wait_mesh(),"Mixed source/bake fixture publishes real worker tangent corners");
  auto mixed_pixels=capture(renderer,baked_document.frame().surface);
  baked_scene.active().painted=std::make_shared<PaintedMesh>(quad(),64);baked_scene.active().painted->install_bake(std::move(baked));
  baked_document.replace(baked_scene);check(baked_document.wait_mesh(),"Independent bake-only reference publishes worker tangent corners");
  check(difference(mixed_pixels,capture(renderer,baked_document.frame().surface))==0,
        "Source-backed baked GPU output exactly matches bake-only normal/AO/PBR despite conflicting original views and factors");
  for(int arg=first;arg<argc;++arg){auto path=std::filesystem::path(argv[arg]);bool female=path.filename().string().find("female")!=std::string::npos;auto created=create_humanoid(humanoid_baseline_defaults(female?"vroid-beta-female":"vroid-beta-male"),{},path.parent_path().parent_path());auto all=std::make_shared<MeshSnapshot>();auto start=std::chrono::steady_clock::now();auto builds=paint_composite_cache_stats().builds;uint64_t id=1;
    for(auto &part:created.parts){PaintSurfaceOptions options;options.policy=part.source_maps?PaintUvPolicy::SourceAuthored:PaintUvPolicy::Strict;auto mesh=part.source_maps?PaintedMesh::from_maps(part.mesh,part.source_maps,256,options):std::make_shared<PaintedMesh>(part.mesh,256);auto m=white();m.color=part.color;m.roughness=part.roughness;if(part.appearance){auto &a=part.appearance->asset->materials[part.appearance->material];m.metallic=a.metallic;m.emissive=a.emissive;m.double_sided=a.double_sided;m.unlit=a.unlit;m.alpha=a.alpha==ImportedMaterial::AlphaMode::Mask?Material::AlphaMode::Mask:a.alpha==ImportedMaterial::AlphaMode::Blend?Material::AlphaMode::Blend:Material::AlphaMode::Opaque;}
      auto maps=mesh->preview(m);size_t offset=all->mesh.positions.size(),first=all->mesh.indices.size();all->mesh.positions.insert(all->mesh.positions.end(),part.mesh.positions.begin(),part.mesh.positions.end());all->mesh.normals.insert(all->mesh.normals.end(),part.mesh.normals.begin(),part.mesh.normals.end());all->mesh.uv.insert(all->mesh.uv.end(),part.mesh.uv.begin(),part.mesh.uv.end());for(auto index:part.mesh.indices)all->mesh.indices.push_back(uint32_t(offset+index));all->ranges.push_back({id++,first,part.mesh.indices.size(),m,maps,part.appearance});
    }
    auto preparation=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();check(paint_composite_cache_stats().builds==builds,"Actual baseline first-preview preparation bypasses every original-source CPU composite build");start=std::chrono::steady_clock::now();renderer.draw(all,{{0,-.75f,-2},{},1},projection(-.6,.6,-.6,.6),{},Framebuffer::size,Framebuffer::size,true);glFinish();auto cold=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();auto resource=renderer.texture_stats();auto moved=std::make_shared<MeshSnapshot>(*all);moved->mesh.positions[0].x+=.001f;start=std::chrono::steady_clock::now();renderer.draw(moved,{{.01f,-.75f,-2},{},1},projection(-.6,.6,-.6,.6),{},Framebuffer::size,Framebuffer::size,true);glFinish();auto warm=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();check(renderer.texture_stats().uploads==resource.uploads,"Actual baseline repeated geometry/group preview performs zero original-image reuploads");std::cout<<"PROFILE "<<argv[arg]<<" preparation_ms="<<preparation<<" first_gpu_ms="<<cold<<" changed_geometry_ms="<<warm<<" images="<<resource.images<<" bytes="<<resource.bytes<<'\n';
  }
  std::cout<<"Native raw-source GPU preview tests complete\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
