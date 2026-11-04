// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_CANONICAL_CONVERSION_FAULT_HELPERS_ONLY
#include "canonical-normal-conversion-faults.cpp"
#include "baked-normal-archive.hpp"
#include "dome/scene-file.hpp"
namespace {
void actual_field(const Scene &scene){
  const auto &o=scene.active();const auto c=o.material_bindings?o.material_bindings->canonical_attributes:nullptr;
  if(!c||!c->valid_owner(c)||o.painted->normal_projection()||!o.material_bindings->uv_origin||
      c->before_paint()!=o.material_bindings->uv_origin->paint||
      (o.painted->baked()&&(!o.painted->baked()->normal_field||o.painted->baked()->normal_field->origin()!=c)))
    throw std::runtime_error("Archive retry lost actual independent field/original owner authority");
}
struct CacheBudget {
  size_t old=paint_composite_cache_stats().limit_bytes;
  CacheBudget(){set_paint_composite_cache_budget(0);}
  ~CacheBudget(){set_paint_composite_cache_budget(old);}
};
}
int main(int argc,char **argv)try{
  if(argc<2||argc>3)return 2;const bool measure=argc==3&&std::string_view(argv[2])=="--measure";
  CacheBudget cache;
  const auto scene=Scene::load(argv[1]);const auto encoded=scene.encode();actual_field(scene);
  // Verify exceptional nested Scene scopes return the exact existing writer,
  // not a dangling reader granting permission to a later standalone leaf.
  PaintImageArchive outer_images;detail::BakedNormalArchive outer(outer_images);
  auto produce=[&]{return Scene::decode(encoded);};
  auto warm=produce();actual_field(warm);const auto expected=warm.encode();
  conversion_fault::calls=0;conversion_fault::at=SIZE_MAX;conversion_fault::enabled=true;
  auto positive=produce();conversion_fault::enabled=false;const auto count=conversion_fault::calls;
  require_cage(count>0&&count<200'000,"Measure actual complete Scene33 decoding C++ allocation path");
  actual_field(positive);require_cage(positive.encode()==expected&&&detail::BakedNormalArchive::writer()==&outer,
    "Positive owning archive replay preserves exact complete scene and nested codec context");
  std::cout<<"MEASURE persistent_cpp_prefixes="<<count<<std::endl;
  if(measure)return 0;
  for(size_t cut=1;cut<=count;++cut){
    conversion_fault::calls=0;conversion_fault::at=cut;conversion_fault::hit=false;bool caught=false;conversion_fault::enabled=true;
    try{produce();}catch(const std::bad_alloc&){caught=true;}
    conversion_fault::enabled=false;
    if(!caught||!conversion_fault::hit||&detail::BakedNormalArchive::writer()!=&outer||scene.encode()!=encoded)
      throw std::runtime_error("Persistent archive allocation failure missed its actual endpoint or changed original/context state");
    const auto retry=produce();actual_field(retry);
    if(retry.encode()!=expected||&detail::BakedNormalArchive::writer()!=&outer)
      throw std::runtime_error("Archive allocation retry differs from actual original owner/whole native result");
    if(cut%128==0)std::cout<<"PREFIX "<<cut<<"/"<<count<<std::endl;
  }
  size_t polls=0;Scene::load(argv[1],[&]{++polls;return false;});
  for(size_t cut=1;cut<=polls;++cut){size_t at=0;bool cancelled=false;
    try{Scene::load(argv[1],[&]{return ++at>=cut;});}catch(const std::exception &e){cancelled=std::string_view(e.what()).find("cancel")!=std::string_view::npos;}
    if(!cancelled||scene.encode()!=encoded||&detail::BakedNormalArchive::writer()!=&outer)
      throw std::runtime_error("Actual Scene file-load cancellation changed native/context state or failed its observed poll");
  }
  actual_field(Scene::load(argv[1]));
  std::cout<<"PASS scene33_persistent_cpp_prefixes="<<count<<" file_load_cancel_polls="<<polls<<std::endl;
  return 0;
}catch(const std::exception &e){conversion_fault::enabled=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
