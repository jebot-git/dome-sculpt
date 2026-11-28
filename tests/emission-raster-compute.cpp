// SPDX-License-Identifier: GPL-3.0-or-later

#include "dome/scene.hpp"
#include "dome/material-bindings.hpp"
#include "dome/source-maps.hpp"
#include "emission-export.hpp"
#include "emission-raster-device.hpp"
#include "paint-emission-sample.hpp"
#include "paint-overlay-sample.hpp"
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
enum class Fixture { Paint, LegacyBake, DenseBake, PackedBake, BakeOnly };
enum class Typed { None, Legacy, Rotated, Wide, ZeroStrength };
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
  auto paint=fixture==Fixture::BakeOnly?std::make_shared<PaintedMesh>(mesh(),resolution):
                                       PaintedMesh::from_maps(mesh(),maps,resolution);
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
  if(fixture==Fixture::BakeOnly)return scene;
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
void sample_typed(Scene &scene,Typed fixture){
  auto &owner=scene.active();
  auto binding=std::make_shared<SourceMaterialBindings>();
  const size_t role=size_t(MToonSlot::Emission);
  if(fixture==Typed::Legacy)
    binding->material=MToonMaterial::from_vrm0({{"shader","VRM/MToon"},{"textureProperties",{{"_EmissionMap",0}}}});
  auto &slot=binding->material.textures[role];
  slot.index=0;
  auto &image=binding->roles[role];
  image=owner.painted->source_maps()->emission;
  binding->material.parameters.emission={.41f,.19f,.82f};
  binding->material.parameters.emission_strength=fixture==Typed::ZeroStrength?0.f:
                                                 fixture==Typed::Legacy?1.f:4.375f;
  binding->native_reference.emission={.41f,.19f,.82f};
  owner.material.emissive={.53f,.27f,.94f};
  if(fixture!=Typed::Legacy){
    slot.rotation=.4315f;
    slot.offset={-.1281f,.413f};
    slot.scale={-1.1357f,.837f};
  }
  if(fixture==Typed::Wide){
    slot.rotation=0;
    slot.offset={0x1.304p31f,0x1p60f};
    slot.scale={0x1p35f,-0x1p36f};
    image.uv_transform={1.375f,0,0x1p-30f,-1.f,-.25f,.25f};
    image.mag_filter=9729;
  }
  owner.material_bindings=std::move(binding);
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

void compare_bake_uv(){
  constexpr uint32_t extent=8;
  constexpr auto peak=std::numeric_limits<float>::max();
  constexpr auto tiny=8*std::numeric_limits<float>::denorm_min();
  const BakedEmissionSample authored{{.25f,.5f,1.f},{peak,tiny,12000.f},1.f};
  std::array<BakedEmissionSample,extent*extent> values{};
  values[1]=authored;
  values[35]=authored;
  const auto dense=BakedEmissionField::create(extent,values);
  const auto packed=BakedEmissionField::generate_compact(extent,[&](size_t at){return values[at];});
  check(!packed->storage().binary_coverage.empty(),"Bake UV fixture needs compact binary coverage");
  using Sample=detail::EmissionRasterDevice::BakeUvSample;
  const std::array taps{
    Sample{{1,0,0,0},{1,0,0,0}},
    Sample{{0,35,40,2},{.25f,.25f,.25f,.25f}},
    Sample{{1,14,35,3},{.25f,.25f,.25f,.25f}},
    Sample{}};
  const std::array expected{authored,
    BakedEmissionSample{{.0625f,.125f,.25f},{peak*.25f,tiny*.25f,3000.f},.25f},
    BakedEmissionSample{{.125f,.25f,.5f},{peak*.5f,tiny*.5f,6000.f},.5f},
    BakedEmissionSample{}};
  for(const auto &field:{dense,packed}){
    auto device=detail::create_bake_uv_emission_device(*field,taps.size(),{});
    std::array<BakedEmissionSample,taps.size()> out{};
    device->read_bake_uv(taps,out,{});
    check(std::memcmp(expected.data(),out.data(),sizeof(out))==0,
      "Bake UV GPU filter changed FLOAT coefficients, coverage or subnormal energy");
    bool cancelled=false;
    try{device->read_bake_uv(taps,out,[]{return true;});}
    catch(const std::runtime_error &){cancelled=true;}
    check(cancelled,"Bake UV GPU filter ignored cancellation");
    device->read_bake_uv(std::span(taps).subspan(1,1),std::span(out).first(1),{});
    check(std::memcmp(&expected[1],&out[0],sizeof(out[0]))==0,
      "Bake UV GPU short batch after cancellation used stale taps");
  }
  values[1]={{},{1e-7f,0,0},std::numeric_limits<float>::denorm_min()};
  const auto underflow=BakedEmissionField::create(extent,values);
  const auto device=detail::create_bake_uv_emission_device(*underflow,1,{});
  const Sample fractional{{1,0,0,0},{.25f,0,0,0}};
  BakedEmissionSample filtered;
  device->read_bake_uv(std::span(&fractional,1),std::span(&filtered,1),{});
  bool refused=false;
  try{filtered.validate();}catch(const std::invalid_argument &){refused=true;}
  check(refused&&filtered.coverage==0&&filtered.absolute[0]>0,
    "Bake UV GPU filter silently discarded energy when coverage underflowed");
  std::cout<<"PASS Bake UV exact dense/packed FLOAT, peak, subnormal and cancellation\n";
}

void compare_bake_uv_maps(){
  constexpr uint32_t extent=64;
  constexpr size_t count=extent*extent;
  using Device=detail::EmissionRasterDevice;
  BakedTextures bake;
  bake.coverage.resize(count);
  for(auto *image:{&bake.maps.color,&bake.maps.metal_rough,&bake.maps.emission,
                   &bake.maps.normal,&bake.maps.mtoon_normal})
    *image={extent,extent,std::vector<uint8_t>(count*4)};
  std::array<Device::BakeUvFrame,4> frames{};
  for(unsigned face=0;face<frames.size();++face)for(unsigned corner=0;corner<3;++corner){
    const Vec3 n{(float(corner)-1)*.12f,(float(face)-1)*.08f,1};
    const float w=face%2?-1.f:1.f;
    auto &frame=frames[face];
    using detail::NormalVertexBitangent;
    frame.before[corner]=detail::normal_bake_vertex(n,{1,.1f*corner,0},w,NormalVertexBitangent::RawCross);
    const Vec3 tangent=face%2?Vec3{0,-1,.1f*corner}:Vec3{.2f,1,.04f*corner};
    frame.after[corner]=detail::normal_bake_vertex(n,tangent,face<2?w:-w,NormalVertexBitangent::RawCross);
  }
  std::vector<Device::BakeUvSample> taps(count);
  for(size_t at=0;at<count;++at){
    bake.coverage[at]=at%7?uint8_t(at^at/17):0;
    unsigned seed=29;
    for(auto *image:{&bake.maps.color,&bake.maps.metal_rough,&bake.maps.emission,
                     &bake.maps.normal,&bake.maps.mtoon_normal}){
      for(unsigned c=0;c<4;++c)image->rgba[at*4+c]=uint8_t((at*(seed+c+2))^(at/(23+c)));
      seed+=7;
    }
    const float u=float(at%127)/128,v=float(at%31)/32;
    if(at%9)taps[at]={{uint32_t(at),uint32_t((at+13)%count),uint32_t((at+37)%count),uint32_t((at+31)%count)},
                     {(1-u)*(1-v),u*(1-v),(1-u)*v,u*v},uint32_t(at%frames.size()),
                     {float(at%61)/128,float(at%57)/128}};
  }
  const auto product=[](float a,float b){volatile float value=a*b;return value;};
  const auto add=[](float a,float b){volatile float value=a+b;return value;};
  const auto quotient=[](float a,float b){volatile float divisor=b,value=a/divisor;return value;};
  const auto unit=[&](uint8_t value){return quotient(float(value),255);};
  const auto byte=[](float value){return uint8_t(std::lround(std::clamp(value,0.f,1.f)*255));};
  const auto color=[&](float value){return byte(linear_to_srgb(value));};
  std::vector<Device::BakeUvMapsPixel> expected(count),actual(count);
  for(size_t at=0;at<count;++at){
    float coverage=0,rgba[4]{},orm[3]{},emission[3]{},normals[2][3]{};
    const auto &sample=taps[at];
    for(unsigned tap=0;tap<4;++tap){
      const size_t source=sample.texels[tap],i=source*4;
      const float alpha=product(sample.weights[tap],unit(bake.coverage[source]));
      coverage=add(coverage,alpha);
      for(unsigned c=0;c<3;++c){
        rgba[c]=add(rgba[c],product(srgb_to_linear(unit(bake.maps.color.rgba[i+c])),alpha));
        orm[c]=add(orm[c],product(unit(bake.maps.metal_rough.rgba[i+c]),alpha));
        emission[c]=add(emission[c],product(srgb_to_linear(unit(bake.maps.emission.rgba[i+c])),alpha));
        normals[0][c]=add(normals[0][c],product(add(product(unit(bake.maps.normal.rgba[i+c]),2),-1),alpha));
        normals[1][c]=add(normals[1][c],product(add(product(unit(bake.maps.mtoon_normal.rgba[i+c]),2),-1),alpha));
      }
      rgba[3]=add(rgba[3],product(unit(bake.maps.color.rgba[i+3]),alpha));
    }
    auto &out=expected[at];
    out.coverage=byte(coverage);
    if(!out.coverage)continue;
    for(unsigned c=0;c<3;++c){
      out.color[c]=color(quotient(rgba[c],coverage));
      out.orm[c]=byte(quotient(orm[c],coverage));
      out.legacy[c]=color(quotient(emission[c],coverage));
    }
    out.color[3]=byte(quotient(rgba[3],coverage));
    out.orm[3]=out.legacy[3]=255;
    const auto &frame=frames[sample.face];
    const std::array<double,3> bary{sample.bary[0],sample.bary[1],1.-sample.bary[0]-sample.bary[1]};
    const auto basis=[&](const auto &columns){
      return detail::normal_bake_basis({columns[0],columns[1],columns[2]},bary,detail::NormalFragmentBasis::RawColumns);
    };
    const auto before=basis(frame.before),after=basis(frame.after);
    for(unsigned role=0;role<2;++role){
      const auto reciprocal=quotient(1,coverage);
      const auto q=Vec3{normals[role][0],normals[role][1],normals[role][2]}*reciprocal;
      const auto desired=before.t*q.x+before.b*q.y+before.n*q.z;
      const auto encoded=detail::encode_normal_bake(after,length(desired)>1e-8f?desired:before.n);
      check(encoded&&encoded->quantized_angle_radians<=.02,"Bake UV quality fixture needs a valid frame");
      std::copy(encoded->rgba.begin(),encoded->rgba.end(),out.normals[role]);
    }
  }
  auto device=detail::create_bake_uv_maps_device(bake,extent,frames,count,{});
  device->read_bake_uv_maps(taps,actual,{},{});
  const auto compare=[&]{
    for(size_t i=0;i<count;++i){
      if(std::memcmp(&expected[i],&actual[i],sizeof(actual[i]))==0)continue;
      const auto expected_bytes=std::as_bytes(std::span(expected).subspan(i,1));
      const auto actual_bytes=std::as_bytes(std::span(actual).subspan(i,1));
      for(size_t byte=0;byte<expected_bytes.size();++byte)
        if(expected_bytes[byte]!=actual_bytes[byte])
          std::cerr<<"GPU UV Bake texel "<<i<<" byte "<<byte<<": expected "
                   <<int(expected_bytes[byte])<<", got "<<int(actual_bytes[byte])<<'\n';
      throw std::runtime_error("GPU UV Bake maps differ from the CPU reference");
    }
  };
  compare();
  bake.maps.emission={};
  bake.maps.mtoon_normal={};
  bake.emission_field=BakedEmissionField::generate_compact(extent,[](size_t at){
    return at%2?BakedEmissionSample{{.25f,0,0},{12345.f,.125f,0},1}:BakedEmissionSample{};
  });
  for(auto &sample:expected){
    std::fill_n(sample.legacy,4,0);
    std::fill_n(sample.normals[1],4,0);
  }
  device=detail::create_bake_uv_maps_device(bake,extent,frames,count,{});
  auto emission_device=detail::create_bake_uv_emission_device(*bake.emission_field,count,{});
  std::vector<BakedEmissionSample> reference(count),fused(count);
  emission_device->read_bake_uv(taps,reference,{});
  device->read_bake_uv_maps(taps,actual,fused,{});
  compare();
  check(std::memcmp(reference.data(),fused.data(),count*sizeof(fused[0]))==0,
        "Fusing RGB8 Bake UV maps changed the FLOAT coefficients");
  // Include a non-singular frame that exceeds the angular error limit.
  auto invalid=frames;
  for(auto &corner:invalid[0].after)corner.b={};
  invalid[1].after[1].w=-invalid[1].after[0].w;
  for(unsigned corner=0;corner<3;++corner){
    using detail::NormalVertexBitangent;
    invalid[3].before[corner]=detail::normal_bake_vertex({0,0,1},{1,0,0},1,NormalVertexBitangent::RawCross);
    invalid[3].after[corner]=detail::normal_bake_vertex({0,0,1},{.02f,0,1},1,NormalVertexBitangent::RawCross);
  }
  bake.coverage[0]=255;
  const std::array<uint8_t,3> high_error{102,178,135};
  std::copy(high_error.begin(),high_error.end(),bake.maps.normal.rgba.begin());
  const auto &bad=invalid[3].after[0];
  const auto bad_encoding=detail::encode_normal_bake({bad.t,bad.b,bad.n},
      {unit(high_error[0])*2-1,unit(high_error[1])*2-1,unit(high_error[2])*2-1});
  check(bad_encoding&&bad_encoding->quantized_angle_radians>.02,
        "Frame refusal fixture must fail quantization and pass the determinant gate");
  device=detail::create_bake_uv_maps_device(bake,extent,invalid,4,{});
  const Device::BakeUvSample angular{{0,0,0,0},{1,0,0,0},3,{.5f,.125f}};
  const std::array failures{taps[8],taps[5],angular,taps[10]};
  device->read_bake_uv_maps(failures,std::span(actual).first(failures.size()),{},{});
  for(unsigned i=0;i<failures.size();++i){
    check(actual[i].coverage>0,"Frame refusal fixture must sample covered data");
    check(bool(actual[i].invalid_normal)==(i<3),"Device accepted an invalid Bake UV tangent frame");
  }
  std::cout<<"PASS Bake UV maps exact RGBA/ORM/legacy/coverage, frame-transformed normals and fused FLOAT "<<count<<" texels\n";
}

void compare_finite(Fixture fixture,uint32_t extent,bool original){
  const auto scene=model(fixture);
  const auto &object=scene.active();
  const auto &paint=*object.painted;
  const auto *source=original?&paint.source_maps()->emission:nullptr;
  const float offset=!source&&extent>resolution?-float(extent/resolution-1)/(2*extent):0.f;
  auto cancelled=false;
  const auto stop=[&]{return cancelled;};
  const auto begin=std::chrono::steady_clock::now();
  auto device=detail::create_finite_emission_raster_device(paint,source,object.material.emissive,extent,offset,stop);
  using Radiance=detail::EmissionRasterDevice::Radiance;
  std::vector<Radiance> pixels(size_t(extent)*extent);
  device->read_finite(0,pixels,stop);
  const auto milliseconds=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-begin).count();
  const detail::PaintEmissionSampler sampler(paint);
  for(size_t i=0;i<pixels.size();++i){
    const float u=(float(i%extent)+.5f)/extent-offset,v=(float(i/extent)+.5f)/extent-offset;
    const auto p=sampler.sample(u,v);
    const auto s=source?sample_source_texture(*source,u,v,true):std::array<float,4>{1,1,1,1};
    for(unsigned k=0;k<3;++k){
      const float expected=float(double(object.material.emissive[k])*s[k]*(1-p.coverage)+p.legacy[k]+p.absolute[k]);
      if(std::bit_cast<uint32_t>(expected)!=std::bit_cast<uint32_t>(pixels[i][k])){
        std::cerr<<"Finite fixture "<<int(fixture)<<" resolution "<<extent<<" (source "<<original<<") pixel "<<i<<" channel "<<k
                 <<": expected "<<std::hex<<std::bit_cast<uint32_t>(expected)<<" got "<<std::bit_cast<uint32_t>(pixels[i][k])<<std::dec<<'\n';
        throw std::runtime_error("Selected device changed the finite FLOAT radiance");
      }
    }
  }
  const size_t first=19*extent+73;
  std::vector<Radiance> stripe(2*extent+7);
  cancelled=true;
  bool refused=false;
  try{device->read_finite(first,stripe,stop);}catch(const std::runtime_error &){refused=true;}
  check(refused,"Cancellation must stop a finite GPU raster read");
  cancelled=false;
  device->read_finite(first,stripe,stop);
  check(std::memcmp(pixels.data()+first,stripe.data(),stripe.size()*sizeof(Radiance))==0,
        "Partial finite read after cancellation must preserve indexing");
  std::cout<<"PASS finite fixture="<<int(fixture)<<" source="<<original<<" extent="<<extent<<' '
           <<pixels.size()<<" exact RGB FLOAT pixels in "<<milliseconds<<" ms\n";
}

