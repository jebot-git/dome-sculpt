// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/scene.hpp"
#include "dome/source-maps.hpp"
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <iostream>
#include <new>

namespace {
struct Allocations {
  bool enabled=false;
  size_t largest=0,bytes=0,calls=0,deny_at=0;
} allocations;
constexpr size_t dense_limit=1024*1024;
struct Watch {
  explicit Watch(size_t deny=0) { allocations={true,0,0,0,deny}; }
  ~Watch() { allocations.enabled=false; }
};
size_t checks=0;
void check(bool ok,const char *message) {
  if(!ok)throw std::runtime_error(message);
  ++checks;
}
template<class F> bool refuses(F f) {
  try { f(); } catch(const std::exception &) { return true; }
  return false;
}
dome::Mesh geometry(bool outside) {
  dome::Mesh m;
  m.positions={{0,0,0},{1,0,0},{1,1,0},{0,1,0}};
  m.normals.assign(4,{0,0,1});m.indices={0,1,2,0,2,3};
  m.uv=outside?std::vector<std::array<float,2>>{{-1,-1},{2,-1},{2,2},{-1,2}}:
               std::vector<std::array<float,2>>{{0,0},{1,0},{1,1},{0,1}};
  return m;
}
}
void *operator new(size_t n) {
  if(allocations.enabled) {
    ++allocations.calls;allocations.bytes+=n;
    allocations.largest=std::max(allocations.largest,n);
    if(n>=dense_limit || (allocations.deny_at && allocations.calls==allocations.deny_at))
      throw std::bad_alloc();
  }
  if(void *p=std::malloc(n?n:1))return p;
  throw std::bad_alloc();
}
void *operator new[](size_t n) { return ::operator new(n); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p,size_t) noexcept { std::free(p); }
void operator delete[](void *p,size_t) noexcept { std::free(p); }

int main() try {
  using namespace dome;
  auto image=std::make_shared<const Image>(Image{2,2,{255,0,0,255,0,255,0,255,
                                                     0,0,255,255,255,255,255,255}});
  auto maps=std::make_shared<PaintSourceMaps>();maps->color.image=image;
  PaintSurfaceOptions source;source.policy=PaintUvPolicy::SourceAuthored;
  const auto mesh=geometry(true);Material material;
  size_t total_bytes=0,largest=0;
  for(int wrap:{10497,33648,33071})for(uint32_t resolution:{64u,512u,2048u}) {
    maps->color.wrap_s=maps->color.wrap_t=wrap;
    std::shared_ptr<PaintedMesh> paint;
    {
      Watch watch;
      paint=PaintedMesh::from_maps(mesh,maps,resolution,source);
      total_bytes+=allocations.bytes;largest=std::max(largest,allocations.largest);
    }
    const auto baseline=paint->encode();
    {
      Watch watch;
      auto clone=paint->snapshot();
      check(clone->mesh().uv==mesh.uv && clone->resolution()==resolution,
            "Read-only capture retains original coordinates and requested resolution");
      auto marker=clone->composite(material);
      check(marker->source_only && marker->source_maps->color.image==image &&
            marker->source_maps->color.wrap_s==wrap,
            "Preview keeps the original image and sampler without dense grids");
      check(clone->pixel(clone->active_layer(),resolution-1,resolution-1)==PaintPixel{} &&
            clone->pbr_pixel(clone->active_layer(),0,0)==std::array<uint8_t,4>{},
            "Absent writable storage still exposes the existing empty pixel contract");
      auto same=clone->reparameterize(mesh);
      check(same->mesh().uv==mesh.uv && same->composite(material)->source_only,
            "Metadata-only source cloning does not allocate a raster");
      auto evaluated=clone->evaluate(std::make_shared<const Mesh>(mesh));
      check(bool(evaluated),"Evaluated source geometry does not need writable texels");
      check(refuses([&]{clone->begin_stroke();}) && !clone->in_stroke(),
            "Capture still refuses a paint gesture before changing state");
      check(clone->encode()==baseline,
            "Refused paint retains exact source, layer metadata and history");
      clone->add_layer("Unused source layer");
      check(clone->pixel(clone->active_layer(),resolution-1,resolution-1)==PaintPixel{},
            "Layer metadata retains addressable empty tile slots");
      auto encoded=clone->encode();auto reopened=PaintedMesh::decode(encoded);
      check(reopened->encode()==encoded && reopened->composite(material)->source_only,
            "Decode reconstructs the read-only state without a dense grid");
      auto forged=baseline;
      forged["layers"][0]["tiles"].push_back({{"index",0},
        {"data",nlohmann::json::binary(std::vector<uint8_t>(32*32*14))}});
      check(refuses([&]{(void)PaintedMesh::decode(forged);}),
            "Empty-looking persisted tiles cannot turn a capture into writable data");
      total_bytes+=allocations.bytes;largest=std::max(largest,allocations.largest);
    }
    check(paint->encode()==baseline,"All read-only operations preserve their original owner");
  }
  // Deny every actual allocation in high-resolution capture construction.
  // Each retry starts from the same source, never a partially installed raster.
  size_t calls=0;
  { Watch watch;auto p=PaintedMesh::from_maps(mesh,maps,2048,source);calls=allocations.calls; }
  for(size_t prefix=1;prefix<=calls;++prefix) {
    bool denied=false;
    { Watch watch(prefix);try { (void)PaintedMesh::from_maps(mesh,maps,2048,source); }
      catch(const std::bad_alloc &) { denied=true; } }
    check(denied,"Every actual capture allocation denial propagates without publication");
    auto retry=PaintedMesh::from_maps(mesh,maps,2048,source);
    check(retry->mesh().uv==mesh.uv && retry->preview(material)->source_only,
          "A complete retry retains genuine original source coordinates");
  }
  auto unit=geometry(false);PaintedMesh writable(unit,64);
  writable.begin_stroke();Brush brush;brush.from=brush.to={.5f,.5f,0};
  brush.normal={0,0,1};brush.radius=.3f;PaintSettings ink;ink.front_only=false;
  writable.stamp(brush,ink);writable.end_stroke();
  check(writable.composite(material)->color.width==64,
        "Ordinary writable surfaces still build and use their raster");
  bool dense=false;
  { Watch watch;try { PaintedMesh control(unit,2048); }
    catch(const std::bad_alloc &) { dense=allocations.largest>=dense_limit; } }
  check(dense,"Allocation oracle distinguishes a real high-resolution writable raster");
  std::cout<<"PASS "<<checks<<" source-capture storage checks; largest watched allocation="
           <<largest<<" cumulative watched bytes="<<total_bytes
           <<" construction OOM prefixes="<<calls<<'\n';
  return 0;
} catch(const std::exception &e) {
  allocations.enabled=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;
}
