// SPDX-License-Identifier: GPL-3.0-or-later
// Reuse the independent CPU lighting expectation and isolated EGL/FBO helpers.
#define main existing_mtoon_reference_suite
#include "mtoon-render.cpp"
#undef main
#include "dome/material-bindings.hpp"
namespace {
bool short_buffer_limit=false,all_gpu_copies=true;
size_t gpu_copy_calls=0;
float decode(uint8_t value){return mtoon_srgb_to_linear(value/255.f);}
Vec3 product(Vec3 a,Vec3 b){return {a.x*b.x,a.y*b.y,a.z*b.z};}
Vec3 rgb(const Image &value){return {decode(value.rgba[0]),decode(value.rgba[1]),decode(value.rgba[2])};}
Image square(std::array<uint8_t,4> value){Image out{64,64,std::vector<uint8_t>(64*64*4)};for(size_t n=0;n<out.rgba.size();++n)out.rgba[n]=value[n%4];return out;}
struct EmissionFixture {
  Material native=white();MToonMaterial material;
  std::shared_ptr<PaintSourceMaps> source=std::make_shared<PaintSourceMaps>();
  std::shared_ptr<PaintedMesh> paint;
  std::shared_ptr<CreatorAppearance> authored;
  std::shared_ptr<MeshSnapshot> scene;
  std::shared_ptr<const Image> original;
  Vec3 effective;
  explicit EmissionFixture(bool legacy):material(typed(legacy)) {
    native.color={0,0,0,1};native.emissive={.4f,0,.3f};
    material.parameters.base_color={0,0,0,1};material.parameters.shade_color={};
    material.parameters.emission={.3f,.6f,.9f};material.parameters.emission_strength=legacy?1:1.5f;
    material.parameters.cull=MToonCull::Off;material.textures[3].index=0;
    original=image({160,96,48,255});source->color.image=image({255,255,255,255});
    source->emission.image=image({32,64,96,255});source->emission.min_filter=source->emission.mag_filter=GL_NEAREST;
    paint=PaintedMesh::from_maps(quad(),source,64);authored=appearance(material,{original});
    auto asset=std::make_shared<ImportedAsset>(*authored->asset);asset->materials[0].color=native.color;
    asset->materials[0].emissive={.2f,0,.6f};authored->asset=asset;
    effective={.6f,.6f,.45f};scene=snapshot(native);scene->ranges[0].appearance=authored;
  }
  std::array<int,4> expected_pixel(Vec3 emission)const {
    auto neutral=material;neutral.parameters.emission={1,1,1};MToonSamples samples;samples.emission=emission;
    return expected(neutral,samples);
  }
  void composite(){scene=std::make_shared<MeshSnapshot>(*scene);scene->ranges[0].textures=paint->composite(native);}
};
}
extern "C" {
void __real_glGetTexImage(GLenum,GLint,GLenum,GLenum,void*);
void __real_glGetIntegerv(GLenum,GLint*);
void __wrap_glGetIntegerv(GLenum name,GLint *value) {
  __real_glGetIntegerv(name,value);if(short_buffer_limit && name==GL_MAX_TEXTURE_BUFFER_SIZE)*value=1;
}
void __wrap_glGetTexImage(GLenum target,GLint level,GLenum format,GLenum type,void *offset) {
  GLint buffer=0;__real_glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING,&buffer);
  all_gpu_copies&=buffer!=0;++gpu_copy_calls;
  if(!buffer)throw std::runtime_error("Production source pack attempted a host texture download");
  __real_glGetTexImage(target,level,format,type,offset);
}
}
int main(int argc,char **argv)try {
  NativeGraphics graphics;std::string device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));
  GLint units=0,texels=0;glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS,&units);glGetIntegerv(GL_MAX_TEXTURE_BUFFER_SIZE,&texels);
  std::cout<<"GPU "<<device<<" fragment_units="<<units<<" buffer_texels="<<texels<<'\n';
  check(argc<2||std::string(argv[1])!="--hardware"||(device.find("llvmpipe")==std::string::npos&&device.find("softpipe")==std::string::npos),
        "Emission and GPU packing tests run on the requested hardware");
  Fbo framebuffer;framebuffer.bind();SurfaceRenderer renderer;renderer.set_frame_time(0);
  for(bool legacy:{false,true}) {
    auto material=typed(legacy);material.textures[0].index=material.textures[1].index=0;
    material.parameters.base_color={1,1,1,1};material.parameters.shade_color={1,1,1};
    material.parameters.shift=legacy?1:-1;material.parameters.toony=0;material.parameters.cull=MToonCull::Off;
    auto binding=std::make_shared<SourceMaterialBindings>();binding->material=material;binding->native_reference.color={1,1,1,1};
    binding->roles[0].image=image({255,0,0,255});binding->roles[1].image=image({0,0,255,255});
    auto source=std::make_shared<PaintSourceMaps>();source->color=binding->roles[0];auto paint=PaintedMesh::from_maps(quad(),source,64);
    Brush brush;brush.from=brush.to={};brush.normal={0,0,1};brush.radius=4;PaintSettings settings;settings.color={0,1,0,1};
    paint->begin_stroke();paint->stamp(brush,settings);paint->end_stroke();
    auto scene=snapshot();scene->ranges[0].textures=paint->composite(white());scene->ranges[0].material_bindings=binding;
    MToonSamples samples;samples.base={0,1,0,1};samples.shade={0,0,1};
    check(close(at(draw(renderer,scene)),expected(material,samples),3),
          "Derived Base/Shade roles with equal historical indices remain independent when current images differ");
  }
  for(bool legacy:{false,true}) {
    EmissionFixture fixture(legacy);fixture.scene->ranges[0].textures=fixture.paint->preview(fixture.native);
    auto wanted=fixture.expected_pixel(product(rgb(*fixture.original),fixture.effective));
    check(close(at(draw(renderer,fixture.scene)),wanted,3),"Raw typed emission keeps independent role RGB, nonzero/zero original factors and authored strength");
    fixture.composite();check(close(at(draw(renderer,fixture.scene)),wanted,3),"Zero emission coverage restores exact typed emission rather than quantized generic PBR emission");
    Brush brush;brush.from=brush.to={};brush.normal={0,0,1};brush.radius=.6f;PaintSettings settings;
    settings.color={0,0,0,1};fixture.paint->begin_stroke();fixture.paint->stamp(brush,settings);fixture.paint->end_stroke();fixture.composite();
    check(fixture.scene->ranges[0].textures->color_coverage.width>1 && fixture.scene->ranges[0].textures->emission_coverage.width==1 &&
          close(at(draw(renderer,fixture.scene)),wanted,3),"Color-only paint does not replace an independent protected emission channel");
    settings.channel=PaintChannel::Emission;settings.emission={.8f,.1f,.2f};fixture.paint->begin_stroke();fixture.paint->stamp(brush,settings);fixture.paint->end_stroke();
    const auto &pixel=fixture.paint->pixel(fixture.paint->active_layer(),32,32);
    Vec3 painted{decode(pixel.data[6]),decode(pixel.data[7]),decode(pixel.data[8])};
    for(float opacity:{0.f,.25f,.5f,1.f}) {
      fixture.paint->layer_properties("Emission",true,opacity);fixture.composite();
      float coverage=pixel.data[12]/255.f*pixel.data[13]/255.f*opacity;
      auto value=product(rgb(*fixture.original),fixture.effective)*(1-coverage)+painted*coverage;
      auto actual=at(draw(renderer,fixture.scene)),wanted=fixture.expected_pixel(value);
      if(!close(actual,wanted,3))std::cerr<<"EM legacy="<<legacy<<" opacity="<<opacity<<" coverage="<<coverage<<" pixelcoverage="<<int(pixel.data[12])<<" actual="<<actual[0]<<","<<actual[1]<<","<<actual[2]<<" wanted="<<wanted[0]<<","<<wanted[1]<<","<<wanted[2]<<"\n";
      check(close(actual,wanted,3),"Protected, fractional and full emission preserve differing typed/PBR roles and factors through editable layers");
    }
    auto stats=renderer.texture_stats();draw(renderer,fixture.scene);auto shifted=std::make_shared<MeshSnapshot>(*fixture.scene);shifted->mesh.positions[0].x+=.001f;draw(renderer,shifted);
    check(stats.packed_bytes>0 && stats.pack_builds>0 && renderer.texture_stats().pack_builds==stats.pack_builds &&
          renderer.texture_stats().uploads==stats.uploads && renderer.texture_stats().bytes==stats.bytes,
          "Repeated and geometry-only stereo views reuse immutable GPU packs with no image upload or repacking");
    fixture.paint=PaintedMesh::from_maps(quad(),fixture.source,64);BakedTextures baked;
    baked.maps.color=square({0,0,0,255});baked.maps.metal_rough=square({255,255,0,255});baked.maps.emission=square({0,128,0,255});baked.coverage.resize(64*64);
    for(int y=0;y<64;++y)for(int x=0;x<64;++x)baked.coverage[y*64+x]=x<22?0:x<43?128:255;
    fixture.paint->install_bake(baked);fixture.composite();auto output=draw(renderer,fixture.scene);Vec3 value{0,decode(128),0};
    for(auto [x,coverage]:std::array<std::pair<int,float>,3>{{{24,0},{48,128/255.f},{72,1}}})
      check(close(at(output,x,48),fixture.expected_pixel(product(rgb(*fixture.original),fixture.effective)*(1-coverage)+value*coverage),3),
            "Source-aware baked emission retains typed originals under protected and fractional bake coverage");
    auto binding=std::make_shared<SourceMaterialBindings>();binding->material=fixture.material;
    binding->native_reference={fixture.native.color,{.2f,0,.6f},1,.5f,false};binding->roles[3].image=image({16,160,224,255});
    binding->roles[3].min_filter=binding->roles[3].mag_filter=GL_NEAREST;
    fixture.scene->ranges[0].material_bindings=binding;fixture.scene->ranges[0].textures=PaintedMesh::from_maps(quad(),fixture.source,64)->preview(fixture.native);
    check(close(at(draw(renderer,std::make_shared<MeshSnapshot>(*fixture.scene))),fixture.expected_pixel(product(rgb(*binding->roles[3].image),fixture.effective)),3),
          "Explicit current material role images override historical appearance indices without rewriting provenance");
    fixture.native.emissive[1]=.25f;fixture.scene=std::make_shared<MeshSnapshot>(*fixture.scene);fixture.scene->ranges[0].material=fixture.native;
    fixture.scene->ranges[0].textures=PaintedMesh::from_maps(quad(),fixture.source,64)->composite(fixture.native);
    check(close(at(draw(renderer,fixture.scene)),fixture.expected_pixel(product(rgb(*binding->roles[3].image),{.6f,.25f,.45f})),3),
          "Nonzero native edits over an originally zero emission component use the explicit absolute factor rule once");
  }
  for(int filter:{GL_NEAREST,GL_LINEAR,GL_NEAREST_MIPMAP_NEAREST,GL_LINEAR_MIPMAP_NEAREST,GL_NEAREST_MIPMAP_LINEAR,GL_LINEAR_MIPMAP_LINEAR}) {
    EmissionFixture fixture(false);auto patterned=std::make_shared<Image>(square({0,0,0,255}));
    for(int y=0;y<64;++y)for(int x=0;x<64;++x)for(int c=0;c<3;++c)patterned->rgba[(y*64+x)*4+c]=((x+y)&1)?std::array<uint8_t,3>{224,48,96}[c]:std::array<uint8_t,3>{16,192,128}[c];
    fixture.original=patterned;fixture.material.textures[3].scale={48,48};fixture.authored=appearance(fixture.material,{patterned});
    auto asset=std::make_shared<ImportedAsset>(*fixture.authored->asset);asset->materials[0].color=fixture.native.color;asset->materials[0].emissive={.2f,0,.6f};asset->textures[0].min_filter=filter;asset->textures[0].mag_filter=GL_LINEAR;fixture.authored->asset=asset;
    fixture.scene->ranges[0].appearance=fixture.authored;fixture.scene->ranges[0].textures=fixture.paint->preview(fixture.native);auto native=draw(renderer,fixture.scene);
    fixture.composite();check(delta(native,draw(renderer,fixture.scene))<=4,"GPU-packed typed emission matches original native nearest/linear/mipmap filtering in linear light");
  }
  for(int wrap:{GL_REPEAT,GL_MIRRORED_REPEAT,GL_CLAMP_TO_EDGE}) {
    EmissionFixture fixture(false);fixture.original=std::make_shared<const Image>(Image{2,1,{224,16,64,255,16,128,224,255}});
    fixture.material.textures[3].offset={1.1f,-.2f};fixture.material.textures[3].scale={0,0};fixture.authored=appearance(fixture.material,{fixture.original});
    auto asset=std::make_shared<ImportedAsset>(*fixture.authored->asset);asset->materials[0].color=fixture.native.color;asset->materials[0].emissive={.2f,0,.6f};asset->textures[0].wrap_s=asset->textures[0].wrap_t=wrap;asset->textures[0].min_filter=asset->textures[0].mag_filter=GL_LINEAR;fixture.authored->asset=asset;
    fixture.scene->ranges[0].appearance=fixture.authored;fixture.scene->ranges[0].textures=fixture.paint->preview(fixture.native);auto native=draw(renderer,fixture.scene);
    fixture.composite();check(delta(native,draw(renderer,fixture.scene))<=3,"Packed emission applies independent affine, repeat/mirror/clamp and decode-before-linear-filter semantics");
  }
  EmissionFixture animated(false);animated.original=std::make_shared<const Image>(Image{2,1,{255,0,0,255,0,0,255,255}});animated.material.parameters.scroll={.5f,0};animated.authored=appearance(animated.material,{animated.original});
  auto asset=std::make_shared<ImportedAsset>(*animated.authored->asset);asset->materials[0].color=animated.native.color;asset->materials[0].emissive={.2f,0,.6f};animated.authored->asset=asset;animated.scene->ranges[0].appearance=animated.authored;animated.composite();
  renderer.set_frame_time(0);auto first=draw(renderer,animated.scene);auto packing=renderer.texture_stats().pack_builds;renderer.set_frame_time(1);auto second=draw(renderer,animated.scene);
  check(at(first)!=at(second) && renderer.texture_stats().pack_builds==packing,"Typed emission animation advances on GPU through protected coverage without static baking or repacking");renderer.set_frame_time(0);
  EmissionFixture state(false);state.composite();GLuint buffer=0,texture=0;glGenBuffers(1,&buffer);glBindBuffer(GL_PIXEL_PACK_BUFFER,buffer);glBufferData(GL_PIXEL_PACK_BUFFER,64,nullptr,GL_STATIC_COPY);glGenTextures(1,&texture);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_BUFFER,texture);glTexBuffer(GL_TEXTURE_BUFFER,GL_RGBA8,buffer);
  constexpr std::array<GLenum,8> pack_names{GL_PACK_ALIGNMENT,GL_PACK_ROW_LENGTH,GL_PACK_IMAGE_HEIGHT,GL_PACK_SKIP_PIXELS,GL_PACK_SKIP_ROWS,GL_PACK_SKIP_IMAGES,GL_PACK_SWAP_BYTES,GL_PACK_LSB_FIRST};
  constexpr std::array<GLint,8> pack_values{8,17,19,2,3,4,1,1};
  for(size_t n=0;n<pack_names.size();++n)glPixelStorei(pack_names[n],pack_values[n]);
  renderer.draw(state.scene,{},identity,{0,0,2},96,96,true);GLint packed=0,binding=0;bool layout=true;
  glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING,&packed);glGetIntegerv(GL_TEXTURE_BINDING_BUFFER,&binding);
  for(size_t n=0;n<pack_names.size();++n){GLint value=0;glGetIntegerv(pack_names[n],&value);layout&=value==pack_values[n];}
  check(GLuint(packed)==buffer && layout && GLuint(binding)==texture,
        "GPU source copies restore caller pixel-pack buffer, pixel layout and texture-buffer bindings");
  glBindBuffer(GL_PIXEL_PACK_BUFFER,0);for(size_t n=0;n<pack_names.size();++n)glPixelStorei(pack_names[n],n==0?4:0);glBindTexture(GL_TEXTURE_BUFFER,0);glDeleteTextures(1,&texture);glDeleteBuffers(1,&buffer);
  auto stats=renderer.texture_stats();SurfaceTextureLimits budget;budget.max_bytes=stats.bytes-1;SurfaceRenderer limited(budget);
  check(rejects([&]{draw(limited,state.scene);}) && limited.texture_stats().images==0,"Combined original/mip/packed GPU byte budgets reject oversized candidates without retained partial resources");
  short_buffer_limit=true;SurfaceRenderer narrow;auto refused=rejects([&]{draw(narrow,state.scene);});short_buffer_limit=false;
  check(refused && narrow.texture_stats().images==0,"Actual texture-buffer texel limits are enforced before packing allocation");
  check(gpu_copy_calls>0 && all_gpu_copies,"Every production texture transfer targets an unmapped GPU pixel-pack buffer rather than host memory");
  std::cout<<"MToon emission/packed-source tests complete: "<<checks<<" checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
