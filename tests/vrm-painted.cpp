// SPDX-License-Identifier: GPL-3.0-or-later
#include "vrm-painted.hpp"
#include <iostream>
using namespace dome;
namespace {
void check(bool ok,const char *message) { if(!ok)throw std::runtime_error(message);std::cout<<"PASS "<<message<<'\n'; }
template<class F> bool rejects(F f) {try{f();return false;}catch(const std::exception&){return true;}}
Image image(uint32_t size,std::array<uint8_t,4> color) {
  Image result{size,size,std::vector<uint8_t>(size_t(size)*size*4)};
  for(size_t i=0;i<result.rgba.size();++i)result.rgba[i]=color[i%4];return result;
}
uint8_t byte(float x) {return uint8_t(std::lround(std::clamp(x,0.f,1.f)*255));}
}
int main() try {
  ObjectSurface object;object.mesh.positions={{0,0,0},{1,0,0},{0,1,0}};
  object.mesh.normals.assign(3,{0,0,1});object.mesh.indices={0,1,2};object.mesh.uv={{{0,0}},{{1,0}},{{0,1}}};
  object.material.color={0,.2f,.3f,1};
  auto base=std::make_shared<const Image>(image(128,{128,160,192,255}));
  std::array<PaintSourceTexture,size_t(MToonSlot::Count)> roles;
  roles[0].image=roles[1].image=base;
  auto typed=MToonMaterial::from_vrm0({{"name","Shared"},{"shader","VRM/MToon"},
       {"textureProperties",{{"_MainTex",0},{"_ShadeTexture",0}}}});
  typed.parameters.base_color={0,.1f,.6f,1};typed.parameters.shade_color={.7f,.6f,.5f};
  auto maps=std::make_shared<TextureMaps>();auto source=std::make_shared<PaintSourceMaps>();source->color=roles[0];maps->source_maps=source;
  maps->color=image(64,{0,0,0,255});maps->color_coverage=image(64,{0,0,0,255});maps->emission=image(64,{0,32,64,255});
  maps->emission_coverage=image(64,{255,0,0,255});
  std::array<float,3> source_value{};for(size_t c=0;c<3;++c)source_value[c]=srgb_to_linear(base->rgba[c]/255.f);
  for(size_t y=0;y<64;++y)for(size_t x=0;x<64;++x) {
    size_t at=(y*64+x)*4;uint8_t mask=x<20?0:x<44?128:255;maps->color_coverage.rgba[at]=mask;
    for(size_t c=0;c<3;++c) {
      float paint=std::array<float,3>{.8f,.6f,.4f}[c];
      maps->color.rgba[at+c]=byte(linear_to_srgb(object.material.color[c]*source_value[c]*(1-mask/255.f)+paint*mask/255.f));
    }
  }
  object.textures=maps;SourceNativeMaterial reference;reference.color=object.material.color;
  auto before=maps->color.rgba;
  auto compiled=detail::vrm_painted_material(object,typed,roles,reference,64,{});
  check(compiled.material.parameters.base_color==std::array<float,4>{1,1,1,1} && compiled.roles[0].image && compiled.roles[1].image,
        "Painted VRM0 emits explicit neutral main factor and separate reconstructed shade map");
  check(compiled.roles[1].wrap_s==roles[1].wrap_s && compiled.roles[1].wrap_t==roles[1].wrap_t &&
        compiled.roles[1].min_filter==roles[1].min_filter && compiled.roles[1].mag_filter==roles[1].mag_filter,
        "Reconstructed shade retains original role wrapping and filtering rather than acquiring a clamp sampler");
  for(uint32_t x:{10u,32u,55u}) {
    auto at=(size_t(32)*64+x)*4;float coverage=maps->color_coverage.rgba[at]/255.f;
    auto main=sample_source_texture(compiled.roles[0],(x+.5f)/64,32.5f/64,true);
    auto shade=sample_source_texture(compiled.roles[1],(x+.5f)/64,32.5f/64,true);
    for(size_t c=0;c<3;++c) {
      float generic=srgb_to_linear(maps->color.rgba[at+c]/255.f);
      float native=object.material.color[c];
      float expected=coverage==0?typed.parameters.base_color[c]*source_value[c]:
        std::max(0.f,generic+(typed.parameters.base_color[c]-native)*source_value[c]*(1-coverage));
      if(std::abs(main[c]-expected)>.006f)throw std::runtime_error("Independent protected/partial/full main color differs");
      float paint=coverage==0?0:std::max(0.f,generic-native*source_value[c]*(1-coverage));
      float dark=source_value[c]*(1-coverage)+(native?paint/native:paint);
      std::array<float,3> old_factor{typed.parameters.shade_color.x,typed.parameters.shade_color.y,typed.parameters.shade_color.z};
      std::array<float,3> new_factor{compiled.material.parameters.shade_color.x,compiled.material.parameters.shade_color.y,compiled.material.parameters.shade_color.z};
      if(std::abs(shade[c]*new_factor[c]-dark*old_factor[c])>.016f)
        throw std::runtime_error("Independent aliased shade differs, including zero native/base channel");
    }
  }
  check(true,"Independent protected, fractional and full paint equations retain zero-channel original shade");
  auto emitted=sample_source_texture(compiled.roles[3],.5f,.5f,true);
  check(compiled.material.parameters.shade_color.y>1 && std::abs(emitted[1]-srgb_to_linear(32/255.f))<.003f &&
        compiled.material.parameters.emission.x==1 && compiled.material.parameters.emission.y==1,
        "HDR shade range uses a typed factor while emission pixels and factors are applied once");
  auto emitter=typed;emitter.parameters.emission={3,.2f,.8f};
  auto emitter_roles=roles;emitter_roles[3].image=std::make_shared<const Image>(image(32,{200,180,160,255}));
  emitter.textures[3].index=1;
  auto emission_maps=std::make_shared<TextureMaps>(*maps);auto emission_source=std::make_shared<PaintSourceMaps>(*source);
  emission_source->emission=emitter_roles[3];emission_maps->source_maps=emission_source;
  emission_maps->emission=image(32,{32,64,96,255});emission_maps->emission_coverage=image(32,{0,0,0,255});
  for(size_t y=0;y<32;++y)for(size_t x=0;x<32;++x) {
    auto at=(y*32+x)*4;uint8_t mask=x<10?255:x<22?128:0;emission_maps->emission_coverage.rgba[at]=mask;
    if(mask)for(size_t c=0;c<3;++c)emission_maps->emission.rgba[at+c]=byte(linear_to_srgb(std::array<float,3>{.2f,.4f,.6f}[c]*mask/255.f));
  }
  auto emission_object=object;emission_object.textures=emission_maps;
  auto exact_emission=detail::vrm_painted_material(emission_object,emitter,emitter_roles,reference,64,{});
  auto generic_emission=PaintSourceTexture{};generic_emission.image=std::shared_ptr<const Image>(emission_maps,&emission_maps->emission);generic_emission.wrap_s=generic_emission.wrap_t=33071;
  auto emission_mask=generic_emission;emission_mask.image=std::shared_ptr<const Image>(emission_maps,&emission_maps->emission_coverage);
  for(uint32_t x:{10u,32u,55u}) {
    float u=(x+.5f)/64,v=32.5f/64;
    auto mask=sample_source_texture(emission_mask,u,v,false)[0];
    auto generic=sample_source_texture(generic_emission,u,v,true);
    auto original=sample_source_texture(emitter_roles[3],u,v,true);
    auto actual=sample_source_texture(exact_emission.roles[3],u,v,true);
    std::array<float,3> old{3,.2f,.8f},factor{exact_emission.material.parameters.emission.x,exact_emission.material.parameters.emission.y,exact_emission.material.parameters.emission.z};
    for(size_t c=0;c<3;++c) {
      float expected=mask==0?old[c]*original[c]:generic[c]+old[c]*original[c]*(1-mask);
      if(std::abs(actual[c]*factor[c]-expected)>.015f)throw std::runtime_error("Independent separate-grid HDR emission expectation differs");
    }
  }
  check(exact_emission.material.parameters.emission.x>1,
        "Independent emission coverage grid preserves protected, fractional and full HDR typed emission");
  auto protected_emission=std::make_shared<TextureMaps>(*emission_maps);protected_emission->emission_coverage=image(32,{0,0,0,255});
  emission_object.textures=protected_emission;
  auto unchanged_emission=detail::vrm_painted_material(emission_object,emitter,emitter_roles,reference,64,{});
  check(unchanged_emission.roles[3].image==emitter_roles[3].image && unchanged_emission.material.parameters.emission.x==3 &&
        !unchanged_emission.rewritten_properties.contains("_EmissionColor"),
        "Unpainted emission retains its original role and factor so material expressions keep their property semantics");
  auto nonalias=typed;nonalias.textures[1].index=1;
  auto independent=detail::vrm_painted_material(object,nonalias,roles,reference,64,{});
  check(independent.roles[1].image==base && independent.material.parameters.shade_color.y==typed.parameters.shade_color.y,
        "Separate material role identities do not acquire shadow paint merely because images happen to match");
  auto resolved=object;auto binding=std::make_shared<SourceMaterialBindings>();binding->material=nonalias;binding->roles=roles;binding->native_reference=reference;resolved.material_bindings=binding;
  auto resolved_alias=detail::vrm_painted_material(resolved,nonalias,roles,reference,64,{});
  check(resolved_alias.roles[1].image->rgba==compiled.roles[1].image->rgba && resolved_alias.material.parameters.shade_color.y==compiled.material.parameters.shade_color.y,
        "Resolved current shade/base roles alias by image, sampler and UV identity even when historical texture indices differ");
  auto protected_maps=std::make_shared<TextureMaps>(*maps);protected_maps->color_coverage=image(1,{0,0,0,255});protected_maps->emission_coverage=image(1,{0,0,0,255});resolved.textures=protected_maps;
  auto preserved=typed;preserved.parameters.scroll={.1f,.2f};auto untouched=detail::vrm_painted_material(resolved,preserved,roles,reference,64,{});
  check(untouched.roles[0].image==roles[0].image && untouched.material.parameters.scroll==preserved.parameters.scroll && untouched.rewritten_properties.empty(),
        "Exactly zero common color/emission replacement preserves animated MToon roles and expression property identity");
  auto full=detail::vrm_painted_material(object,typed,roles,reference,0,{});
  check(full.roles[0].image->width==128 && full.roles[1].image->width==128,
        "Default painted export preserves original role resolution rather than silently choosing paint resolution");
  auto transformed=typed;transformed.textures[0].offset={.1f,0};
  check(rejects([&]{detail::vrm_painted_material(object,transformed,roles,reference,64,{});}),
        "Shared ST requires explicit all-slot bake instead of changing only MainTex");
  auto animated=typed;animated.parameters.scroll={.1f,0};
  check(rejects([&]{detail::vrm_painted_material(object,animated,roles,reference,64,{});}),
        "Animated source UVs are never silently frozen for a fixed paint overlay");
  auto malformed=std::make_shared<TextureMaps>(*maps);malformed->color_coverage.rgba.pop_back();
  auto invalid=object;invalid.textures=malformed;
  check(rejects([&]{detail::vrm_painted_material(invalid,typed,roles,reference,64,{});}),
        "Malformed coverage storage fails before image publication");
  auto hdr=object;hdr.material.color[1]=1;reference.color[1]=.01f;
  check(rejects([&]{detail::vrm_painted_material(hdr,typed,roles,reference,64,{});}),
        "Unrepresentable main HDR factors reject rather than clipping material data");
  check(rejects([&]{detail::vrm_painted_material(object,typed,roles,reference,64,[]{return true;});}) && maps->color.rgba==before,
        "Cancellation preserves every original immutable composite byte");
  return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