void compare_mapped(Fixture fixture,uint32_t extent,bool original){
  using Device=detail::EmissionRasterDevice;
  const auto scene=model(fixture);
  const auto &object=scene.active();
  const auto &paint=*object.painted;
  const auto *source=original?&paint.source_maps()->emission:nullptr;
  auto device=detail::create_mapped_emission_raster_device(paint,source,object.material.emissive,extent,Device::mapped_batch,{});
  std::vector<Device::MappedSample> requests(Device::mapped_batch);
  std::vector<Device::Radiance> result(requests.size());
  const detail::PaintEmissionSampler sampler(paint);
  for(size_t i=0;i<requests.size();++i){
    const auto at=uint32_t((i*541u+11)%(extent*extent));
    // Donor coordinates are independent of the writable texel and cross
    // the texture's repeat and mirrored-repeat seams.
    requests[i]={at,float(int(i%149)-75)/64,float(int(i%173)-61)/64};
  }
  bool cancelled=false;
  const auto stop=[&]{return cancelled;};
  device->read_mapped(requests,result,stop);
  for(size_t i=0;i<requests.size();++i){
    const auto &q=requests[i];
    const auto p=sampler.sample((float(q.texel%extent)+.5f)/extent,(float(q.texel/extent)+.5f)/extent);
    const auto s=source?sample_source_texture(*source,q.u,q.v,true):std::array<float,4>{1,1,1,1};
    for(unsigned k=0;k<3;++k){
      const float expected=float(double(object.material.emissive[k])*s[k]*(1-p.coverage)+p.legacy[k]+p.absolute[k]);
      check(std::bit_cast<uint32_t>(expected)==std::bit_cast<uint32_t>(result[i][k]),
            "Mapped GPU emission changed binary32 radiance");
    }
  }
  cancelled=true;
  Device::Radiance sample;
  bool refused=false;
  try{device->read_mapped(std::span(requests).last(1),std::span(&sample,1),stop);}
  catch(const std::runtime_error &){refused=true;}
  check(refused,"Mapped emission must observe cancellation before submitting work");
  cancelled=false;
  device->read_mapped(std::span(requests).last(1),std::span(&sample,1),stop);
  check(std::memcmp(&sample,&result.back(),sizeof(sample))==0,"Mapped emission changed on retry");
  std::cout<<"PASS mapped fixture="<<int(fixture)<<" source="<<original<<" extent="<<extent<<' '
           <<result.size()<<" exact RGB FLOAT samples\n";
}

