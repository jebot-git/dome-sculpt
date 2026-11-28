// SPDX-License-Identifier: GPL-3.0-or-later

#include "dome/scene.hpp"
#include "dome/source-maps.hpp"
#include "emission-export.hpp"
#include <chrono>
#include <cstring>
#include <iostream>
#include <limits>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace {
using namespace dome;
constexpr uint32_t resolution=512;
thread_local bool host_reference=false;
void check(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}

Mesh mesh(){
  Mesh quad;
  quad.positions={{-.5f,-.5f,0},{.5f,-.5f,0},{.5f,.5f,0},{-.5f,.5f,0}};
  quad.normals.assign(4,{0,0,1});
  quad.uv={{{.18f,.17f}},{{.91f,.21f}},{{.85f,.79f}},{{.21f,.78f}}};
  quad.indices={0,1,2,0,2,3};
  return quad;
}
enum class Fixture { Paint, LegacyBake, DenseBake, PackedBake };
Scene model(Fixture fixture){
  Scene scene;
  auto &object=scene.active();
  object.mesh_active=true;
  object.material.emissive={384.25f,170.75f,12.25f};
  auto maps=std::make_shared<PaintSourceMaps>();
  Image original{37,29,std::vector<uint8_t>(37*29*4)};
  for(size_t i=0;i<original.rgba.size();++i)original.rgba[i]=uint8_t((i*151u)^(i/97));
  maps->emission.image=std::make_shared<const Image>(std::move(original));
  maps->emission.uv_transform={.87f,-1.15f,.63f,1.41f,-.375f,.23f};
  maps->emission.wrap_s=33648;
  maps->emission.wrap_t=10497;
  maps->emission.mag_filter=fixture==Fixture::DenseBake?9728:9729;
  auto paint=PaintedMesh::from_maps(mesh(),maps,resolution);
  object.painted=paint;
  if(fixture!=Fixture::Paint){
    const size_t count=size_t(resolution)*resolution;
    BakedTextures bake;
    for(auto *image:{&bake.maps.color,&bake.maps.metal_rough})
      *image={resolution,resolution,std::vector<uint8_t>(count*4,255)};
    bake.coverage.resize(count);
    for(size_t i=0;i<count;++i)bake.coverage[i]=uint8_t(i^(i/resolution));
    if(fixture==Fixture::LegacyBake){
      bake.maps.emission={resolution,resolution,std::vector<uint8_t>(count*4)};
      for(size_t i=0;i<count*4;++i)bake.maps.emission.rgba[i]=uint8_t((i*139u)^(i/41));
    }else if(fixture==Fixture::DenseBake){
      bake.emission_field=BakedEmissionField::generate(resolution,[](size_t i){
        const float coverage=i%23?float(i%257)/256:std::numeric_limits<float>::denorm_min();
        return BakedEmissionSample{{coverage*.375f,coverage*.125f,coverage},
            {coverage*12003.f,coverage*.00000317f,coverage*.73f},coverage};
      });
      check(!bake.emission_field->storage().dense.empty(),"Expected dense FLOAT Bake storage");
    }else{
      bake.emission_field=BakedEmissionField::generate_compact(resolution,[](size_t i){
        return i%11?BakedEmissionSample{{0,.5f,0},{12000,0,2},1}:BakedEmissionSample{};
      });
      check(!bake.emission_field->storage().binary_coverage.empty(),"Expected packed FLOAT coverage bits");
    }
    paint->install_bake(std::move(bake));
  }
  auto stroke=[&](PaintSettings settings,float y){
    Brush brush;
    brush.from={-.35f,y,0};brush.to={.38f,y+.04f,0};
    brush.radius=.26f;brush.pressure=.6f;brush.strength=.7f;brush.normal={0,0,1};
    settings.front_only=false;
    paint->begin_stroke();
    paint->stamp(brush,settings);
    check(paint->end_stroke(),"Emission fixture must author actual paint");
  };
  PaintSettings legacy;
  legacy.channel=PaintChannel::Emission;
  legacy.emission={.61f,.022f,.74f};
  stroke(legacy,-.08f);
  paint->layer_properties("Paint",true,.72f);
  PaintSettings physical=legacy;
  physical.emission_units=PaintEmissionUnits::Physical;
  physical.emission={254.5f,.000000135f,1.25f};
  paint->add_layer("Physical glaze");
  stroke(physical,-.16f);
  paint->layer_properties("Physical glaze",true,.43f);
  auto mask=legacy;
  mask.channel=PaintChannel::Mask;
  mask.mask=.29f;
  stroke(mask,-.16f);
  paint->add_layer("Hidden physical paint");
  physical.emission={9800,3300,200};
  stroke(physical,.15f);
  paint->layer_properties("Hidden physical paint",false,1);
  return scene;
}

