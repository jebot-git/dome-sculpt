// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_BAKED_NORMAL_EXPORT_HELPERS_ONLY
#include "baked-normal-export.cpp"
#include "baked-normal-field.hpp"
#include <type_traits>
static_assert(!std::is_default_constructible_v<dome::detail::BakedNormalField>);
static_assert(!std::is_copy_constructible_v<dome::detail::BakedNormalField>);
namespace {
bool refuses(const auto &call,std::string_view message){try{call();return false;}catch(const std::exception &e){return std::string_view(e.what()).find(message)!=std::string_view::npos;}}
Image oracle(const Image &before,const Image &after,const std::vector<uint8_t> &coverage){
  Image result=before;
  for(size_t i=0;i<coverage.size();++i){
    if(!coverage[i])continue;
    std::array<double,3> n{};double norm=0;const double a=coverage[i]/255.;
    for(size_t c=0;c<3;++c){n[c]=(before.rgba[i*4+c]/255.*2-1)*(1-a)+(after.rgba[i*4+c]/255.*2-1)*a;norm+=n[c]*n[c];}
    if(norm<=1e-16)n={0,0,1};else for(auto &x:n)x/=std::sqrt(norm);
    for(size_t c=0;c<3;++c)result.rgba[i*4+c]=uint8_t(std::lround(std::clamp((n[c]+1)*127.5,0.,255.)));
    result.rgba[i*4+3]=255;
  }
  return result;
}
int max_error(const Image &a,const Image &b){int out=0;for(size_t i=0;i<a.rgba.size();++i)out=std::max(out,std::abs(int(a.rgba[i])-int(b.rgba[i])));return out;}
}
int main()try{
  using detail::BakedNormalField;using detail::BakedNormalFieldOptions;
  for(int variant=0;variant<8;++variant){
    auto scene=baked_fan(variant&1,variant&2,variant&4);const auto exact=scene.encode();
    const auto input=scene.active().painted;
    auto origin=detail::CanonicalNormalConversion::create({input,scene.active().appearance,scene.active().material_bindings,scene.active().deformation,scene.active().material},{});
    auto field=BakedNormalField::from_canonical(origin);
    require_cage(!input->normal_projection()&&!origin->before_paint()->normal_projection()&&field->pbr()==origin->projection()->pbr,
      "Converted baked role does not fabricate an intrinsic projection or change the genuine input");
    require_cage(std::all_of(field->normal_coverage().begin(),field->normal_coverage().end(),[](auto a){return a==255;})&&
      std::any_of(input->baked()->coverage.begin(),input->baked()->coverage.end(),[](auto a){return a!=255;}),
      "Resolved normal coverage is independent of partial material coverage and missed atlas regions");
    auto incoming=*input->baked();incoming.normal_interpretation=BakedNormalInterpretation::PreparedAttributes;
    incoming.maps.normal=*field->pbr();incoming.maps.mtoon_normal=*field->mtoon();
    for(size_t i=0;i<incoming.coverage.size();++i){
      incoming.coverage[i]=std::array<uint8_t,4>{0,63,128,255}[i%4];
      for(size_t c=0;c<3;++c){incoming.maps.normal.rgba[i*4+c]=uint8_t(23+(i*17+c*71)%210);incoming.maps.mtoon_normal.rgba[i*4+c]=uint8_t(31+(i*13+c*59)%190);}
    }
    const auto expected_pbr=oracle(*field->pbr(),incoming.maps.normal,incoming.coverage);
    const auto expected_toon=oracle(*field->mtoon(),incoming.maps.mtoon_normal,incoming.coverage);
    const auto next=field->replace(incoming);
    require_cage(max_error(*next->pbr(),expected_pbr)<=1&&max_error(*next->mtoon(),expected_toon)<=1,
      "Independent double-precision oracle verifies both roles normalize and quantize each texel before filtering");
    bool missed=true;
    for(size_t i=0;i<incoming.coverage.size();++i)if(!incoming.coverage[i])for(size_t c=0;c<4;++c)
      missed&=next->pbr()->rgba[i*4+c]==field->pbr()->rgba[i*4+c]&&next->mtoon()->rgba[i*4+c]==field->mtoon()->rgba[i*4+c];
    require_cage(missed&&next->origin()==origin&&next->prior()==field&&next->replacements()==1,
      "Partial normal replacement preserves missed texels byte-exactly and retains its actual prior field and origin");
    incoming.maps.normal.rgba.assign(incoming.maps.normal.rgba.size(),0);
    incoming.maps.mtoon_normal.rgba.assign(incoming.maps.mtoon_normal.rgba.size(),255);
    require_cage(max_error(*next->pbr(),expected_pbr)<=1&&max_error(*next->mtoon(),expected_toon)<=1,
      "Caller mutation cannot alter detached replacement planes or the previous immutable field");
    incoming.coverage.assign(incoming.coverage.size(),0);
    require_cage(next->replace(incoming)==next,"All-miss replacement is an exact same-owner no-op");
    incoming.coverage.assign(incoming.coverage.size(),128);
    auto again=next->replace(incoming);
    require_cage(max_error(*again->pbr(),oracle(*next->pbr(),incoming.maps.normal,incoming.coverage))<=1&&again->replacements()==2,
      "Repeated partial baking combines with the real previous resolved normal, not material alpha or neutral fallback");
    BakedNormalFieldOptions limited;limited.max_replacements=1;
    require_cage(refuses([&]{next->replace(incoming,limited);},"history"),"Replacement history is bounded before detached copies");
    limited={};limited.max_working_bytes=1;
    require_cage(refuses([&]{field->replace(incoming,limited);},"budget")&&refuses([&]{BakedNormalField::from_canonical(origin,limited);},"budget"),
      "Initial and replacement retained/working memory limits refuse atomically");
    incoming.normal_interpretation=BakedNormalInterpretation::CanonicalMikk;incoming.maps.mtoon_normal={};
    require_cage(refuses([&]{field->replace(incoming);},"prepared"),"Raw canonical pixels cannot masquerade as prepared replacement detail");
    auto intrinsic=origin->before_paint()->snapshot();intrinsic->clear_bake();
    field.reset();
    require_cage(!intrinsic->normal_projection()&&!intrinsic->baked()&&origin->before_paint()->baked()&&scene.encode()==exact,
      "Baked field can be released while durable original provenance and genuine intrinsic absence remain intact");
    std::cout<<"VARIANT "<<variant<<" pbr_error="<<max_error(*next->pbr(),expected_pbr)<<" toon_error="<<max_error(*next->mtoon(),expected_toon)<<'\n';
  }
  auto scene=baked_fan(true,true,true);const auto exact=scene.encode();
  auto origin=detail::CanonicalNormalConversion::create({scene.active().painted,{},{},scene.active().deformation,scene.active().material},{});
  auto field=BakedNormalField::from_canonical(origin);auto incoming=*scene.active().painted->baked();
  incoming.normal_interpretation=BakedNormalInterpretation::PreparedAttributes;
  incoming.maps.normal=*field->pbr();incoming.maps.mtoon_normal=*field->mtoon();
  BakedNormalFieldOptions o;size_t polls=0;o.cancelled=[&]{++polls;return false;};const auto positive=field->replace(incoming,o);
  for(size_t cut=1;cut<=polls;++cut){size_t at=0;o.cancelled=[&]{return ++at>=cut;};
    require_cage(refuses([&]{field->replace(incoming,o);},"cancelled")&&scene.encode()==exact&&field->replacements()==0,
      "Every replacement cancellation poll leaves complete source and prior field exact");
  }
  require_cage(field->replace(incoming)->pbr()->rgba==positive->pbr()->rgba,"Cancelled replacement retries to exact resolved pixels");
  std::cout<<"PASS "<<cage_checks<<" independent layered baked-normal checks; all_cancel_polls="<<polls<<'\n';return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