enum class SetBackdrop { Constant, Core, MToon };

void compare_material(bool baked,uint32_t extent){
  using namespace paint_compute;
  const auto texture=[](unsigned seed){
    auto image=std::make_shared<Image>(Image{131,127,std::vector<uint8_t>(131*127*4)});
    for(size_t i=0;i<image->rgba.size();++i)image->rgba[i]=uint8_t((i*(seed+3))^(i/31));
    return image;
  };
  auto maps=std::make_shared<PaintSourceMaps>();
  maps->uv1=std::make_shared<const std::vector<std::array<float,2>>>(mesh().uv);
  maps->color.image=texture(43);
  maps->metal_rough.image=texture(74);
  maps->emission.image=maps->color.image;
  maps->occlusion.image=texture(109);
  maps->metal_rough.texcoord=maps->occlusion.texcoord=1;
  maps->metal_rough.mag_filter=9728;
  maps->occlusion.wrap_t=33071;
  maps->emission.uv_transform={.8f,-.3f,1.2f,1.4f,-.23f,.84f};
  maps->emission.wrap_s=33648;
  maps->occlusion_strength=.61f;
  constexpr size_t native=64;
  auto paint=std::make_shared<PaintedMesh>(mesh(),native);
  if(baked){
    BakedTextures bake;
    for(auto *image:{&bake.maps.color,&bake.maps.metal_rough,&bake.maps.emission}){
      *image={native,native,std::vector<uint8_t>(native*native*4)};
      for(size_t i=0;i<image->rgba.size();++i)image->rgba[i]=uint8_t((i*131u)^(i/17u));
    }
    bake.coverage.resize(native*native);
    for(size_t i=0;i<bake.coverage.size();++i)bake.coverage[i]=uint8_t(i^(i/native));
    paint->install_bake(std::move(bake));
  }
  const auto stroke=[&](PaintChannel channel,float y){
    PaintSettings settings;
    settings.channel=channel;
    settings.color={.89f,.013f,.43f,.42f};
    settings.metallic=.83f;
    settings.roughness=.47f;
    settings.emission={.16f,.007f,.29f};
    settings.mask=.3f;
    settings.front_only=false;
    Brush brush;
    brush.from={-.35f,y,0};brush.to={.38f,y+.04f,0};
    brush.radius=.26f;brush.pressure=.6f;brush.strength=.7f;brush.normal={0,0,1};
    paint->begin_stroke();
    paint->stamp(brush,settings);
    check(paint->end_stroke(),"Material fixture must author paint");
  };
  stroke(PaintChannel::Color,0);
  stroke(PaintChannel::Metallic,-.16f);
  paint->add_layer("Second material glaze");
  stroke(PaintChannel::Color,.19f);
  stroke(PaintChannel::Emission,-.12f);
  stroke(PaintChannel::Roughness,.2f);
  stroke(PaintChannel::Mask,-.05f);
  paint->layer_properties("Second material glaze",true,.61f);
  paint->add_layer("Hidden material glaze");
  stroke(PaintChannel::Color,-.19f);
  paint->layer_properties("Hidden material glaze",false,1);
  Material material;
  material.color={.83f,.7f,.45f,.88f};
  material.metallic=.78f;
  material.roughness=.43f;
  material.emissive={.25f,.16f,.3f};
  using Raster=detail::SourceMaterialRaster;
  const Raster::Roles roles{&maps->color,&maps->metal_rough,&maps->emission,&maps->occlusion};
  std::vector<uint32_t> texels(4096);
  for(size_t i=0;i<texels.size();++i)texels[i]=uint32_t((i*541u+11)%(extent*extent));
  const auto coordinate=[](unsigned set,size_t i){
    return detail::SourceUvSampler::Coordinate{float(int((i+set*19)%149)-75)/64,float(int((i+set*23)%173)-61)/64};
  };
  const auto byte=[](float value){return uint8_t(std::lround(std::clamp(value,0.f,1.f)*255));};
  const auto packet=detail::PaintOverlayFactory::capture(*paint);
  std::vector<Output> expected(texels.size());
  for(size_t i=0;i<texels.size();++i){
    const auto at=texels[i];
    const auto overlay=detail::sample_overlay_values(*packet,(float(at%extent)+.5f)/extent,(float(at/extent)+.5f)/extent,true);
    std::array<std::array<float,4>,SourceRoleCount> source;
    for(unsigned role=0;role<roles.size();++role){
      const auto uv=coordinate(roles[role]->texcoord,i);
      auto view=*roles[role];
      view.texcoord=0;
      source[role]=sample_source_texture(view,uv.u,uv.v,role==SourceColor||role==SourceEmission);
    }
    auto &out=expected[i];
    for(unsigned k=0;k<4;++k){
      const float color=material.color[k]*source[SourceColor][k]*(1-overlay[3][0])+overlay[0][k];
      out.color[k]=byte(k<3?linear_to_srgb(color):color);
      if(k==3)out.transparent=color<.999f;
    }
    const auto ao=1-maps->occlusion_strength*(1-source[SourceOcclusion][0]);
    out.orm[0]=byte(ao*(1-overlay[1][3])+overlay[1][0]);
    out.orm[1]=byte(material.roughness*source[SourceOrm][1]*(1-overlay[3][2])+overlay[1][1]);
    out.orm[2]=byte(material.metallic*source[SourceOrm][2]*(1-overlay[3][1])+overlay[1][2]);
    out.orm[3]=out.emission[3]=255;
    for(unsigned k=0;k<3;++k)
      out.emission[k]=byte(linear_to_srgb(material.emissive[k]*source[SourceEmission][k]*(1-overlay[3][3])+overlay[2][k]+overlay[4][k]));
    out.color_coverage=byte(overlay[3][0]);
    out.emission_coverage=byte(overlay[3][3]);
  }
  for(size_t bytes:{size_t(6*1024*1024),size_t(640*1024)}){
    auto device=detail::create_source_material_raster(*paint,material,roles,maps->occlusion_strength,extent,bytes,{});
    std::vector<Output> got(texels.size());
    for(size_t first=0;first<texels.size();){
      const auto count=std::min(device->capacity(),texels.size()-first);
      unsigned coordinate_batches=0;
      device->read_material(std::span(texels).subspan(first,count),[&](unsigned role,auto uv){
        ++coordinate_batches;
        for(size_t i=0;i<uv.size();++i)uv[i]=coordinate(roles[role]->texcoord,first+i);
      },std::span(got).subspan(first,count),{});
      check(coordinate_batches==2,"Only the two actual coordinate sets should be uploaded");
      first+=count;
    }
    for(size_t i=0;i<got.size();++i)
      check(std::memcmp(&expected[i],&got[i],sizeof(Output))==0,"Mapped GPU material changed portable material bytes");
    bool refused=false;
    try{
      device->read_material(std::span(texels).first(1),{},std::span(got).first(1),[]{return true;});
    }catch(const std::runtime_error &){refused=true;}
    check(refused,"Cancellation must stop mapped material composition");
    std::cout<<"PASS material extent="<<extent<<" bake="<<baked<<" budget="<<bytes
             <<" capacity="<<device->capacity()<<' '<<got.size()<<" exact RGBA/ORM/emission/coverage samples\n";
  }
}

