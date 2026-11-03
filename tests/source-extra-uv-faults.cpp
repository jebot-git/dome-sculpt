// SPDX-License-Identifier: GPL-3.0-or-later
#include "import-authoring-fixtures.hpp"
#include "dome/creator-archive.hpp"
#include "dome/import-image.hpp"
#include <cstdlib>
#include <new>
#include <iostream>
namespace allocation {thread_local bool armed=false,hit=false;thread_local size_t calls=0,cut=0;}
void *operator new(size_t n){if(allocation::armed&&++allocation::calls>=allocation::cut&&allocation::cut){allocation::hit=true;throw std::bad_alloc();}if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void *operator new[](size_t n){return ::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}
int main()try {
 using namespace dome;import_fixture::Fixture f;while(f.bin.size()%4)f.bin.push_back(0);const auto uv=f.values({0,0,0,1,1,1,1,0},2,"VEC2");f.j["meshes"][0]["primitives"][0]["attributes"]["TEXCOORD_2"]=uv;
 CreatorSourceModel source;source.original_bytes=std::make_shared<const std::vector<uint8_t>>(f.bytes());ImportLimits limits;limits.mode=ImportMode::PreserveGltf;source.asset=std::make_shared<const ImportedAsset>(import_glb(*source.original_bytes,limits,ImportMode::PreserveGltf));auto decoded=std::make_shared<std::vector<std::shared_ptr<const Image>>>();for(const auto &image:source.asset->images)decoded->push_back(std::make_shared<const Image>(decode_import_image(image)));source.decoded_images=decoded;
 PaintImageArchive images;CreatorAssetArchive baseline;baseline.add_model(source,images);const auto expected=baseline.encode(),expected_images=images.encode();const auto raw=*source.original_bytes;size_t reached=0;
 for(int operation=0;operation<2;++operation){
  allocation::cut=allocation::calls=0;allocation::armed=true;{if(operation==0){CreatorAssetArchive a;PaintImageArchive i;a.add_model(source,i);}else{auto a=CreatorAssetArchive::decode(expected,images);}}allocation::armed=false;const auto measured=allocation::calls;
  if(measured<30||measured>20000)throw std::runtime_error("Source archive fault sweep has unexpected allocation coverage");size_t failures=0,completed=0;
  for(size_t cut=1;cut<=measured;++cut){CreatorAssetArchive target;PaintImageArchive target_images;bool failed=false;allocation::calls=0;allocation::cut=cut;allocation::hit=false;allocation::armed=true;
   try{if(operation==0)target.add_model(source,target_images);else target=CreatorAssetArchive::decode(expected,images);}catch(const std::bad_alloc&){failed=true;}allocation::armed=false;
   if(!allocation::hit)continue;++reached;
   if(failed){++failures;if(target.size()!=0 || target_images.size()!=0)throw std::runtime_error("Denied source archive published partial source/image entries");}else{++completed;if(target.encode()!=expected)throw std::runtime_error("Successful source archive denial changed the canonical output");}
   if(*source.original_bytes!=raw || images.encode()!=expected_images)throw std::runtime_error("Source archive denial changed original source or image authority");
   if(failed){if(operation==0)target.add_model(source,target_images);else target=CreatorAssetArchive::decode(expected,images);}
   if(target.encode()!=expected)throw std::runtime_error("Source archive cannot retry exactly after persistent denial");
  }
  std::cout<<"PASS extra-source archive operation="<<operation<<" measured="<<measured<<" denied="<<failures<<" completed="<<completed<<'\n';
 }
 std::cout<<"PASS "<<reached<<" reached persistent extra-source archive allocation boundaries\n";return 0;
}catch(const std::exception&e){allocation::armed=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
