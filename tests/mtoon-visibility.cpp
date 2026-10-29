// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_mtoon_renderer_reference
#include "mtoon-render.cpp"
#undef main
#include "dome/material-visibility.hpp"
#include "dome/material-bindings.hpp"
namespace {
std::shared_ptr<const Image> alpha_pair(bool inverse=false) {
  return std::make_shared<const Image>(Image{2,1,{255,255,255,uint8_t(inverse?255:0),255,255,255,uint8_t(inverse?0:255)}});
}
void parity(SurfaceRenderer &renderer,std::shared_ptr<MeshSnapshot> scene,
            std::array<bool,2> wanted,const char *message,double seconds=0) {
  renderer.set_frame_time(seconds);auto pixels=draw(renderer,scene);
  MaterialSurfaceQuery query(scene->mesh,scene->ranges);bool good=true;
  for(size_t n=0;n<2;++n) {
    int x=n?60:36,y=48;Vec3 origin{(x+.5f)/48-1,(y+.5f)/48-1,2};
    bool cpu=bool(query.raycast(origin,{0,0,-1},4,seconds));bool gpu=at(pixels,x,y)[3]!=0;
    good&=cpu==gpu && gpu==wanted[n];
    if(cpu!=gpu || gpu!=wanted[n])std::cerr<<message<<" sample="<<n<<" cpu="<<cpu<<" gpu="<<gpu<<" expected="<<wanted[n]<<'\n';
  }
  check(good,message);
}
}
int main(int argc,char **argv)try {
  NativeGraphics graphics;std::string device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));
  std::cout<<"GPU "<<device<<'\n';
  check(argc<2||std::string(argv[1])!="--hardware"||(device.find("llvmpipe")==std::string::npos&&device.find("softpipe")==std::string::npos),"Material visibility parity runs on requested hardware");
  Fbo framebuffer;framebuffer.bind();SurfaceRenderer renderer;auto native=white();native.alpha=Material::AlphaMode::Mask;native.double_sided=true;
  auto source=std::make_shared<PaintSourceMaps>();source->color.image=alpha_pair();source->color.min_filter=source->color.mag_filter=9728;
  auto paint=PaintedMesh::from_maps(quad(),source,64);auto generic=snapshot(native);generic->ranges[0].textures=paint->preview(native);
  parity(renderer,generic,{false,true},"PBR source-only alpha holes agree between production fragments and material picking");
  generic->ranges[0].textures=paint->composite(native);parity(renderer,std::make_shared<MeshSnapshot>(*generic),{false,true},"PBR derived protected alpha agrees between production fragments and material picking");
  for(bool legacy:{false,true}) {
    auto holder=std::make_shared<SourceMaterialBindings>();
    holder->material=legacy?MToonMaterial::from_vrm0({{"shader","VRM/MToon"},{"floatProperties",{{"_BlendMode",1},{"_CullMode",0}}}}):
      MToonMaterial::from_vrm1({{"alphaMode","MASK"},{"doubleSided",true},{"extensions",{{"VRMC_materials_mtoon",{{"specVersion","1.0"}}}}}});
    holder->material.parameters.alpha_cutoff=.5f;
    holder->material.parameters.base_color={1,1,1,1};holder->material.textures[0].index=99;
    holder->native_reference.color={1,1,1,1};holder->native_reference.double_sided=true;
    holder->roles[0].image=alpha_pair(true);holder->roles[0].min_filter=holder->roles[0].mag_filter=9728;
    auto scene=snapshot(native);scene->ranges[0].material_bindings=holder;scene->ranges[0].textures=paint->preview(native);
    parity(renderer,scene,{true,false},"Current explicit MToon role alpha wins without original appearance or historical texture registry");
    scene->ranges[0].appearance=appearance(typed(legacy),{alpha_pair()});scene=std::make_shared<MeshSnapshot>(*scene);
    parity(renderer,scene,{true,false},"Current explicit alpha remains authoritative over stale original appearance images");
    scene->ranges[0].textures=paint->composite(native);scene=std::make_shared<MeshSnapshot>(*scene);
    parity(renderer,scene,{true,false},"Protected derived alpha uses current typed role independently of original PBR source coverage");
    auto cutoff=std::make_shared<SourceMaterialBindings>(*holder);cutoff->native_reference.alpha_cutoff=.3f;
    cutoff->material.parameters.alpha_cutoff=.7f;cutoff->roles[0].image=image({255,255,255,153});
    scene=std::make_shared<MeshSnapshot>(*scene);scene->ranges[0].material_bindings=cutoff;scene->ranges[0].textures=paint->preview(native);
    parity(renderer,scene,{true,true},"Native cutoff override compares against resolved baseline identically in GPU and picker");
    auto protected_holder=std::make_shared<SourceMaterialBindings>(*holder);protected_holder->native_reference.color[3]=.603f;
    protected_holder->material.parameters.base_color[3]=.72f;protected_holder->material.parameters.alpha_cutoff=.7205f;
    protected_holder->roles[0].image=image({255,255,255,255});auto protected_native=native;protected_native.color[3]=.603f;
    auto solid=std::make_shared<PaintSourceMaps>();solid->color=protected_holder->roles[0];
    scene=std::make_shared<MeshSnapshot>(*scene);scene->ranges[0].material=protected_native;scene->ranges[0].material_bindings=protected_holder;
    scene->ranges[0].textures=PaintedMesh::from_maps(quad(),solid,64)->composite(protected_native);
    parity(renderer,scene,{false,false},"Zero coverage preserves exact typed opacity across a quantized PBR cutoff boundary");
    auto fractional=std::make_shared<SourceMaterialBindings>(*holder);fractional->roles[0].image=image({255,255,255,230});
    auto pbr=std::make_shared<PaintSourceMaps>();pbr->color.image=image({255,255,255,102});auto painter=PaintedMesh::from_maps(quad(),pbr,64);
    Brush brush;brush.from=brush.to={};brush.normal={0,0,1};brush.radius=4;PaintSettings settings;settings.color={0,1,0,.2f};
    painter->begin_stroke();painter->stamp(brush,settings);painter->end_stroke();painter->layer_properties("Fractional alpha",true,.5f);
    scene=std::make_shared<MeshSnapshot>(*scene);scene->ranges[0].material=native;scene->ranges[0].material_bindings=fractional;scene->ranges[0].textures=painter->composite(native);
    parity(renderer,scene,{true,true},"Independent fractional paint alpha reconstruction agrees with actual typed fragments");
    painter->layer_properties("Full alpha",true,1);scene=std::make_shared<MeshSnapshot>(*scene);scene->ranges[0].textures=painter->composite(native);
    parity(renderer,scene,{false,false},"Full paint replacement alpha masks out typed source identically in GPU and picker");
    auto animated=std::make_shared<SourceMaterialBindings>(*holder);animated->material.parameters.scroll={.5f,0};
    scene=std::make_shared<MeshSnapshot>(*scene);scene->ranges[0].textures=paint->preview(native);scene->ranges[0].material_bindings=animated;
    parity(renderer,scene,{true,false},"Explicit current animated alpha starts at identical GPU and picker coordinates",0);
    parity(renderer,std::make_shared<MeshSnapshot>(*scene),{false,true},"Explicit current alpha animation advances at shared GPU and picker frame time",1);
    auto reference=std::make_shared<SourceMaterialBindings>(*holder);reference->native_reference.color[3]=.5f;
    reference->material.parameters.base_color[3]=.8f;reference->material.parameters.alpha_cutoff=.35f;reference->roles[0].image=image({255,255,255,255});
    auto overridden=native;overridden.color[3]=.25f;scene=std::make_shared<MeshSnapshot>(*scene);scene->ranges[0].material=overridden;scene->ranges[0].material_bindings=reference;scene->ranges[0].textures=paint->preview(overridden);
    parity(renderer,scene,{true,true},"Resolved alpha reference ratio agrees in GPU and picker despite different raw material baseline");
    if(!legacy) {
      auto uv1=std::make_shared<SourceMaterialBindings>(*holder);uv1->material.textures[0].texcoord=1;
      scene=std::make_shared<MeshSnapshot>(*scene);scene->ranges[0].material=native;scene->ranges[0].material_bindings=uv1;scene->ranges[0].textures=paint->preview(native);
      parity(renderer,scene,{false,false},"Current explicit UV1 role alpha uses identical retained accessor coordinates in GPU and picker");
    } else {
      auto invalid=std::make_shared<SourceMaterialBindings>(*holder);invalid->material.textures[0].texcoord=1;
      check(rejects([&]{invalid->validate();}),"Legacy MToon refuses metadata that cannot represent an edited UV1 role");
    }
  }
  std::cout<<"Material GPU/picking parity complete: "<<checks<<" checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
