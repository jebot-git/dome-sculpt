// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_CANONICAL_CONVERSION_FAULT_HELPERS_ONLY
#include "canonical-normal-conversion-faults.cpp"
#include "baked-normal-field.hpp"
int main()try{
  using detail::BakedNormalField;using detail::BakedNormalFieldOptions;
  auto scene=baked_fan(true,true,true);const auto exact=scene.encode();
  auto origin=detail::CanonicalNormalConversion::create({scene.active().painted,{},{},scene.active().deformation,scene.active().material},{});
  auto field=BakedNormalField::from_canonical(origin);auto incoming=*scene.active().painted->baked();
  incoming.normal_interpretation=BakedNormalInterpretation::PreparedAttributes;
  incoming.maps.normal=*field->pbr();incoming.maps.mtoon_normal=*field->mtoon();
  for(size_t i=0;i<incoming.coverage.size();++i){
    incoming.coverage[i]=std::array<uint8_t,4>{0,63,128,255}[i%4];
    incoming.maps.normal.rgba[i*4]=37;incoming.maps.mtoon_normal.rgba[i*4+1]=201;
  }
  auto signature=[](const auto &p){return nlohmann::json{{"pbr",p->pbr()->rgba},{"toon",p->mtoon()->rgba},{"coverage",p->normal_coverage()},{"replacements",p->replacements()},{"bytes",p->retained_bytes()}};};
  for(bool replacement:{false,true}){
    auto produce=[&]{return replacement?field->replace(incoming):BakedNormalField::from_canonical(origin);};
    const auto warm=produce();const auto expected=signature(warm);
    conversion_fault::calls=0;conversion_fault::at=SIZE_MAX;conversion_fault::enabled=true;
    auto measured=produce();conversion_fault::enabled=false;const auto count=conversion_fault::calls;
    require_cage(count>0&&count<4096,"Measure real independent field C++ allocation prefixes");
    for(size_t cut=1;cut<=count;++cut){
      conversion_fault::calls=0;conversion_fault::at=cut;conversion_fault::hit=false;bool caught=false;conversion_fault::enabled=true;
      try{produce();}catch(const std::bad_alloc&){caught=true;}conversion_fault::enabled=false;
      if(!caught||!conversion_fault::hit||scene.encode()!=exact||field->replacements()!=0||field->origin()!=origin)
        throw std::runtime_error("Persistent independent field allocation failure lost exact source/current-field state");
      if(signature(produce())!=expected)throw std::runtime_error("Independent field allocation retry differs from measured result");
    }
    size_t polls=0;BakedNormalFieldOptions o;o.cancelled=[&]{++polls;return false;};
    auto with_options=[&]{return replacement?field->replace(incoming,o):BakedNormalField::from_canonical(origin,o);};with_options();
    for(size_t cut=1;cut<=polls;++cut){size_t at=0;o.cancelled=[&]{return ++at>=cut;};
      if(!failure(with_options,"cancelled")||scene.encode()!=exact||field->replacements()!=0)
        throw std::runtime_error("Independent field cancellation changed actual input or current field");
    }
    require_cage(signature(produce())==expected,"Every persistent allocation and cancellation endpoint retries to identical owning normal planes");
    std::cout<<"PASS "<<(replacement?"replace":"initial")<<" persistent_cpp_prefixes="<<count<<" all_cancel_polls="<<polls<<'\n';
  }
  return 0;
}catch(const std::exception &e){conversion_fault::enabled=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
