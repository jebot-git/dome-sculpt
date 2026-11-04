// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_CANONICAL_CONVERSION_FAULT_HELPERS_ONLY
#include "canonical-normal-conversion-faults.cpp"
#include "baked-normal-archive.hpp"
#include "dome/scene-file.hpp"
#include "native-paint-sampling.hpp"
namespace {
// Require actual simultaneously owned roots. The active object may be native
// paint or converted normals; selection is not an archive capability.
void actual_fields(const Scene &scene){
  bool normals=false,native=false;
  for(const auto &o:scene.objects()){
    const auto b=o.material_bindings;if(!b)continue;
    if(const auto c=b->canonical_attributes){
      if(!c->valid_owner(c)||!o.painted||o.painted->normal_projection()||!b->uv_origin||
         c->before_paint()!=b->uv_origin->paint||
         (o.painted->baked()&&(!o.painted->baked()->normal_field||o.painted->baked()->normal_field->origin()!=c)))
        throw std::runtime_error("Mixed archive lost actual normal field/original attribute authority");
      if(b->canonical_uv){const auto &uv=b->canonical_uv;
        if(!uv->valid_owner(uv)||uv->origin()!=c)throw std::runtime_error("Mixed archive lost authentic canonical UV/root sharing");
        uv->validate_current(o.painted->mesh());
      }
      normals=true;
    }
    if(const auto p=b->native_paint_sampling){
      if(!o.painted||!p->valid_owner(p)||!p->matches_current(*o.painted))
        throw std::runtime_error("Mixed archive lost actual native paint/domain ownership");
      p->validate_current(*o.painted);native=true;
    }
  }
  if(!normals||!native)throw std::runtime_error("Mixed archive fixture lacks genuine simultaneous normal and native paint roots");
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
  const auto scene=Scene::load(argv[1]);const auto encoded=scene.encode();actual_fields(scene);
  // Verify exceptional nested Scene scopes return the exact existing writer,
  // not a dangling reader granting permission to a later standalone leaf.
  PaintImageArchive outer_images;detail::BakedNormalArchive outer(outer_images);
  auto exercise=[&](std::string_view mode,auto produce,auto signature){
    auto warm=produce();const auto expected=signature(warm);
    conversion_fault::calls=0;conversion_fault::at=SIZE_MAX;conversion_fault::enabled=true;
    auto positive=produce();conversion_fault::enabled=false;const auto count=conversion_fault::calls;
    require_cage(count>0&&count<200'000,"Measure actual complete owning Scene C++ allocation path");
    require_cage(signature(positive)==expected&& &detail::BakedNormalArchive::writer()==&outer,
      "Positive owning archive operation preserves exact complete scene and nested codec context");
    std::cout<<"MEASURE mode="<<mode<<" persistent_cpp_prefixes="<<count<<std::endl;
    if(!measure)for(size_t cut=1;cut<=count;++cut){
      conversion_fault::calls=0;conversion_fault::at=cut;conversion_fault::hit=false;bool caught=false;conversion_fault::enabled=true;
      try{produce();}catch(const std::bad_alloc&){caught=true;}
      conversion_fault::enabled=false;
      if(!caught||!conversion_fault::hit||&detail::BakedNormalArchive::writer()!=&outer||scene.encode()!=encoded)
        throw std::runtime_error("Persistent archive allocation failure missed its actual endpoint or changed original/context state");
      const auto retry=produce();
      if(signature(retry)!=expected||&detail::BakedNormalArchive::writer()!=&outer)
        throw std::runtime_error("Archive allocation retry differs from actual original owner/whole native result");
      if(cut%128==0)std::cout<<"PREFIX "<<mode<<" "<<cut<<"/"<<count<<std::endl;
    }
    return count;
  };
  const auto decode_count=exercise("decode",[&]{return Scene::decode(encoded);},[&](const Scene &value){actual_fields(value);return value.encode();});
  const auto encode_count=exercise("encode",[&]{return scene.encode();},[&](const nlohmann::json &value){return value;});
  if(measure)return 0;
  size_t polls=0;Scene::load(argv[1],[&]{++polls;return false;});
  for(size_t cut=1;cut<=polls;++cut){size_t at=0;bool cancelled=false;
    try{Scene::load(argv[1],[&]{return ++at>=cut;});}catch(const std::exception &e){cancelled=std::string_view(e.what()).find("cancel")!=std::string_view::npos;}
    if(!cancelled||scene.encode()!=encoded||&detail::BakedNormalArchive::writer()!=&outer)
      throw std::runtime_error("Actual Scene file-load cancellation changed native/context state or failed its observed poll");
  }
  actual_fields(Scene::load(argv[1]));
  std::cout<<"PASS mixed_owning_scene_persistent_cpp_prefixes="<<decode_count<<" encode_prefixes="<<encode_count<<" file_load_cancel_polls="<<polls<<std::endl;
  return 0;
}catch(const std::exception &e){conversion_fault::enabled=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
