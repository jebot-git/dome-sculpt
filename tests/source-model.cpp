// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/creator-archive.hpp"
#include "dome/import-image.hpp"
#include "import-authoring-fixtures.hpp"
#include <bit>
#include <cstdlib>
#include <iostream>
#include <new>
#include <numeric>
using namespace dome;
namespace {
thread_local bool armed=false,hit=false;
thread_local size_t calls=0,fail_at=SIZE_MAX;
size_t checks=0;
void check(bool b,const char*m){if(!b)throw std::runtime_error(m);++checks;}
template<class F>void refuses(F f,const char*m){bool failed=false;try{f();}catch(const std::exception&){failed=true;}check(failed,m);}
CreatorSourceModel model(import_fixture::Fixture &f) {
  CreatorSourceModel m;m.original_bytes=std::make_shared<const std::vector<uint8_t>>(f.bytes());
  m.asset=std::make_shared<const ImportedAsset>(import_glb(*m.original_bytes));
  auto images=std::make_shared<std::vector<std::shared_ptr<const Image>>>();
  for(const auto &encoded:m.asset->images)images->push_back(std::make_shared<const Image>(decode_import_image(encoded)));
  m.decoded_images=std::move(images);return m;
}
CreatorAppearance appearance(const CreatorSourceModel &m,const import_fixture::Fixture &f) {
  CreatorAppearance a;a.original_bytes=m.original_bytes;a.asset=m.asset;a.decoded_images=m.decoded_images;
  a.material=1;a.source_material=0;a.source_mesh=0;a.source_primitive=0;a.source_node=1;
  a.source_vertices.resize(m.asset->primitives[0].mesh.positions.size());std::iota(a.source_vertices.begin(),a.source_vertices.end(),0u);
  a.material_json=f.j.at("materials").at(0).dump();return a;
}
}
void *operator new(size_t n){if(armed&&++calls>=fail_at){hit=true;throw std::bad_alloc();}if(auto*p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void *operator new[](size_t n){return ::operator new(n);}
void operator delete(void*p)noexcept{std::free(p);}
void operator delete[](void*p)noexcept{std::free(p);}
void operator delete(void*p,size_t)noexcept{std::free(p);}
void operator delete[](void*p,size_t)noexcept{std::free(p);}
int main()try {
  import_fixture::Fixture f;
  f.j["nodes"][0]["scale"]={2,1,-3};
  f.j["nodes"][1]["rotation"]={0,0,std::sin(.37f),std::cos(.37f)};
  auto source=model(f);PaintImageArchive images;CreatorAssetArchive archive;
  check(archive.add_model(source,images)==0,"model-only hierarchy authority needs no primitive appearance");
  check(archive.size()==1 && images.size()==source.asset->images.size(),"model and decoded-image tables commit together");
  check(archive.model(0)==archive.model(0),"model owner is stable without repeated source/remap copies");
  check(archive.model(0)->asset==source.asset && archive.model(0)->original_bytes==source.original_bytes,
        "original model keeps exact immutable parsed/raw owners");
  auto a=appearance(source,f);const auto encoded_appearance=encode_creator_appearance(a,archive,images);
  check(archive.size()==1 && archive.add_model(source,images)==0,"appearance and hierarchy share one model record");
  const auto encoded=archive.encode(),encoded_images=images.encode();
  auto legacy_shape=nlohmann::json::array({{{"original_glb",nlohmann::json::binary(*source.original_bytes)},{"images",{0}}}});
  check(encoded==legacy_shape,"model-only retention keeps the existing source_models wire shape");
  auto decoded_images=PaintImageArchive::decode(encoded_images);
  auto decoded=CreatorAssetArchive::decode(encoded,decoded_images);
  auto restored=decoded.model(0);
  check(*restored->original_bytes==*source.original_bytes && restored->asset->nodes.size()==source.asset->nodes.size(),"raw source and original graph survive restart without any appearance");
  for(size_t n=0;n<source.asset->nodes.size();++n)
    check(std::bit_cast<std::array<uint32_t,16>>(restored->asset->nodes[n].local)==std::bit_cast<std::array<uint32_t,16>>(source.asset->nodes[n].local),"nonuniform/reflected/rotated source local bits survive restart");
  auto restored_a=decode_creator_appearance(encoded_appearance,decoded);
  check(restored_a->asset==restored->asset && restored_a->decoded_images==restored->decoded_images,
        "restarted appearance and hierarchy share parsed/image authority");
  check(decoded.encode()==encoded,"common model refactor preserves existing archive bytes/shape");
  refuses([&]{(void)archive.model(1);},"missing model reference refuses");
  auto mismatch=source;mismatch.asset=std::make_shared<const ImportedAsset>(*source.asset);
  refuses([&]{archive.add_model(mismatch,images);},"shared bytes cannot silently adopt a different parsed owner");
  auto bad=[&](auto edit) {
    auto copied=std::make_shared<ImportedAsset>(*source.asset);edit(*copied);
    auto invalid=source;invalid.asset=std::move(copied);
    CreatorAssetArchive empty;PaintImageArchive no_images;
    refuses([&]{empty.add_model(invalid,no_images);},"canonical source witness refuses altered node authority");
    check(empty.size()==0 && no_images.size()==0,"invalid model leaves both archives unchanged");
  };
  bad([](auto&s){s.nodes[1].local[12]+=.25f;});
  bad([](auto&s){s.nodes[1].parent=-1;});
  bad([](auto&s){s.nodes[0].children.clear();});
  bad([](auto&s){s.nodes[1].mesh=-1;});
  bad([](auto&s){s.nodes[1].has_rotation=false;});
  bad([](auto&s){s.nodes[1].weights.push_back(.11f);});
  // An untextured/no-material source is legal model authority even before the
  // separate Scene13 primitive-default-material adoption is enabled.
  auto no_material=f;no_material.j.erase("materials");no_material.j["meshes"][0]["primitives"][0].erase("material");
  auto plain=model(no_material);CreatorAssetArchive plain_archive;PaintImageArchive plain_images;
  check(plain_archive.add_model(plain,plain_images)==0,"model retention does not invent an authored default material");
  size_t observations=0;CreatorArchiveLimits limits;limits.cancelled=[&]{++observations;return false;};
  {CreatorAssetArchive measured(limits);PaintImageArchive m;measured.add_model(source,m);}
  const auto cancellations=observations;
  for(size_t at=1;at<=cancellations;++at) {
    observations=0;limits.cancelled=[&]{return ++observations>=at;};
    CreatorAssetArchive cancelled(limits);PaintImageArchive m;
    refuses([&]{cancelled.add_model(source,m);},"model cancellation refuses atomically");
    check(cancelled.size()==0 && m.size()==0,"cancelled model does not leak an archive/image publication");
  }
  calls=0;fail_at=SIZE_MAX;armed=true;
  {CreatorAssetArchive measured;PaintImageArchive m;measured.add_model(source,m);}
  armed=false;const auto allocations=calls;
  for(size_t at=1;at<=allocations;++at) {
    CreatorAssetArchive denied;PaintImageArchive m;calls=0;fail_at=at;hit=false;armed=true;bool caught=false;
    try{denied.add_model(source,m);}catch(const std::bad_alloc&){caught=true;}
    armed=false;check(hit&&caught,"every common-model allocation propagates persistent failure safely");
    check(denied.size()==0 && m.size()==0,"denied common model retains both prior archives");
    denied.add_model(source,m);check(denied.encode()==encoded && m.encode()==encoded_images,"retry after model allocation failure is exact");
  }
  calls=0;fail_at=SIZE_MAX;armed=true;
  {auto measured=CreatorAssetArchive::decode(encoded,decoded_images);}
  armed=false;const auto decode_allocations=calls;
  for(size_t at=1;at<=decode_allocations;++at) {
    calls=0;fail_at=at;hit=false;armed=true;bool caught=false;
    try{auto denied=CreatorAssetArchive::decode(encoded,decoded_images);}catch(const std::bad_alloc&){caught=true;}
    armed=false;check(hit&&caught,"every model restart allocation unwinds safely under persistent denial");
    check(decoded_images.encode()==encoded_images && source.asset==archive.model(0)->asset,
          "failed restart leaves input images and original authority unchanged");
    auto retry=CreatorAssetArchive::decode(encoded,decoded_images);
    check(retry.encode()==encoded,"retry after restart denial is exact");
  }
  std::cout<<checks<<" source-model assertions +"<<allocations<<" add/"<<decode_allocations
           <<" decode persistent allocation +"<<cancellations<<" add cancellation boundaries passed\n";return 0;
}catch(const std::exception&e){armed=false;std::cerr<<"FAIL "<<e.what()<<" calls="<<calls<<" site="<<fail_at<<'\n';return 1;}