using Pixel=detail::EmissionExrPixel;
bool exact(std::span<const Pixel> a,std::span<const Pixel> b){
  if(a.size()!=b.size())return false;
  for(size_t i=0;i<a.size();++i)if(std::memcmp(&a[i],&b[i],sizeof(Pixel))){
    std::cerr<<"First unequal texel "<<i<<'\n';
    for(unsigned k=0;k<10;++k)std::cerr<<k<<' '<<std::hex<<std::bit_cast<uint32_t>(a[i][k])<<' '
                                      <<std::bit_cast<uint32_t>(b[i][k])<<std::dec<<'\n';
    return false;
  }
  return true;
}
}

std::string_view actual_backend() asm("__real__ZN4dome15compute_backendEv");
std::string_view wrapped_backend() asm("__wrap__ZN4dome15compute_backendEv");
std::string_view wrapped_backend(){return host_reference?"CPU":actual_backend();}

int main(int argc,char **argv)try{
  if(argc>1&&std::string_view(argv[1])=="--missing"){
    const auto scene=model(Fixture::Paint);
    auto raster=detail::export_emission_raster(scene.active(),{},false);
    Pixel one;
    try{raster.read(10,std::span(&one,1));}
    catch(const std::exception &error){std::cout<<"REFUSED "<<error.what()<<'\n';return 0;}
    throw std::runtime_error("Missing selected device returned a FLOAT raster");
  }
  for(auto fixture:{Fixture::Paint,Fixture::LegacyBake,Fixture::DenseBake,Fixture::PackedBake}){
    const auto scene=model(fixture);
    const auto owner=scene.active().painted->baked();
    const auto layers=scene.active().painted->layers();
    auto cancelled=false;
    auto device=detail::export_emission_raster(scene.active(),[&]{return cancelled;},false);
    host_reference=true;
    auto reference=detail::export_emission_raster(scene.active(),{},false);
    host_reference=false;
    check(device.version==reference.version&&device.resolution==reference.resolution,"Raster metadata must be preserved");
    std::vector<Pixel> before(size_t(resolution)*resolution),after(before.size());
    const auto reference_begin=std::chrono::steady_clock::now();
    reference.read(0,before);
    const auto reference_ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-reference_begin).count();
    const auto begin=std::chrono::steady_clock::now();
    device.read(0,after);
    const auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-begin).count();
    check(exact(before,after),"Selected device changed legacy, absolute, coverage or resolved radiance words");
    std::vector<Pixel> stripe(size_t(resolution)*13+17);
    cancelled=true;
    bool refused=false;
    try{device.read(129,stripe);}catch(const std::runtime_error &){refused=true;}
    check(refused,"Cancellation must prevent reuse of the staged result");
    cancelled=false;
    device.read(129,stripe);
    check(exact(std::span(before).subspan(129*resolution,stripe.size()),stripe),"Partial read after cancellation changed absolute indexing");
    check(scene.active().painted->baked()==owner,"Selected device replaced an immutable Bake owner");
    for(size_t layer=0;layer<layers.size();++layer)
      check(layers[layer].tiles==scene.active().painted->layers()[layer].tiles&&
            layers[layer].emission_tiles==scene.active().painted->layers()[layer].emission_tiles,
            "Selected device replaced editable layer tiles");
    std::cout<<"PASS "<<int(fixture)<<' '<<before.size()<<" exact FLOAT pixels on "<<actual_backend()
             <<" in "<<ms<<" ms, host reference "<<reference_ms<<" ms\n";
  }
}catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
