// SPDX-License-Identifier: GPL-3.0-or-later
#include "prepared-normal-shapes-fixtures.hpp"
#include "normal-sampling-domain.hpp"
#include "dome/normal-sampling.hpp"
#include <iostream>
#include <numeric>

using namespace dome;
namespace {
unsigned checks = 0;
void check(bool result, const char *message) {
  if (!result) throw std::runtime_error(message);
  ++checks;
  std::cout << "PASS " << message << '\n';
}
template<class F> bool refuses(F operation) {
  try { operation(); } catch (const std::exception &) { return true; }
  return false;
}
// Independent clamped bilinear decoder. No field mapping, normal preparation
// or image-sampling implementation from the product is used by this oracle.
std::array<double,4> bilinear(const Image &image, double u, double v) {
  const double x = std::clamp(u,0.,1.)*image.width-.5;
  const double y = std::clamp(v,0.,1.)*image.height-.5;
  const auto ix = int(std::floor(x)), iy = int(std::floor(y));
  std::array<double,4> result{};
  for (int dy=0;dy<2;++dy) for (int dx=0;dx<2;++dx) {
    const size_t at=size_t(std::clamp(iy+dy,0,int(image.height)-1))*image.width+
        size_t(std::clamp(ix+dx,0,int(image.width)-1));
    const double weight=(dx?x-ix:1-x+ix)*(dy?y-iy:1-y+iy);
    for(size_t c=0;c<4;++c)result[c]+=weight*image.rgba[at*4+c]/255.;
  }
  return result;
}
}
int main() try {
  auto scene = prepared_shape_fixture::model(true);
  const auto &source = scene.active();
  const auto encoded = scene.encode();
  const auto &mesh = source.painted->mesh();
  std::vector<uint32_t> identity(mesh.positions.size());
  std::iota(identity.begin(),identity.end(),0);
  auto scaled = mesh;
  for(auto &uv:scaled.uv)for(auto &q:uv)q=.5f+(q-.5f)*.95f;
  auto field = detail::NormalSamplingDomain::capture(*source.painted,
      source.material,scaled,identity);
  check(field->coordinates==mesh.uv && field->intrinsic==source.painted->normal_projection(),
        "Fractional atlas edit retains exact intrinsic image owners and original sampling coordinates");
  const auto maps = source.painted->preview(source.material);
  check(field->resolved_pbr->rgba==maps->normal.rgba &&
        field->resolved_mtoon->rgba==maps->mtoon_normal.rgba &&
        field->original_bake==source.painted->baked(),
        "Captured resolved detail fields retain exact pre-filter quantization and owning bake");

  Mesh split;
  for(size_t c=0;c<mesh.indices.size();++c){
    const auto vertex=mesh.indices[c];
    split.positions.push_back(mesh.positions[vertex]);
    split.normals.push_back(mesh.normals[vertex]);
    split.indices.push_back(uint32_t(c));
    // Independent chart domains, deliberately unrelated to the normal field.
    split.uv.push_back(c<3 ? std::array<float,2>{.05f+.18f*float(c),.1f+.07f*float(c*c)}
        : std::array<float,2>{.95f-.15f*float(c-3),.9f-.09f*float((c-3)*(c-3))});
  }
  auto next=detail::NormalSamplingDomain::reparameterize(field,split,mesh.indices);
  check(next->before==field && next->depth==2 && next->coordinates.size()==6 &&
        next->resolved_pbr==field->resolved_pbr && next->intrinsic==field->intrinsic,
        "Independent split charts retain the exact field instead of rebuilding an image or averaging seam coordinates");
  size_t samples=0;
  for(size_t face=0;face<2;++face)for(int i=0;i<=8;++i)for(int j=0;j<=8-i;++j){
    const double weights[]={i/8.,j/8.,1-(i+j)/8.};
    std::array<double,2> wanted{},actual{};
    for(size_t k=0;k<3;++k)for(size_t q=0;q<2;++q){
      wanted[q]+=weights[k]*mesh.uv[mesh.indices[face*3+k]][q];
      actual[q]+=weights[k]*next->coordinates[split.indices[face*3+k]][q];
    }
    for(const auto &image:{field->intrinsic->pbr,field->intrinsic->mtoon,
                          field->resolved_pbr,field->resolved_mtoon}){
      const auto a=bilinear(*image,wanted[0],wanted[1]);
      const auto b=bilinear(*image,actual[0],actual[1]);
      for(size_t c=0;c<4;++c)if(a[c]!=b[c])
        throw std::runtime_error("Independent split-domain bilinear field differs");
      ++samples;
    }
  }
  check(samples==360,"Independent decoder proves exact intrinsic and resolved fields over both split charts");
  auto altered=split;altered.positions[0].z+=.01f;
  check(refuses([&]{detail::NormalSamplingDomain::reparameterize(field,altered,mesh.indices);}),
        "Sampling-domain authoring cannot silently become geometry/key authority");
  auto wrong=mesh.indices;std::swap(wrong[0],wrong[1]);
  check(refuses([&]{detail::NormalSamplingDomain::reparameterize(field,split,wrong);}),
        "Wrong ordered corner identity refuses without geometric nearest matching");
  detail::NormalSamplingLimits budget;budget.max_bytes=next->aggregate_bytes-1;
  check(refuses([&]{detail::NormalSamplingDomain::reparameterize(field,split,mesh.indices,budget);}),
        "Retained correspondence history is charged cumulatively before allocation");
  budget={};budget.max_depth=1;
  check(refuses([&]{detail::NormalSamplingDomain::reparameterize(field,split,mesh.indices,budget);}),
        "Repeated domain depth remains explicitly bounded");
  budget={};unsigned polls=0;budget.cancelled=[&]{return ++polls>1;};
  check(refuses([&]{detail::NormalSamplingDomain::reparameterize(field,split,mesh.indices,budget);})&&polls>1,
        "Detached correspondence cancellation publishes no partial domain");
  check(scene.encode()==encoded,"All source, detail, original artist and rig authorities remain unchanged");
  auto leaf=std::make_shared<dome::NormalSamplingDomain>();
  leaf->resolution=source.painted->resolution();leaf->coordinates=next->coordinates;
  for(const auto uv:leaf->coordinates)
    leaf->mtoon_coordinates.push_back({2*uv[0]-.25f,.5f*uv[1]+.75f});
  leaf->indices=next->rest->indices;
  leaf->resolved_pbr=next->resolved_pbr;leaf->resolved_mtoon=next->resolved_mtoon;
  auto detail=std::make_shared<NormalDetailAuthority>();
  const auto bake=source.painted->baked();
  detail->pbr=std::shared_ptr<const Image>(bake,&bake->maps.normal);
  detail->mtoon=std::shared_ptr<const Image>(bake,&bake->maps.mtoon_normal);
  detail->coverage=bake->coverage;leaf->detail=detail;
  NormalSamplingArchive pool;
  check(pool.add(leaf)==0 && pool.add(leaf)==0 && pool.size()==1,
        "Shared immutable normal-domain owners have one structural archive record");
  PaintImageArchive image_pool;
  const auto archive=pool.encode(image_pool);
  const auto decoded_images=PaintImageArchive::decode(image_pool.encode());
  auto decoded=NormalSamplingArchive::decode(archive,decoded_images);
  check(refuses([&]{decoded.require_reachable();}),
        "Structural decode cannot silently retain unreferenced normal domains");
  const auto &a=decoded.at(0), &b=decoded.at(0);
  decoded.require_reachable();
  check(a==b && a->coordinates==leaf->coordinates &&
        a->mtoon_coordinates==leaf->mtoon_coordinates && a->indices==leaf->indices &&
        a->resolved_pbr->rgba==leaf->resolved_pbr->rgba &&
        a->detail->coverage==leaf->detail->coverage && decoded.bytes()==leaf->storage_bytes(),
        "Geometry-only pool roundtrip preserves aliases, exact binary coordinates and normal detail metadata");
  check(a->detail==decoded.detail_at(0),
        "Leaf and baked-event references resolve the same durable detail identity");
  bool distinct_sampling=true;
  for(size_t face=0;face<2;++face)for(int i=0;i<=8;++i)for(int j=0;j<=8-i;++j){
    const double weights[]={i/8.,j/8.,1-(i+j)/8.};
    std::array<double,2> original{},actual{};
    for(size_t k=0;k<3;++k)for(size_t q=0;q<2;++q){
      original[q]+=weights[k]*mesh.uv[mesh.indices[face*3+k]][q];
      actual[q]+=weights[k]*a->mtoon_coordinates[a->indices[face*3+k]][q];
    }
    distinct_sampling &= bilinear(*a->resolved_mtoon,2*original[0]-.25,.5*original[1]+.75)==
        bilinear(*a->resolved_mtoon,actual[0],actual[1]);
  }
  check(distinct_sampling,"Distinct MToon affine coordinates retain outside-unit interpolation and clamp only at the independent image fetch");
  auto broken=archive;broken["version"]=3;
  check(refuses([&]{NormalSamplingArchive::decode(broken,decoded_images);}),
        "Unknown pool versions explicitly refuse");
  broken=archive;broken["domains"][0]["version"]=2;
  check(refuses([&]{NormalSamplingArchive::decode(broken,decoded_images);}),
        "Unknown leaf versions explicitly refuse");
  broken=archive;broken["details"][0]["interpretation"]="canonical-mikk";
  check(refuses([&]{NormalSamplingArchive::decode(broken,decoded_images);}),
        "Captured prepared detail cannot silently reinterpret historical canonical pixels");
  broken=archive;broken["domains"][0]["coordinates"].get_binary()[0]=255;
  broken["domains"][0]["coordinates"].get_binary()[1]=255;
  broken["domains"][0]["coordinates"].get_binary()[2]=255;
  broken["domains"][0]["coordinates"].get_binary()[3]=255;
  check(refuses([&]{NormalSamplingArchive::decode(broken,decoded_images);}),
        "Nonfinite binary normal coordinates refuse");
  broken=archive;broken["domains"][0]["indices"].get_binary()[0]=255;
  check(refuses([&]{NormalSamplingArchive::decode(broken,decoded_images);}),
        "Unavailable corner coordinates refuse");
  broken=archive;broken["domains"][0]["mtoon_coordinates"].get_binary().resize(8);
  check(refuses([&]{NormalSamplingArchive::decode(broken,decoded_images);}),
        "PBR and MToon coordinate roles require one exact common geometry domain");
  NormalSamplingArchiveLimits archive_limits;archive_limits.max_bytes=leaf->storage_bytes()-1;
  check(refuses([&]{NormalSamplingArchive::decode(archive,decoded_images,archive_limits);}),
        "Normal pool budget preflight runs before decoding coordinate arrays");
  archive_limits={};unsigned archive_polls=0;
  archive_limits.cancelled=[&]{return ++archive_polls>2;};
  check(refuses([&]{NormalSamplingArchive::decode(archive,decoded_images,archive_limits);})&&archive_polls>2,
        "Normal-domain binary reconstruction polls cancellation without granting authority");
  std::cout<<"PASS "<<checks<<" detached sampling-domain foundation checks\n";
  return 0;
} catch(const std::exception &error) {
  std::cerr<<"FAIL "<<error.what()<<'\n';
  return 1;
}