void compare_texture_set(Fixture fixture,SetBackdrop backdrop){
  const auto scene=model(fixture);
  const auto &paint=*scene.active().painted;
  const auto *source=backdrop==SetBackdrop::Core?&paint.source_maps()->emission:nullptr;
  const auto background=scene.active().material.emissive;
  constexpr float strength=6.125f;
  Image aligned;
  if(backdrop==SetBackdrop::MToon){
    aligned={resolution,resolution,std::vector<uint8_t>(size_t(resolution)*resolution*4)};
    for(size_t i=0;i<aligned.rgba.size();++i)aligned.rgba[i]=uint8_t((i*53u)^(i/73));
  }
  const auto begin=std::chrono::steady_clock::now();
  auto device=detail::create_texture_set_emission_device(paint,source,aligned.rgba.empty()?nullptr:&aligned,
                                                         background,strength,{});
  std::vector<Pixel> got(size_t(resolution)*resolution),expected(got.size());
  device->read(0,got,{});
  const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-begin).count();
  const detail::PaintEmissionSampler sampler(paint);
  for(size_t at=0;at<expected.size();++at){
    const auto x=uint32_t(at%resolution),y=uint32_t(at/resolution);
    auto bg=background;
    if(source){
      const auto texel=sample_source_texture(*source,(float(x)+.5f)/resolution,(float(y)+.5f)/resolution,true);
      for(unsigned k=0;k<3;++k)bg[k]*=texel[k];
    }else if(!aligned.rgba.empty()){
      for(unsigned k=0;k<3;++k)bg[k]*=mtoon_srgb_to_linear(aligned.rgba[4*at+k]/255.f);
    }
    const auto p=sampler.sample_texel(x,y);
    const auto rgb=p.resolve(bg,strength);
    expected[at]={rgb[0],rgb[1],rgb[2],p.legacy[0],p.legacy[1],p.legacy[2],
                  p.absolute[0],p.absolute[1],p.absolute[2],p.coverage};
  }
  check(exact(expected,got),"Texture-set device changed an authored FLOAT coefficient or resolved radiance");
  std::vector<Pixel> stripe(2*resolution+7);
  const size_t first=19*resolution+73;
  device->read(first,stripe,{});
  check(exact(std::span(expected).subspan(first,stripe.size()),stripe),"Partial texture-set GPU read changed indexing");
  std::vector<detail::EmissionRasterDevice::TextureSetPreview> preview(stripe.size());
  device->read_texture_set_preview(first,preview,{});
  for(size_t i=0;i<preview.size();++i){
    const auto &full=expected[first+i];
    const std::array compact{full[0],full[1],full[2],full[9]};
    check(std::memcmp(preview[i].data(),compact.data(),sizeof(compact))==0,"Compact texture-set preview changed RGB or coverage");
  }
  std::cout<<"PASS texture-set fixture="<<int(fixture)<<" backdrop="<<int(backdrop)<<' '
           <<got.size()<<" exact FLOAT texels in "<<elapsed<<" ms\n";
}
}

