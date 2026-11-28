// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/bake.hpp"
#ifdef DOME_TEST_BAKE_RAY_DEVICE
#include "bake-material-input.hpp"
#include "bake-raster-device.hpp"
#include "bake-ray-device.hpp"
#include "bake-ray-query.hpp"
#include "bake-sampler-cache.hpp"
#include "dome/color-preparation.hpp"
#include "emission-raster-device.hpp"
#include "paint-overlay-sample.hpp"
#include "source-color-fixtures.hpp"
#endif
#include <algorithm>
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
namespace {
void check(bool value,const char *message) {
  if (!value) throw std::runtime_error(message);
  std::cout<<"PASS "<<message<<'\n';
}
template<class F>bool rejects(F &&f) {try{f();return false;}catch(const std::exception&){return true;}}
Mesh quad(float left=-1,float right=1,float z=0,float u0=0,float u1=1,float v0=0,float v1=1,Vec3 normal={0,0,1}) {
  Mesh mesh;mesh.positions={{left,-1,z},{right,-1,z},{right,1,z},{left,1,z}};
  mesh.normals.assign(4,normal);mesh.indices={0,1,2,0,2,3};mesh.uv={{{u0,v0}},{{u1,v0}},{{u1,v1}},{{u0,v1}}};return mesh;
}
void append(Mesh &to,const Mesh &from) {
  auto offset=uint32_t(to.positions.size());to.positions.insert(to.positions.end(),from.positions.begin(),from.positions.end());
  to.normals.insert(to.normals.end(),from.normals.begin(),from.normals.end());to.uv.insert(to.uv.end(),from.uv.begin(),from.uv.end());
  for(auto i:from.indices)to.indices.push_back(i+offset);
}
Mesh box(float r) {
  Mesh result;
  std::array<std::array<Vec3,4>,6> faces{{
      {{{-r,-r,r},{r,-r,r},{r,r,r},{-r,r,r}}},
      {{{r,-r,-r},{-r,-r,-r},{-r,r,-r},{r,r,-r}}},
      {{{r,-r,r},{r,-r,-r},{r,r,-r},{r,r,r}}},
      {{{-r,-r,-r},{-r,-r,r},{-r,r,r},{-r,r,-r}}},
      {{{-r,r,r},{r,r,r},{r,r,-r},{-r,r,-r}}},
      {{{-r,-r,-r},{r,-r,-r},{r,-r,r},{-r,-r,r}}}
  }};
  for(auto face:faces){Mesh m;m.positions.assign(face.begin(),face.end());m.normals.assign(4,normalized(cross(face[1]-face[0],face[2]-face[0])));m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.indices={0,1,2,0,2,3};append(result,m);}return result;
}
Image constant(std::array<uint8_t,4> color) {return {1,1,{color[0],color[1],color[2],color[3]}};}
std::array<uint8_t,4> pixel(const Image &image,int x,int y) {
  auto p=image.rgba.data()+(size_t(y)*image.width+x)*4;return {p[0],p[1],p[2],p[3]};
}
bool near(int a,int b,int tolerance=1) {return std::abs(a-b)<=tolerance;}
void edge_projection() {
  const auto target=quad(-1,1,0,1.f/64,63.f/64,1.f/64,63.f/64);
  BakeOptions settings;settings.resolution=32;settings.ao_samples=0;settings.padding=0;
  settings.front_distance=settings.back_distance=.25f;
  BakeSource source;source.mesh=quad(-1,std::nextafter(1.f,0.f),.125f);
  source.materials.front().material.color={1,0,0,1};
  const auto rounded=bake_mesh(target,source,settings);
  check(rounded.projected==31*31&&rounded.missed==0&&rounded.rays==31*31&&
        pixel(rounded.maps.color,31,16)[0]==255,
        "A one-float-step boundary projects to the actual donor edge without adding a ray");

  auto layered=source;
  append(layered.mesh,quad(-1,1,-.125f));
  layered.materials.emplace_back().material.color={0,1,0,1};
  layered.triangle_materials={0,0,1,1};
  const auto exact=bake_mesh(target,layered,settings);
  check(pixel(exact.maps.color,16,16)[0]==255&&pixel(exact.maps.color,31,16)[1]==255,
        "An actual deeper ray hit has precedence over a nearby foreground edge");

  source.mesh=quad(-1,1-.00001f,.125f);
  check(rejects([&]{bake_mesh(target,source,settings);}),
        "Strict projection refuses a real lateral gap beyond float precision");
  settings.fail_on_miss=false;
  const auto gap=bake_mesh(target,source,settings);
  check(gap.projected==30*31&&gap.missed==31&&gap.coverage[16*32+31]==3,
        "Partial projection leaves the true missed edge uncovered");
  settings.fail_on_miss=true;
  for(float z:{std::nextafter(.25f,1.f),std::nextafter(-.25f,-1.f)}) {
    source.mesh=quad(-1,std::nextafter(1.f,0.f),z);
    check(rejects([&]{bake_mesh(target,source,settings);}),
          "Rounding recovery never extends a front or back projection depth");
  }
}

void ordered_raster_batches(){
  constexpr uint32_t resolution=263;
  const auto target=quad();
  auto overlapping=target;
  append(overlapping,quad(-1,1,0,0,1,1,0));
  BakeSource source;
  source.mesh=overlapping;
  source.materials.front().material.color={1,0,0,1};
  source.materials.emplace_back().material.color={0,0,1,1};
  source.triangle_materials={0,0,1,1};
  BakeOptions options;
  options.resolution=resolution;
  options.ao_samples=0;
  options.padding=0;
  options.fail_on_overlap=false;
  options.source_triangles={0,1,2,3};
  for(unsigned triangle=0;triangle<4;++triangle)
    options.source_corner_barycentrics.insert(options.source_corner_barycentrics.end(),{{1,0,0},{0,1,0},{0,0,1}});
  options.writable.resize(resolution*resolution);
  uint32_t writable=0;
  for(uint32_t i=0;i<options.writable.size();++i){
    options.writable[i]=i%7!=0;
    writable+=options.writable[i];
  }
  size_t encoded=0;
  uint32_t previous=0;
  bool ordered=true;
  options.encode_target_normal=[&](uint32_t triangle,std::array<float,3> bary,Vec3){
    const float x=triangle==0?bary[1]+bary[2]:bary[1];
    const float y=triangle==0?bary[2]:bary[1]+bary[2];
    const auto index=triangle*resolution*resolution+uint32_t(y*resolution)*resolution+uint32_t(x*resolution);
    ordered&=triangle<2&&(!encoded||previous<index);
    previous=index;
    ++encoded;
    return std::array<std::array<uint8_t,4>,2>{{{128,128,255,255},{128,128,255,255}}};
  };
  const auto result=bake_mesh(overlapping,source,options);
  check(ordered&&encoded==writable&&result.raster_samples==4ull*resolution*resolution&&
        result.projected==writable&&result.overlapping==writable&&result.rays==0,
        "Split rectangles keep exact face and pixel order, count mirrored overlaps and skip protected texels");
  for(uint32_t i=0;i<options.writable.size();++i){
    const auto color=pixel(result.maps.color,int(i%resolution),int(i/resolution));
    const auto expected=options.writable[i]?std::array<uint8_t,4>{255,0,0,255}:std::array<uint8_t,4>{};
    if(color!=expected||result.coverage[i]!=(options.writable[i]?1:4))
      throw std::runtime_error("Bake atlas ownership or writable mask changed between batches");
  }
  check(true,"The first owning material and every mask byte survive cross-batch overlap");
  options.max_raster_samples=result.raster_samples-1;
  check(rejects([&]{bake_mesh(overlapping,source,options);}),"A late raster budget refusal charges all rectangle candidates");
}
#ifdef DOME_TEST_BAKE_RAY_DEVICE
void ambient_device_traversal() {
  using namespace detail::bake_rays;
  Mesh surface;
  surface.positions={{0,0,.25f},{1,0,.25f},{0,1,.25f}};
  surface.normals.assign(3,{0,0,1});
  for(size_t i=0;i<4100;++i)surface.indices.insert(surface.indices.end(),{0,1,2});
  const SurfaceQuery reference(surface);
  const std::array<Vec3,1> pattern{{{0,0,1}}};
  const std::array<Frame,3> frames{{
    {{.75f,.75f,0},{1,0,0},{0,1,0},{0,0,1}},
    {{.5f,.5f,0},{1,0,0},{0,1,0},{0,0,1}},
    {{1.1f,.5f,0},{1,0,0},{0,1,0},{0,0,1}},
  }};
  auto query=detail::BakeRayQuery::upload(reference,pattern,.25f,true,{});
  std::array<uint16_t,3> counts{42,42,42};
  unsigned polls=0;
  check(rejects([&]{query->count_blocked(frames,counts,[&]{return ++polls==2;});})&&
        counts==std::array<uint16_t,3>{42,42,42},"Cancellation between device passes leaves all staged AO results untouched");
  polls=0;
  query->count_blocked(frames,counts,[&]{++polls;return false;});
  check(polls>3&&counts==std::array<uint16_t,3>{0,1,0},
        "Long watertight GPU traversals resume and accept an exact shared-edge segment endpoint");
  auto short_ray=detail::BakeRayQuery::upload(reference,pattern,std::nextafter(.25f,0.f),false,{});
  short_ray->count_blocked(frames,counts,{});
  check(counts==std::array<uint16_t,3>{0,0,0},"The device rejects a blocker one float step past the ray endpoint");

  const auto above=std::nextafter(.5f,1.f),shorter=std::nextafter(.25f,0.f);
  const auto make_ray=[](Vec3 origin,float distance){
    return detail::BakeRayQuery::projection_ray(origin,{0,0,1},distance,1e-6);
  };
  const std::array rays{make_ray({.5f,.5f,0},.25f),make_ray({.5f,above,0},.25f),
                        make_ray({.5f,above,0},shorter),make_ray({.75f,.75f,0},.25f)};
  std::array<ProjectionHit,rays.size()> hits;
  for(auto &hit:hits)hit.triangle=42;
  polls=0;
  check(rejects([&]{query->project(rays,hits,9000,[&]{return ++polls==2;});})&&
        std::ranges::all_of(hits,[](const auto &hit){return hit.triangle==42;}),
        "Cancelled projection continuation publishes no staged hits");
  const auto recovery=std::span(rays).subspan(1,1);
  check(rejects([&]{query->project(recovery,std::span(hits).first(1),4100,{});})&&hits[0].triangle==42,
        "The exact and edge passes share the original per-ray triangle-test budget");
  query->project(rays,hits,9000,{});
  check(hits[0].triangle!=hits[1].triangle&&hits[0].triangle!=UINT32_MAX&&hits[1].triangle==0&&
        hits[2].triangle==UINT32_MAX&&hits[3].triangle==UINT32_MAX,
        "Exact depth ties retain BVH order and the recovered edge tie uses original triangle ID");
  for(size_t i=0;i<rays.size();++i){
    const auto &ray=rays[i];
    const auto host=detail::BakeRayQuery::cast(reference,ray.origin,ray.direction,ray.maximum,1e-6,{});
    const auto device=detail::BakeRayQuery::resolve(reference,hits[i]);
    check(bool(host)==bool(device)&&(!host||(host->triangle==device->triangle&&
          host->barycentric==device->barycentric&&host->distance==device->distance&&
          length(host->position-device->position)==0&&length(host->normal-device->normal)==0)),
          "Device projection matches the original ray metric, barycentrics and hit reconstruction exactly");
  }
}

void oblique_device_precision(){
  // A ray that lands very close to a slanted triangle edge. Losing the
  // low bits of the wide reciprocal changes its third barycentric value.
  Mesh mesh;
  mesh.positions={{-0x1.ep+3f,-0x1.ep+3f,0x1.04039cp-4f},
                  {-0x1.cb22e6p+3f,-0x1.e09526p+3f,0x1.e6106cp-4f},
                  {-0x1.e1ff64p+3f,-0x1.cb4b48p+3f,0x1.64a5ap-6f}};
  mesh.indices={0,1,2};
  mesh.normals.assign(3,{0,0,1});
  const SurfaceQuery reference(mesh);
  const Vec3 origin{-0x1.d4b06cp+3f,-0x1.dc3d94p+3f,0x1.d87c04p-2f};
  const Vec3 direction{-0x1.1ecb8cp-4f,-0x1.4a6076p-2f,-0x1.e34aep-1f};
  constexpr float maximum=0x1.91c582p-2f;
  const auto host=detail::BakeRayQuery::cast(reference,origin,direction,maximum,0,{});
  const auto ray=detail::BakeRayQuery::projection_ray(origin,direction,maximum,0);
  detail::bake_rays::ProjectionHit hit;
  auto query=detail::BakeRayQuery::upload(reference,{},0,true,{});
  query->project(std::span(&ray,1),std::span(&hit,1),1,{});
  const auto projected=detail::BakeRayQuery::resolve(reference,hit);
  check(host&&projected&&projected->barycentric==host->barycentric&&projected->distance==host->distance,
        "Oblique edge-adjacent projection keeps every bit of the wide reciprocal and resulting barycentrics");
}

void resident_cage_projection(){
  namespace raster=detail::bake_raster;
  namespace rays=detail::bake_rays;
  constexpr uint32_t resolution=37;
  std::array<float,resolution> axis{};
  for(uint32_t i=0;i<resolution;++i)axis[i]=(i+.5f)/resolution;
  const raster::Span span{std::bit_cast<uint64_t>(1.),{{0,0},{1,0},{0,1}},0,0,0,resolution,0,1};
  const SurfaceQuery reference(quad(-1,1,.047f));
  auto query=detail::BakeRayQuery::upload(reference,{},0,true,{});
  const std::array<Vec3,3> points{{{-.4f,-.4f,-.01f},{.4f,-.4f,.01f},{-.4f,.4f,0}}};
  const std::array<Vec3,3> normals{{{-.12f,.07f,.9f},{.18f,-.1f,1.f},{.13f,.15f,1.1f}}};
  for(bool supplied:{false,true})for(bool explicit_cage:{false,true}){
    auto target=query->create_projection_target(axis,{},raster::ProjectionSettings{.11f,.09f,supplied,explicit_cage});
    raster::ProjectionFace face{};
    face.radius_bits=std::bit_cast<uint64_t>(1e-6);
    face.geometric_normal=normalized(cross(points[1]-points[0],points[2]-points[0]));
    for(size_t i=0;i<3;++i){
      face.positions[i]=points[i];
      face.normals[i]=supplied?normals[i]:normalized(normals[i]);
      face.cage[i]=points[i]+Vec3{-.02f+.01f*float(i),.03f,.09f+.005f*float(i)};
    }
    const auto result=target->rasterize_and_project(std::span(&span,1),std::span(&face,1),
                                                    resolution*resolution,2000,2000,{});
    std::vector<rays::ProjectionRay> packets;
    for(const auto &sample:result.samples){
      Vec3 position,normal,cage;
      for(size_t i=0;i<3;++i){
        position+=face.positions[i]*sample.barycentric[i];
        normal+=face.normals[i]*sample.barycentric[i];
        cage+=face.cage[i]*sample.barycentric[i];
      }
      normal=supplied&&length(normal)<=1e-8f?face.geometric_normal:normalized(normal);
      const auto offset=explicit_cage?cage-position:normal*.11f;
      const float front=length(offset);
      packets.push_back(detail::BakeRayQuery::projection_ray(position+offset,
          (front>1e-8f?offset/front:normal)*-1,front+.09f,1e-6));
    }
    std::vector<rays::ProjectionHit> hits(packets.size());
    query->project(packets,hits,2000,{});
    bool same=hits.size()>650&&result.hits.size()==hits.size()&&!result.overlaps;
    for(size_t i=0;i<hits.size();++i){
      const auto &a=hits[i],&b=result.hits[i];
      same&=a.triangle==b.triangle&&a.triangle!=UINT32_MAX&&a.distance==b.distance&&
          a.position.x==b.position.x&&a.position.y==b.position.y&&a.position.z==b.position.z&&
          std::ranges::equal(a.barycentric,b.barycentric);
    }
    check(same,"Resident cage rays match host-prepared hits for interpolated and supplied normals");
  }
  auto limited=query->create_projection_target(axis,{},raster::ProjectionSettings{.11f,.09f,false,false});
  raster::ProjectionFace face{};
  check(rejects([&]{limited->rasterize_and_project(std::span(&span,1),std::span(&face,1),
        resolution*resolution,1,2000,{});}),"The resident pipeline refuses an insufficient ray budget before tracing");
}

void original_material_device_sampling(){
  namespace device=detail::bake_material;
  auto mesh=quad();
  auto image=std::make_shared<Image>(Image{11,7,std::vector<uint8_t>(11*7*4)});
  for(size_t i=0;i<image->rgba.size();++i)image->rgba[i]=uint8_t((i*71+i/9*31)%256);
  auto maps=std::make_shared<PaintSourceMaps>();
  maps->uv1=std::make_shared<const std::vector<std::array<float,2>>>(
      std::initializer_list<std::array<float,2>>{{-.2f,2.5f},{-1.2f,.1f},{2.8f,-.9f},{-.4f,-.3f}});
  maps->extra_uv_sets[2]=std::make_shared<const std::vector<std::array<float,2>>>(
      std::initializer_list<std::array<float,2>>{{.4f,-.1f},{2.3f,2.1f},{-4.8f,.9f},{2.4f,.3f}});
  maps->color.image=maps->metal_rough.image=maps->emission.image=maps->occlusion.image=maps->normal.image=image;
  maps->color.texcoord=1;maps->color.wrap_s=33648;maps->color.uv_transform={.713f,-.231f,.147f,.866f,-.81f,.37f};
  maps->metal_rough.texcoord=2;maps->metal_rough.wrap_t=33071;
  maps->emission.texcoord=1;maps->emission.mag_filter=9728;maps->emission.wrap_t=33648;
  maps->occlusion.texcoord=2;maps->occlusion_strength=.47f;
  maps->occlusion.uv_transform={-.926f,.211f,.08f,.773f,-.5f,1.34f};
  maps->normal.texcoord=2;maps->normal.wrap_s=33648;maps->normal.wrap_t=33071;
  maps->normal.uv_transform={-.43f,.81f,.23f,.67f,-.19f,1.46f};
  maps->validate_surface(mesh);
  BakeMaterial material;
  material.material.color={.31f,.67f,.83f,.72f};
  material.material.emissive={.53f,.11f,.41f};
  material.material.roughness=.61f;material.material.metallic=.29f;
  auto texture=std::make_shared<TextureMaps>();
  texture->source_only=true;texture->source_maps=maps;
  material.maps=texture;
  const device::MaterialInput input(material,false,nullptr,true);
  std::vector<device::Coordinates> coordinates;
  std::vector<std::array<float,3>> weights;
  for(unsigned y=0;y<37;++y)for(unsigned x=0;x<=y;++x){
    const float u=(x+.3f)/37.f,v=(y-x+.2f)/37.f;
    weights.push_back({u,v,1-u-v});
    coordinates.push_back(input.coordinates(mesh,0,weights.back(),{},{}));
  }
  auto sampler=device::upload(input.images,input.parameters);
  std::vector<device::Values> values(coordinates.size());
  sampler->read(coordinates,values,{});
  for(size_t i=0;i<weights.size();++i){
    const auto color=sample_surface_texture(mesh,*maps,maps->color,0,weights[i],true);
    const auto orm=sample_surface_texture(mesh,*maps,maps->metal_rough,0,weights[i],false);
    const auto emission=sample_surface_texture(mesh,*maps,maps->emission,0,weights[i],true);
    const auto ao=sample_surface_texture(mesh,*maps,maps->occlusion,0,weights[i],false);
    const auto normal=sample_surface_texture(mesh,*maps,maps->normal,0,weights[i],false);
    const auto &actual=values[i];
    bool exact=actual.roughness==orm[1]*material.material.roughness&&actual.metallic==orm[2]*material.material.metallic&&
        actual.occlusion==1-maps->occlusion_strength*(1-ao[0])&&actual.normal[3]==normal[3];
    for(unsigned c=0;c<4;++c)exact&=actual.color[c]==color[c]*material.material.color[c];
    for(unsigned c=0;c<3;++c){
      exact&=actual.emission[c]==emission[c]*material.material.emissive[c];
      exact&=actual.normal[c]==normal[c];
    }
    if(!exact)throw std::runtime_error("Device material lost exact extra-UV, affine, sampler or factor semantics");
  }
  check(true,"GPU original normal and material samples exactly preserve UV1 and UV2, affine filtering and separate occlusion");
}

std::array<float,4> covered_normal_oracle(const detail::PaintOverlayPacket &packet,float u,float v){
  std::array<float,4> result{};
  if(!packet.baked())return result;
  const auto &baked=*packet.baked();
  const int res=int(packet.resolution());
  const float x=std::clamp(u,0.f,1.f)*res-.5f,y=std::clamp(v,0.f,1.f)*res-.5f;
  const int ix=int(std::floor(x)),iy=int(std::floor(y));
  const float fx=x-ix,fy=y-iy;
  for(int dy=0;dy<2;++dy)for(int dx=0;dx<2;++dx){
    const auto texel=size_t(std::clamp(iy+dy,0,res-1))*res+std::clamp(ix+dx,0,res-1);
    const auto owner=packet.padding()->at(texel);
    if(owner<0)continue;
    const float weight=(dx?fx:1-fx)*(dy?fy:1-fy)*(baked.coverage.at(owner)/255.f);
    const auto *normal=baked.maps.normal.rgba.data()+4*size_t(owner);
    for(unsigned c=0;c<3;++c)result[c]+=(normal[c]/255.f*2-1)*weight;
    result[3]+=weight;
  }
  return result;
}

void sparse_material_device_sampling(){
  namespace device=detail::bake_material;
  constexpr uint32_t resolution=64;
  const auto mesh=quad(-1,1,0,.17f,.83f,.18f,.82f);
  auto paint=std::make_shared<PaintedMesh>(mesh,resolution);
  const auto stamp=[&](PaintSettings settings,Vec3 center,float radius){
    paint->begin_stroke();
    paint->stamp(Brush{center,center,{0,0,1},radius,.73f},settings);
    check(paint->end_stroke(),"Bake fixture paints an editable sparse channel");
  };
  PaintSettings settings;
  settings.color={.19f,.67f,.02f,.43f};
  stamp(settings,{-.32f,.09f,0},.82f);
  settings.channel=PaintChannel::Metallic;
  stamp(settings,{-.41f,.16f,0},.41f);
  settings.channel=PaintChannel::Mask;settings.mask=.24f;
  stamp(settings,{-.23f,.05f,0},.53f);
  paint->layer_properties("Masked color and metal",true,.67f);
  paint->add_layer("Roughness and emission");
  settings.channel=PaintChannel::Roughness;settings.roughness=.19f;
  stamp(settings,{.07f,.15f,0},.58f);
  settings.channel=PaintChannel::Emission;settings.emission={.19f,.71f,.01f};
  stamp(settings,{-.15f,.11f,0},1.1f);
  paint->layer_properties("Roughness and emission",true,.82f);
  paint->add_layer("Hidden");
  settings.channel=PaintChannel::Color;settings.color={1,0,1,1};
  stamp(settings,{0,0,0},1.f);
  paint->layer_properties("Hidden",false,1);

  BakedTextures baked;
  const auto image=[](std::array<uint8_t,4> value){
    Image output{resolution,resolution,std::vector<uint8_t>(resolution*resolution*4)};
    for(size_t at=0;at<output.rgba.size();at+=4)std::ranges::copy(value,output.rgba.begin()+at);
    return output;
  };
  baked.maps.color=image({40,90,210,170});
  baked.maps.metal_rough=image({190,40,230,255});
  baked.maps.emission=image({20,40,70,255});
  baked.maps.normal=image({128,128,230,255});
  baked.coverage.resize(resolution*resolution);
  for(size_t at=0;at<baked.coverage.size();++at){
    baked.coverage[at]=uint8_t(at*13+at/resolution*19);
    baked.maps.normal.rgba[at*4]=uint8_t(at*17);
    baked.maps.normal.rgba[at*4+1]=uint8_t(at*19+at/resolution*11);
  }

  const auto unbaked=paint->snapshot();
  for(unsigned variant=0;variant<4;++variant){
    paint=unbaked->snapshot();
    if(variant==2){
      baked.maps.emission={};
      baked.emission_field=BakedEmissionField::generate_compact(resolution,[](size_t i){
        const float coverage=i%7==0?0.f:1.f;
        return BakedEmissionSample{{coverage*.61f,coverage*float(i%31)/31.f,coverage*.18f},{},coverage};
      });
    }
    if(variant==3){
      auto scene=source_color_fixture::model(true);
      const auto &object=scene.active();
      paint=object.painted;
      settings.color={.17f,.67f,.93f,.71f};
      stamp(settings,{0,0,0},.58f);
      paint=prepare_source_colors({paint,object.appearance,object.material_bindings,
                                   object.deformation,object.material}).paint;
      baked.maps.pbr_color=image({170,130,20,190});
      baked.color_interpretation=BakedColorInterpretation::ResolvedPbr;
    }
    if(variant)paint->install_bake(baked);
    const auto original=paint->encode();
    const auto overlay=detail::PaintOverlayFactory::capture(*paint);
    BakeMaterial material;
    material.material.color={.31f,.67f,.83f,.72f};
    material.material.emissive={.53f,.11f,.41f};
    material.material.roughness=.61f;material.material.metallic=.29f;
    device::MaterialInput input(material,true,nullptr);
    input.parameters.separate_pbr=true;
    input.parameters.include_overlay_emission=true;
    input.parameters.include_baked_normal=bool(variant);
    std::vector<device::Coordinates> coordinates;
    for(unsigned i=0;i<889;++i){
      const std::array uv{float(i*73%883)/713-.17f,float(i*131%887)/727-.11f};
      const std::array canonical_uv{1.f-uv[1],uv[0]};
      coordinates.push_back(input.coordinates(mesh,0,{1,0,0},uv,canonical_uv));
    }
    auto sampler=device::upload(input.images,input.parameters,overlay.get());
    std::vector<device::Values> values(coordinates.size());
    sampler->read(coordinates,values,{});
    const auto composite=[](float base,float coverage,float added){return base*(1-coverage)+added;};
    for(size_t i=0;i<values.size();++i){
      const auto [u,v]=coordinates[i].paint_uv;
      const auto common=detail::sample_overlay_values(*overlay,u,v,false);
      const auto pbr=detail::sample_overlay_values(*overlay,u,v,true);
      const auto normal=covered_normal_oracle(*overlay,coordinates[i].high_uv[0],coordinates[i].high_uv[1]);
      const auto &actual=values[i];
      bool exact=actual.roughness==composite(material.material.roughness,common[3][2],common[1][1])&&
          actual.metallic==composite(material.material.metallic,common[3][1],common[1][2])&&
          actual.occlusion==composite(1.f,common[1][3],common[1][0]);
      for(unsigned c=0;c<4;++c){
        exact&=actual.color[c]==composite(material.material.color[c],common[3][0],common[0][c]);
        exact&=actual.pbr[c]==composite(material.material.color[c],pbr[3][0],pbr[0][c]);
        exact&=actual.baked_normal[c]==normal[c];
      }
      for(unsigned c=0;c<3;++c)
        exact&=actual.emission[c]==composite(material.material.emissive[c],common[3][3],common[2][c]);
      if(!exact)throw std::runtime_error("Device sparse material differs from the native point oracle at variant "+
          std::to_string(variant)+", sample "+std::to_string(i));
    }
    check(rejects([&]{sampler->read(coordinates,values,[]{return true;});})&&paint->encode()==original,
          "Cancellation preserves exact native paint owners after GPU sampling");
  }
  check(true,"GPU sparse material and canonical normal match independent filtering through masks, padding and FLOAT coverage");
}

void bake_radiance_device_sampling(){
  namespace raster=detail;
  using Device=raster::EmissionRasterDevice;
  constexpr uint32_t resolution=64;
  auto paint=std::make_shared<PaintedMesh>(quad(-1,1,0,.17f,.83f,.18f,.82f),resolution);
  BakedTextures baked;
  baked.maps.color=baked.maps.metal_rough={resolution,resolution,std::vector<uint8_t>(resolution*resolution*4,255)};
  baked.coverage.assign(resolution*resolution,197);
  baked.emission_field=BakedEmissionField::generate_compact(resolution,[](size_t at){
    const float alpha=float(at%53+13)/127.f;
    return BakedEmissionSample{{alpha*.32f,alpha*.86f,alpha*.45f},
        {alpha*float(at%29)*18700.f,alpha*1e-24f,alpha*float(at%19)},alpha};
  });
  paint->install_bake(baked);
  const Brush dab{{-.27f,.13f,0},{-.27f,.13f,0},{0,0,1},.75f,.78f};
  PaintSettings settings;
  settings.channel=PaintChannel::Emission;settings.emission={.21f,.67f,.78f};
  paint->begin_stroke();paint->stamp(dab,settings);check(paint->end_stroke(),"HDR fixture has a legacy relative stroke");
  settings.channel=PaintChannel::Mask;settings.mask=.23f;
  paint->begin_stroke();paint->stamp(dab,settings);check(paint->end_stroke(),"HDR fixture has a fractional sparse mask");
  paint->add_layer("Absolute emission");
  paint->layer_properties("Absolute emission",true,.47f);
  settings.channel=PaintChannel::Emission;settings.emission_units=PaintEmissionUnits::Physical;
  settings.emission={57.f,1e-20f,2.1f};
  paint->begin_stroke();paint->stamp(dab,settings);check(paint->end_stroke(),"HDR fixture has an absolute FLOAT stroke");
  const auto packet=raster::PaintOverlayFactory::capture(*paint);
  const auto original=paint->encode();
  PaintSourceTexture image;
  image.image=std::make_shared<const Image>(Image{3,2,{255,17,15,255,0,178,3,255,68,75,211,255,
                                                       9,250,80,255,157,50,208,255,19,84,38,255}});
  image.wrap_s=33648;image.wrap_t=33071;image.uv_transform={.78f,-.43f,.38f,.678f,-.2f,1.7f};
  std::vector<Device::BakeRadianceSample> samples;
  for(unsigned i=0;i<1009;++i)samples.push_back({{float(i*83%1031)/851-.15f,float(i*191%1033)/863-.07f},
                                                     {float(i*31%1033)/613-.73f,float(i*7%1031)/737-.37f}});
  const std::array<float,3> background{4.77f,9.63f,3.54e-12f};
  constexpr float strength=3.71f;
  for(const bool textured:{false,true}){
    auto device=raster::create_bake_radiance_device(packet.get(),textured?&image:nullptr,background,strength,samples.size(),{});
    std::vector<Device::Radiance> actual(samples.size());
    device->read_bake_radiance(samples,actual,{});
    for(size_t i=0;i<samples.size();++i){
      const auto &sample=samples[i];
      const auto overlay=raster::sample_overlay_values(*packet,sample.paint_uv[0],sample.paint_uv[1],false);
      const auto texel=textured?sample_source_texture(image,sample.image_uv[0],sample.image_uv[1],true):
          std::array<float,4>{1,1,1,1};
      const BakedEmissionSample sample_value{{overlay[2][0],overlay[2][1],overlay[2][2]},
          {overlay[4][0],overlay[4][1],overlay[4][2]},overlay[3][3]};
      auto original_light=background;
      for(unsigned c=0;c<3;++c)original_light[c]=float(double(background[c])*texel[c]);
      if(sample_value.resolve(original_light,strength)!=actual[i])
        throw std::runtime_error("GPU Bake radiance differs from native ordered arithmetic at "+std::to_string(i));
    }
    check(rejects([&]{device->read_bake_radiance(samples,actual,[]{return true;});})&&paint->encode()==original,
          "Cancelled HDR sampling preserves the captured sparse source");
  }
  constexpr float maximum=std::numeric_limits<float>::max();
  auto edge=std::make_shared<PaintedMesh>(quad(),resolution);
  baked.emission_field=BakedEmissionField::generate_compact(resolution,[](size_t){
    return BakedEmissionSample{{1,0,0},{maximum,0,std::numeric_limits<float>::denorm_min()},1};
  });
  edge->install_bake(std::move(baked));
  const auto edge_packet=raster::PaintOverlayFactory::capture(*edge);
  const std::array<Device::BakeRadianceSample,1> point{{{{.43f,.64f},{}}}};
  std::array<Device::Radiance,1> result{};
  auto bound=raster::create_bake_radiance_device(edge_packet.get(),nullptr,{-0.f,0.f,0.f},0,1,{});
  bound->read_bake_radiance(point,result,{});
  check(result[0]==Device::Radiance{maximum,0,std::numeric_limits<float>::denorm_min()},
        "GPU radiance preserves the FLOAT endpoints above and below normal intensity");
  auto overflow=raster::create_bake_radiance_device(edge_packet.get(),nullptr,{},maximum,1,{});
  bool rejected=false;
  try{overflow->read_bake_radiance(point,result,{});}catch(const std::overflow_error &){rejected=true;}
  check(rejected,"GPU recognizes overflow during the ordered HDR resolve");
  check(true,"GPU Bake radiance exactly matches the native masked mixed HDR oracle with and without an original image");
}

void sampler_cache_pressure() {
  struct Sampler {
    bool &alive;
    explicit Sampler(bool &flag):alive(flag){alive=true;}
    ~Sampler(){alive=false;}
  };
  bool alive[4]{};
  detail::BakeSamplerCache<Sampler> cache(8);
  const auto no_other_cache=[] {return false;};
  const auto load=[&](unsigned material,size_t bytes){
    return &cache.get(material,[&](const auto &reserve){
      reserve(bytes/2);
      reserve(bytes-bytes/2);
      return std::make_unique<Sampler>(alive[material]);
    },no_other_cache);
  };
  const auto *first=load(0,4);
  load(1,4);
  const bool reused=load(0,4)==first;
  load(2,4);
  check(reused&&alive[0]&&!alive[1]&&alive[2],"Bake reuses the recent sampler and evicts the least recent under its byte limit");
  load(3,9);
  check(!alive[0]&&!alive[2]&&alive[3]&&!cache.trim_idle(),"An oversized Bake source gets the original single-sampler workspace");
  load(0,4);
  unsigned retries=0;
  cache.get(1,[&](const auto &reserve){
    reserve(4);
    if(++retries==1)throw std::bad_alloc();
    return std::make_unique<Sampler>(alive[1]);
  },no_other_cache);
  check(retries==2&&!alive[0]&&alive[1]&&!alive[3],"Device allocation pressure discards cached samplers and retries the same source once");
  unsigned cancelled=0;
  check(rejects([&]{cache.get(2,[&](const auto &) -> std::unique_ptr<Sampler>{
    ++cancelled;
    throw std::runtime_error("Mesh baking cancelled");
  },no_other_cache);})&&cancelled==1&&alive[1],"Cancellation leaves the admitted sampler alone and never retries");
}
#endif
} // namespace
int main()try {
  edge_projection();
  ordered_raster_batches();
#ifdef DOME_TEST_BAKE_RAY_DEVICE
  ambient_device_traversal();
  oblique_device_precision();
  resident_cage_projection();
  original_material_device_sampling();
  sparse_material_device_sampling();
  bake_radiance_device_sampling();
  sampler_cache_pressure();
#endif
  auto low=quad();BakeSource source;source.mesh=quad();
  BakeOptions options;options.resolution=32;options.padding=0;options.ao_samples=0;
  auto result=bake_mesh(low,source,options);
  auto normal=pixel(result.normal,16,16),orm=pixel(result.maps.metal_rough,16,16);
  check(result.projected==1024&&!result.missed&&!result.overlapping&&near(normal[0],128)&&near(normal[1],128)&&normal[2]==255,
        "UV edge ownership covers a planar quad once with a neutral tangent normal");
  check(orm[0]==255&&near(orm[1],204)&&orm[2]==0&&pixel(result.ambient_occlusion,16,16)[0]==255,
        "Default material output uses linear R=AO G=roughness B=metal conventions");
  source.mesh.normals.assign(4,{0,.6f,.8f});result=bake_mesh(low,source,options);
  check(near(pixel(result.normal,16,16)[1],204)&&near(pixel(result.normal,16,16)[2],230),
        "High interpolated normals project into the low UV tangent frame");
  options.low_tangents.assign(4,{0,1,0,1});auto explicit_tangent=bake_mesh(low,source,options);
  check(near(pixel(explicit_tangent.normal,16,16)[0],204)&&near(pixel(explicit_tangent.normal,16,16)[1],128),
        "Supplied consumer tangent frames preserve imported/MikkTSpace normal-map conventions");options.low_tangents.clear();
  auto mirrored=quad(-1,1,0,0,1,1,0);auto flipped=bake_mesh(mirrored,source,options);
  check(near(pixel(flipped.normal,16,16)[1],51),"Mirrored UV handedness reverses the tangent-space green component");
  options.normal_y_down=true;auto directx=bake_mesh(mirrored,source,options);options.normal_y_down=false;
  check(near(pixel(directx.normal,16,16)[1],204),"Explicit DirectX Y-down output changes only the normal convention");

  source.mesh=quad(-1,1,.02f);options.cage_positions=low.positions;
  for(auto &p:options.cage_positions)p.z+=.04f;
  check(bake_mesh(low,source,options).projected==1024,"A bounded explicit cage projects onto displaced high detail");
  options.cage_positions[0].z=.1f;
  check(rejects([&]{bake_mesh(low,source,options);}),"Cages outside the front ray budget are rejected");
  options.cage_positions.clear();source.mesh=quad(-1,1,.2f);
  check(rejects([&]{bake_mesh(low,source,options);}),"High surfaces beyond the projection cage are never accepted by a nearest fallback");
  source.mesh=quad();source.mesh.indices={0,2,1,0,3,2};
  check(rejects([&]{bake_mesh(low,source,options);}),"Backface projection is rejected by geometric winding even with misleading vertex normals");
  options.allow_backfaces=true;check(bake_mesh(low,source,options).projected==1024,"Backface transfer is an explicit caller choice");options.allow_backfaces=false;

  source.mesh=quad();auto maps=std::make_shared<TextureMaps>();
  maps->color={2,1,{0,0,0,255,255,255,255,64}};
  maps->metal_rough={2,1,{128,64,32,255,128,192,224,255}};
  maps->emission=constant({128,128,128,255});source.materials[0].maps=maps;
  source.materials[0].material.color={.5f,.5f,.5f,.5f};
  source.materials[0].material.roughness=.5f;source.materials[0].material.metallic=.25f;
  source.materials[0].material.emissive={.5f,.5f,.5f};
  result=bake_mesh(low,source,options);auto color=pixel(result.maps.color,15,16);orm=pixel(result.maps.metal_rough,15,16);
  check(color[0]>175&&color[0]<190&&near(color[3],165)&&orm[0]==128&&near(orm[1],124)&&near(orm[2],122)&&result.maps.transparent,
        "Composited source maps retain alpha and bilinear interpolation occurs in linear RGB");
  source.colors.assign(4,{.5f,1,1,.5f});auto vertex_color=bake_mesh(low,source,options);
  check(pixel(vertex_color.maps.color,15,16)[0]>125&&pixel(vertex_color.maps.color,15,16)[0]<140&&near(pixel(vertex_color.maps.color,15,16)[3],83),
        "Linear imported vertex colors and alpha transfer without an extra sRGB decode");source.colors.clear();
  source.materials[0].maps_are_composited=false;auto factored=bake_mesh(low,source,options);
  check(pixel(factored.maps.color,15,16)[0]>125&&pixel(factored.maps.color,15,16)[0]<140&&near(pixel(factored.maps.color,15,16)[3],83)&&
        near(pixel(factored.maps.metal_rough,15,16)[1],62)&&near(pixel(factored.maps.metal_rough,15,16)[2],31)&&pixel(factored.maps.emission,15,16)[0]<100,
        "Imported material factors apply exactly once to linear channels and sRGB emission");
  source.materials[0].normal_map=std::make_shared<Image>(constant({128,204,230}));
  auto detailed=bake_mesh(low,source,options);
  source.tangents.assign(4,{0,1,0,1});source.materials[0].normal_map=std::make_shared<Image>(constant({204,128,230}));
  auto source_tangent=bake_mesh(low,source,options);
  check(pixel(source_tangent.normal,16,16)[1]>200&&near(pixel(source_tangent.normal,16,16)[0],128),
        "Imported high tangents orient source normal maps before low tangent projection");
  check(pixel(detailed.normal,16,16)[1]>200,"Source tangent normal textures are transferred through the high tangent frame");

  source=BakeSource{};source.mesh=quad();options.ao_samples=64;options.ao_distance=.2f;
  auto open=bake_mesh(low,source,options);
  check(pixel(open.ambient_occlusion,16,16)[0]==255,"A planar open surface does not self-occlude under biased hemisphere rays");
  auto ceiling=quad(-5,5,.08f);for(auto &p:ceiling.positions)p.y*=5;ceiling.indices={0,2,1,0,3,2};ceiling.normals.assign(4,{0,0,-1});append(source.mesh,ceiling);
  auto shadow=bake_mesh(low,source,options);
  check(near(pixel(shadow.ambient_occlusion,16,16)[0],40,8)&&shadow.rays==uint64_t(1024)*65,
        "Cosine hemisphere AO detects a nearby high surface without altering projection bounds");
  auto budget=options;
  budget.max_rays=shadow.rays-1;
  check(rejects([&]{bake_mesh(low,source,budget);}),"A late mixed projection and ambient budget failure rejects the complete Bake");
  options.ao_distance=.04f;auto distant=bake_mesh(low,source,options);
  check(pixel(distant.ambient_occlusion,16,16)[0]==255,"AO rays obey their explicit maximum distance");

  options.ao_samples=0;source.mesh=quad();auto island=quad(-1,1,0,.25f,.75f,.25f,.75f);options.padding=2;
  auto padded=bake_mesh(island,source,options);
  check(padded.projected==256&&padded.padded==144&&padded.coverage[16*32+6]==2&&padded.coverage[0]==0&&
        pixel(padded.normal,6,16)==pixel(padded.normal,8,16)&&pixel(padded.maps.color,0,0)[3]==0,
        "Seam padding copies nearest valid channels only within the requested texel distance");
  options.writable.assign(32*32,1);for(int y=0;y<32;++y)options.writable[y*32+6]=0;
  auto masked=bake_mesh(island,source,options);
  check(masked.coverage[16*32+6]==4&&pixel(masked.maps.color,6,16)[3]==0&&masked.coverage[16*32+7]==2,
        "Writable masks prevent padding from touching protected texture regions");options.writable.clear();
  source.mesh=quad(-1,0);options.fail_on_miss=false;auto missed=bake_mesh(low,source,options);
  check(missed.projected==512&&missed.missed==512&&!missed.padded&&missed.coverage[16*32+24]==3&&pixel(missed.maps.color,24,16)[3]==0,
        "Padding cannot conceal missed projection texels inside a UV island");
  options.fail_on_miss=true;check(rejects([&]{bake_mesh(low,source,options);}),"Incomplete projection fails before any caller can publish partial texture data");

  auto islands=quad(-1,0,0,.1f,.4f,.2f,.8f);append(islands,quad(0,1,0,.6f,.9f,.2f,.8f));
  source=BakeSource{};source.mesh=quad(-1,0);append(source.mesh,quad(0,1));
  source.materials.resize(2);source.materials[0].material.color={1,0,0,1};source.materials[1].material.color={0,0,1,1};source.triangle_materials={0,0,1,1};
  options.padding=4;auto split=bake_mesh(islands,source,options);bool pure=true;
  for(size_t i=0;i<split.coverage.size();++i)if(split.coverage[i]){auto p=split.maps.color.rgba.data()+i*4;pure&=(p[0]==255&&p[2]==0)||(p[0]==0&&p[2]==255);}
  check(pure&&split.padded>0,"Padding across adjacent material islands never blends unrelated seam colors");
  auto overlap=low;append(overlap,low);source=BakeSource{};source.mesh=quad();
  check(rejects([&]{bake_mesh(overlap,source,options);}),"Ambiguous overlapping low UV islands fail explicitly");
  options.fail_on_overlap=false;check(bake_mesh(overlap,source,options).overlapping==1024,"Preview overlap policy reports every conflicting texel");options.fail_on_overlap=true;

  options.padding=0;options.max_rays=10;
  check(rejects([&]{bake_mesh(low,source,options);}),"Bake work cannot exceed the configured ray budget");
  options.max_rays=64'000'000;options.max_raster_samples=10;
  check(rejects([&]{bake_mesh(low,source,options);}),"Even overlapping/empty UV raster work is bounded before ray sampling");options.max_raster_samples=128'000'000;options.max_texels=100;
  check(rejects([&]{bake_mesh(low,source,options);}),"Texture allocation is bounded before raster work");options.max_texels=4'194'304;
  int checks=0;options.cancelled=[&]{return ++checks>10;};options.ao_samples=64;
  check(rejects([&]{bake_mesh(low,source,options);})&&low.indices==std::vector<uint32_t>({0,1,2,0,2,3})&&low.positions[0].x==-1,
        "Cancellation interrupts native projection/AO without modifying input meshes");
  options.cancelled={};options.ao_samples=0;
  auto bad=low;bad.uv[0][0]=2;
  check(rejects([&]{bake_mesh(bad,source,options);}),"Low UV coordinates outside the atlas are rejected");
  bad=low;bad.uv[1]=bad.uv[0];
  check(rejects([&]{bake_mesh(bad,source,options);}),"Degenerate UV tangent frames are reported instead of generating NaNs");
  source.triangle_materials={3,3};check(rejects([&]{bake_mesh(low,source,options);}),"Material assignment errors are validated before sampling");
  source=BakeSource{};source.mesh=quad();auto invalid_maps=std::make_shared<TextureMaps>();
  invalid_maps->color=constant({255,255,255,255});invalid_maps->metal_rough=constant({255,128,128,255});invalid_maps->emission=constant({0,0,0,255});
  invalid_maps->color.rgba.pop_back();source.materials[0].maps=invalid_maps;
  check(rejects([&]{bake_mesh(low,source,options);}),"Malformed material image buffers fail before any texture sampling");
  invalid_maps->color=constant({255,255,255,255});source.mesh.uv.clear();
  check(rejects([&]{bake_mesh(low,source,options);}),"Textured high sources without UV coordinates are rejected explicitly");
  source=BakeSource{};source.mesh=quad();options.low_tangents.assign(4,{1,0,0,.5f});
  check(rejects([&]{bake_mesh(low,source,options);}),"Invalid tangent handedness cannot generate ambiguous normal maps");options.low_tangents.clear();
  options.writable.assign(10,1);check(rejects([&]{bake_mesh(low,source,options);}),"Writable masks must match exact output texel identities");options.writable.clear();
  auto tiny_uv=low;Mesh tiny;tiny.positions={{-.1f,-.1f,0},{.1f,-.1f,0},{0,.1f,0}};tiny.normals.assign(3,{0,0,1});tiny.indices={0,1,2};tiny.uv={{{0,0}},{{1e-7f,0}},{{0,1e-7f}}};append(tiny_uv,tiny);
  check(bake_mesh(tiny_uv,source,options).projected==1024,
        "Tiny nondegenerate subpixel UV triangles do not invalidate otherwise writable atlas data");

  Volume sculpt;sculpt.begin_stroke();Brush clay;clay.radius=.18f;sculpt.stamp(clay);sculpt.end_stroke();
  source=BakeSource{};source.mesh=surface_mesh(sculpt);
  auto dense_uv=source.mesh;unwrap(dense_uv,64,2);double minimum_uv=1;size_t collapsed_uv=0;
  for(size_t t=0;t<dense_uv.indices.size();t+=3){auto a=dense_uv.uv[dense_uv.indices[t]],b=dense_uv.uv[dense_uv.indices[t+1]],c=dense_uv.uv[dense_uv.indices[t+2]];double area=std::abs(double(b[0]-a[0])*(c[1]-a[1])-double(b[1]-a[1])*(c[0]-a[0]));minimum_uv=std::min(minimum_uv,area);collapsed_uv+=area==0;}
  std::cout<<"Dense sculpt atlas diagnostic: min UV determinant="<<minimum_uv<<", exactly collapsed="<<collapsed_uv<<"\n";
  auto sculpt_low=box(.09f);unwrap(sculpt_low,64,2);
  options.resolution=64;options.padding=2;options.front_distance=.15f;
  if(collapsed_uv)check(rejects([&]{bake_mesh(dense_uv,source,options);}),"Actual collapsed atlas triangles are reported rather than silently skipped");
  auto sculpt_bake=bake_mesh(sculpt_low,source,options);
  check(sculpt_bake.projected>1000&&!sculpt_bake.missed&&!sculpt_bake.overlapping&&sculpt_bake.normal.png().size()>100,
        "A native sculpted closed surface bakes through xatlas charts into valid normal PNG data");
  std::cout<<"Native mesh baking tests complete\n";
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