std::string_view actual_backend() asm("__real__ZN4dome15compute_backendEv");
std::string_view wrapped_backend() asm("__wrap__ZN4dome15compute_backendEv");
std::string_view wrapped_backend(){return host_reference?"CPU":actual_backend();}

int main(int argc,char **argv)try{
  if(argc>1&&std::string_view(argv[1])=="--material"){
    compare_material(false,64);
    compare_material(true,128);
    return 0;
  }
  if(argc>1&&std::string_view(argv[1])=="--missing-mapped"){
    const auto scene=model(Fixture::BakeOnly);
    try{
      auto device=detail::create_mapped_emission_raster_device(*scene.active().painted,nullptr,{2,0,0},512,1,{});
      const detail::EmissionRasterDevice::MappedSample request{0,0,0};
      detail::EmissionRasterDevice::Radiance one;
      device->read_mapped(std::span(&request,1),std::span(&one,1),{});
    }catch(const std::exception &error){std::cout<<"REFUSED "<<error.what()<<'\n';return 0;}
    throw std::runtime_error("Missing selected device returned a mapped FLOAT raster");
  }
  if(argc>1&&std::string_view(argv[1])=="--mapped"){
    compare_mapped(Fixture::Paint,512,true);
    compare_mapped(Fixture::LegacyBake,256,true);
    compare_mapped(Fixture::DenseBake,1024,true);
    compare_mapped(Fixture::PackedBake,1024,true);
    compare_mapped(Fixture::BakeOnly,512,false);
    const auto scene=model(Fixture::BakeOnly);
    const auto &paint=*scene.active().painted;
    auto device=detail::create_mapped_emission_raster_device(paint,nullptr,{30001,0,0},512,1,{});
    const detail::EmissionRasterDevice::MappedSample request{0,0,0};
    detail::EmissionRasterDevice::Radiance one;
    bool refused=false;
    try{device->read_mapped(std::span(&request,1),std::span(&one,1),{});}
    catch(const std::invalid_argument &e){refused=std::string_view(e.what())=="Native physical radiance is outside its finite range";}
    check(refused,"Mapped GPU emission must refuse above-range radiance before FLOAT rounding");
    return 0;
  }
  if(argc>1&&std::string_view(argv[1])=="--texture-set"){
    compare_texture_set(Fixture::BakeOnly,SetBackdrop::Constant);
    compare_texture_set(Fixture::LegacyBake,SetBackdrop::Core);
    compare_texture_set(Fixture::DenseBake,SetBackdrop::Core);
    compare_texture_set(Fixture::PackedBake,SetBackdrop::MToon);
    auto scene=model(Fixture::BakeOnly);
    auto &paint=*scene.active().painted;
    auto bake=*paint.baked();
    constexpr auto maximum=std::numeric_limits<float>::max();
    bake.emission_field=BakedEmissionField::generate_compact(resolution,[](size_t){
      return BakedEmissionSample{{.5f,0,0},{maximum,0,0},1};
    });
    paint.install_bake(std::move(bake));
    auto device=detail::create_texture_set_emission_device(paint,nullptr,nullptr,{0,0,0},maximum,{});
    detail::EmissionRasterDevice::TextureSetPreview pixel;
    bool refused=false;
    try{device->read_texture_set_preview(size_t(resolution)*resolution/2+resolution/2,std::span(&pixel,1),{});}
    catch(const std::overflow_error &error){refused=std::string_view(error.what())=="Baked emission radiance exceeds FLOAT range";}
    check(refused,"The typed preview must preserve FLOAT overflow refusal");
    return 0;
  }
  if(argc>1&&std::string_view(argv[1])=="--missing-texture-set"){
    const auto scene=model(Fixture::BakeOnly);
    try{
      auto device=detail::create_texture_set_emission_device(*scene.active().painted,nullptr,nullptr,{1,1,1},1,{});
      Pixel one;
      device->read(0,std::span(&one,1),{});
    }catch(const std::exception &error){std::cout<<"REFUSED "<<error.what()<<'\n';return 0;}
    throw std::runtime_error("Missing selected device returned a texture-set FLOAT raster");
  }
  if(argc>1&&std::string_view(argv[1])=="--finite"){
    compare_bake_uv();
    compare_bake_uv_maps();
    compare_finite(Fixture::BakeOnly,512,false);
    compare_finite(Fixture::LegacyBake,256,false);
    compare_finite(Fixture::Paint,512,true);
    compare_finite(Fixture::DenseBake,1024,false);
    compare_finite(Fixture::PackedBake,1024,true);
    compare_finite(Fixture::DenseBake,256,true);
    const auto scene=model(Fixture::BakeOnly);
    auto device=detail::create_finite_emission_raster_device(*scene.active().painted,nullptr,{30001,0,0},512,0,{});
    detail::EmissionRasterDevice::Radiance one;
    bool refused=false;
    try{device->read_finite(0,std::span(&one,1),{});}
    catch(const std::invalid_argument &e){refused=std::string_view(e.what())=="Native physical radiance is outside its finite range";}
    check(refused,"Finite GPU path must refuse above-range radiance before FLOAT rounding");
    return 0;
  }
  if(argc>1&&std::string_view(argv[1])=="--missing-finite"){
    const auto scene=model(Fixture::BakeOnly);
    try{
      auto finite=detail::create_finite_emission_raster_device(*scene.active().painted,nullptr,{2,0,0},512,0,{});
      detail::EmissionRasterDevice::Radiance one;
      finite->read_finite(0,std::span(&one,1),{});
    }catch(const std::exception &e){std::cout<<"REFUSED "<<e.what()<<'\n';return 0;}
    throw std::runtime_error("Missing selected device returned a finite FLOAT raster");
  }
  if(argc>1&&std::string_view(argv[1]).starts_with("--missing")){
    const bool typed=std::string_view(argv[1])=="--missing-typed";
    auto scene=model(Fixture::Paint);
    if(typed)sample_typed(scene,Typed::Rotated);
    auto raster=detail::export_emission_raster(scene.active(),{},typed);
    Pixel one;
    try{raster.read(10,std::span(&one,1));}
    catch(const std::exception &error){std::cout<<"REFUSED "<<error.what()<<'\n';return 0;}
    throw std::runtime_error("Missing selected device returned a FLOAT raster");
  }
  const auto compare=[](Fixture fixture,Typed typed){
    auto scene=model(fixture);
    if(typed!=Typed::None)sample_typed(scene,typed);
    const auto owner=scene.active().painted->baked();
    const auto layers=scene.active().painted->layers();
    auto cancelled=false;
    auto device=detail::export_emission_raster(scene.active(),[&]{return cancelled;},typed!=Typed::None);
    host_reference=true;
    auto reference=detail::export_emission_raster(scene.active(),{},typed!=Typed::None);
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
    std::cout<<"PASS fixture="<<int(fixture)<<" typed="<<int(typed)<<' '<<before.size()<<" exact FLOAT pixels on "<<actual_backend()
             <<" in "<<ms<<" ms, host reference "<<reference_ms<<" ms\n";
  };
  for(auto fixture:{Fixture::BakeOnly,Fixture::Paint,Fixture::LegacyBake,Fixture::DenseBake,Fixture::PackedBake})
    compare(fixture,Typed::None);
  compare(Fixture::Paint,Typed::Legacy);
  compare(Fixture::PackedBake,Typed::Rotated);
  compare(Fixture::DenseBake,Typed::Wide);
  compare(Fixture::LegacyBake,Typed::ZeroStrength);
  auto overflow=model(Fixture::Paint);
  sample_typed(overflow,Typed::Rotated);
  auto binding=std::make_shared<SourceMaterialBindings>(*overflow.active().material_bindings);
  auto &slot=binding->material.textures[size_t(MToonSlot::Emission)];
  slot.rotation=0;
  slot.scale={std::numeric_limits<float>::max(),1};
  slot.offset={std::numeric_limits<float>::max(),0};
  overflow.active().material_bindings=std::move(binding);
  for(bool reference:{false,true}){
    host_reference=reference;
    auto raster=detail::export_emission_raster(overflow.active(),{},true);
    Pixel pixel;
    bool refused=false;
    try{raster.read(7,std::span(&pixel,1));}
    catch(const std::runtime_error &error){refused=std::string_view(error.what())=="MToon UV overflow";}
    check(refused,"MToon export must refuse an overflowing FLOAT coordinate");
  }
}catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
